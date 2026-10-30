// Copyright (c) 2006-2007, Rodrigo Braz Monteiro, Evgeniy Stepanov
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file subtitles_provider_libass.cpp
/// @brief libass-based subtitle renderer
/// @ingroup subtitle_rendering
///

#include "subtitles_provider_libass.h"

#include "i18n.h"
#include "include/aegisub/subtitles_provider.h"
#include "ui_hooks.h"
#include "video_frame.h"

#include <libaegisub/background_runner.h>
#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/log.h>
#include <libaegisub/util.h>

#include <atomic>
#include <boost/gil.hpp>
#include <memory>
#include <mutex>

#ifdef __ANDROID__
#include "font_index.h"

#include <libaegisub/file_mapping.h>

#include <set>
#include <string_view>
#endif

extern "C" {
#include <ass/ass.h>
}

namespace {
std::unique_ptr<agi::dispatch::Queue> cache_queue;
ASS_Library *library;

void msg_callback(int level, const char *fmt, va_list args, void *) {
	if (level >= 7) return;
	char buf[1024];
#ifdef _WIN32
	vsprintf_s(buf, sizeof(buf), fmt, args);
#else
	vsnprintf(buf, sizeof(buf), fmt, args);
#endif

	if (level < 2) // warning/error
		LOG_I("subtitle/provider/libass") << buf;
	else // verbose
		LOG_D("subtitle/provider/libass") << buf;
}

#ifdef __ANDROID__
// Android has no font provider which libass can use, so the fonts a script
// asks for are found in the font folders and given to libass as memory fonts
std::mutex android_fonts_mutex;
/// The font files given to libass so far
std::set<agi::fs::path> android_font_files;

std::string android_default_font() {
	auto match = fonts::Find("Arial", false, false);
	return match ? match->face.path.string() : "";
}

/// The font names of a track's styles and \fn tags
std::set<std::string> android_track_fonts(ASS_Track *track) {
	std::set<std::string> names;
	for (int i = 0; i < track->n_styles; ++i) {
		if (track->styles[i].FontName)
			names.insert(track->styles[i].FontName);
	}
	for (int i = 0; i < track->n_events; ++i) {
		if (!track->events[i].Text) continue;
		std::string_view text(track->events[i].Text);
		for (size_t pos = text.find("\\fn"); pos != text.npos; pos = text.find("\\fn", pos + 3)) {
			// The tags are only inside override blocks
			auto open = text.rfind('{', pos);
			auto close = text.rfind('}', pos);
			if (open == text.npos || (close != text.npos && close > open)) continue;

			auto start = text.find_first_not_of(' ', pos + 3);
			if (start == text.npos) break;
			auto end = text.find_first_of("\\}", start);
			auto name = text.substr(start, end == text.npos ? text.npos : end - start);
			while (!name.empty() && name.back() == ' ') name.remove_suffix(1);
			if (!name.empty()) names.insert(std::string(name));
		}
	}
	return names;
}

/// Give libass the faces of the fonts a track uses, for each of bold and
/// italic since the lines can switch with tags
/// @return Whether a font was added
bool android_add_fonts(ASS_Track *track) {
	std::lock_guard<std::mutex> lock(android_fonts_mutex);
	bool added = false;
	for (auto const& name : android_track_fonts(track)) {
		for (bool bold : {false, true}) {
			for (bool italic : {false, true}) {
				auto match = fonts::Find(name, bold, italic);
				if (!match || !android_font_files.insert(match->face.path).second)
					continue;
				try {
					agi::read_file_mapping file(match->face.path);
					auto size = file.size();
					ass_add_font(library, match->face.path.filename().string().c_str(), file.read(), static_cast<int>(size));
					added = true;
				}
				catch (agi::Exception const& e) {
					LOG_E("subtitle/provider/libass") << "Could not read " << match->face.path << ": " << e.GetMessage();
				}
			}
		}
	}
	return added;
}
#endif

void set_fonts(ASS_Renderer *renderer) {
#ifdef __ANDROID__
	// The default font is used for the fonts which weren't found
	auto default_font = android_default_font();
	ass_set_fonts(renderer, default_font.empty() ? nullptr : default_font.c_str(), "Sans", 1, nullptr, true);
#else
	ass_set_fonts(renderer, nullptr, "Sans", 1, nullptr, true);
#endif
}

// Stuff used on the cache thread, owned by a shared_ptr in case the provider
// gets deleted before the cache finishing updating
struct cache_thread_shared {
	ASS_Renderer *renderer = nullptr;
	std::atomic<bool> ready{false};
	~cache_thread_shared() { if (renderer) ass_renderer_done(renderer); }
};

class LibassSubtitlesProvider final : public SubtitlesProvider {
	agi::BackgroundRunner *br;
	std::shared_ptr<cache_thread_shared> shared;
	ASS_Track* ass_track = nullptr;
#ifdef __ANDROID__
	/// Whether the fonts of the track have been given to libass
	bool fonts_added = false;
	/// Whether the renderer has drawn a frame, and so remembers which fonts
	/// it couldn't find
	bool rendered = false;
#endif

	ASS_Renderer *renderer() {
		if (shared->ready)
			return shared->renderer;

		auto block = [&] {
			if (shared->ready)
				return;
			agi::util::sleep_for(250);
			if (shared->ready)
				return;
			br->Run([this](agi::ProgressSink *ps) {
				ps->SetTitle(from_ui(_("Updating font index")));
				ps->SetMessage(from_ui(_("This may take several minutes")));
				ps->SetIndeterminate();
				while (!shared->ready && !ps->IsCancelled())
					agi::util::sleep_for(250);
			});
		};

		if (ui::IsMainThread())
			block();
		else
			agi::dispatch::Main().Sync(block);
		return shared->renderer;
	}

public:
	LibassSubtitlesProvider(agi::BackgroundRunner *br);
	~LibassSubtitlesProvider();

	void LoadSubtitles(const char *data, size_t len) override {
		if (ass_track) ass_free_track(ass_track);
		ass_track = ass_read_memory(library, const_cast<char *>(data), len, nullptr);
		if (!ass_track) throw agi::InternalError("libass failed to load subtitles.");
#ifdef __ANDROID__
		fonts_added = false;
#endif
	}

	void DrawSubtitles(VideoFrame &dst, double time) override;

	void Reinitialize() override {
		// No need to reinit if we're not even done with the initial init
		if (!shared->ready)
			return;

		ass_renderer_done(shared->renderer);
		shared->renderer = ass_renderer_init(library);
		ass_set_font_scale(shared->renderer, 1.);
		set_fonts(shared->renderer);
#ifdef __ANDROID__
		rendered = false;
#endif
	}
};

LibassSubtitlesProvider::LibassSubtitlesProvider(agi::BackgroundRunner *br)
: br(br)
, shared(std::make_shared<cache_thread_shared>())
{
	auto state = shared;
	cache_queue->Async([state] {
		auto ass_renderer = ass_renderer_init(library);
		if (ass_renderer) {
			ass_set_font_scale(ass_renderer, 1.);
			set_fonts(ass_renderer);
		}
		state->renderer = ass_renderer;
		state->ready = true;
	});
}

LibassSubtitlesProvider::~LibassSubtitlesProvider() {
	if (ass_track) ass_free_track(ass_track);
}

#define _r(c) ((c)>>24)
#define _g(c) (((c)>>16)&0xFF)
#define _b(c) (((c)>>8)&0xFF)
#define _a(c) ((c)&0xFF)

void LibassSubtitlesProvider::DrawSubtitles(VideoFrame &frame,double time) {
#ifdef __ANDROID__
	if (!fonts_added) {
		// Wait for the renderer before giving libass fonts, then start a
		// new renderer if this one has already missed some of them
		renderer();
		fonts_added = true;
		if (android_add_fonts(ass_track) && rendered)
			Reinitialize();
	}
	rendered = true;
#endif
	ass_set_frame_size(renderer(), frame.width, frame.height);
	// Note: this relies on Aegisub always rendering at video storage res
	ass_set_storage_size(renderer(), frame.width, frame.height);

	// Add 1e-6 to guard against floating point imprecision errors on int -> float -> *1000 -> int round trips
	ASS_Image* img = ass_render_frame(renderer(), ass_track, floor(time * 1000 + 1e-6), nullptr);

	// libass actually returns several alpha-masked monochrome images.
	// Here, we loop through their linked list, get the colour of the current, and blend into the frame.
	// This is repeated for all of them.

	using namespace boost::gil;
	auto dst = interleaved_view(frame.width, frame.height, (bgra8_pixel_t*)frame.data.data(), frame.width * 4);
	if (frame.flipped)
		dst = flipped_up_down_view(dst);

	for (; img; img = img->next) {
		unsigned int opacity = 255 - ((unsigned int)_a(img->color));
		unsigned int r = (unsigned int)_r(img->color);
		unsigned int g = (unsigned int)_g(img->color);
		unsigned int b = (unsigned int)_b(img->color);

		auto srcview = interleaved_view(img->w, img->h, (gray8_pixel_t*)img->bitmap, img->stride);
		auto dstview = subimage_view(dst, img->dst_x, img->dst_y, img->w, img->h);

		transform_pixels(dstview, srcview, dstview, [=](const bgra8_pixel_t frame, const gray8_pixel_t src) -> bgra8_pixel_t {
			unsigned int k = ((unsigned)src) * opacity / 255;
			unsigned int ck = 255 - k;

			bgra8_pixel_t ret;
			ret[0] = (k * b + ck * frame[0]) / 255;
			ret[1] = (k * g + ck * frame[1]) / 255;
			ret[2] = (k * r + ck * frame[2]) / 255;
			ret[3] = 0;
			return ret;
		});
	}
}
}

namespace libass {
std::unique_ptr<SubtitlesProvider> Create(std::string const&, agi::BackgroundRunner *br) {
	return std::make_unique<LibassSubtitlesProvider>(br);
}

void CacheFonts() {
	// Initialize the cache worker thread
	cache_queue = agi::dispatch::Create();

	// Initialize libass
	library = ass_library_init();
	ass_set_message_cb(library, msg_callback, nullptr);

	// Initialize a renderer to force fontconfig to update its cache
	cache_queue->Async([] {
		auto ass_renderer = ass_renderer_init(library);
		set_fonts(ass_renderer);
		ass_renderer_done(ass_renderer);
	});
}
}
