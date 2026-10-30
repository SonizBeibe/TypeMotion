// Copyright (c) 2026, TypeMotion
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

/// @file media.cpp
/// @brief The video and audio of the app
/// @ingroup android

#include "media.h"

#include "bridge.h"
#include "json_io.h"

#include "../async_video_provider.h"
#include "../audio_controller.h"
#include "../include/aegisub/context.h"
#include "../project.h"
#include "../video_controller.h"
#include "../video_frame.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/signal.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <vector>

namespace {
	std::mutex frame_mutex;
	std::shared_ptr<VideoFrame> newest_frame;
	int64_t frame_serial = 0;

	std::vector<agi::signal::Connection> connections;
	std::chrono::steady_clock::time_point last_position_event;
	/// Counts up when the app connects or disconnects, so that an event
	/// queued for an older context is dropped
	int connection_generation = 0;
	bool video_event_queued = false;
	bool audio_event_queued = false;

	json::UnknownElement video_state(agi::Context *c) {
		auto provider = c->project->VideoProvider();
		if (!provider) return json::Null();

		auto vc = c->videoController.get();
		auto [display_width, display_height] = provider->GetDisplayResolution();
		int frame = vc->GetFrameN();

		json::Array keyframes;
		for (int keyframe : c->project->Keyframes())
			keyframes.push_back(static_cast<int64_t>(keyframe));

		return json_io::Obj(
			"path", c->project->VideoName().string(),
			"width", provider->GetWidth(),
			"height", provider->GetHeight(),
			"display_width", display_width,
			"display_height", display_height,
			"frames", provider->GetFrameCount(),
			"fps", provider->GetFPS().FPS(),
			"frame", frame,
			"time", vc->TimeAtFrame(frame),
			"playing", vc->IsPlaying(),
			"keyframes", std::move(keyframes)
		);
	}

	json::UnknownElement audio_state(agi::Context *c) {
		auto provider = c->project->AudioProvider();
		if (!provider) return json::Null();
		return json_io::Obj(
			"path", c->project->AudioName().string(),
			"duration", static_cast<int64_t>(provider->GetNumSamples() * 1000 / std::max(1, provider->GetSampleRate())),
			"sample_rate", provider->GetSampleRate(),
			"channels", provider->GetChannels()
		);
	}

	json::Object seek_event(agi::Context *c, int frame) {
		auto vc = c->videoController.get();
		return json_io::Obj(
			"frame", frame,
			"time", vc->TimeAtFrame(frame),
			"playing", vc->IsPlaying()
		);
	}

	/// Send the video or audio state once the engine is done with the
	/// current call: the project announces a new video before it sets the
	/// video's path, keyframes and timecodes, and it announces it again
	/// when one of those changes
	void queue_state_event(agi::Context *c, bool& queued, const char *name) {
		if (queued) return;
		queued = true;
		int generation = connection_generation;
		agi::dispatch::Main().Async([c, &queued, name, generation] {
			if (generation != connection_generation) return;
			queued = false;
			bridge::Event(name, android::media::State(c, {}));
		});
	}
}

namespace android::media {
	void FrameReady(std::shared_ptr<VideoFrame> frame) {
		FrameSize size;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			newest_frame = std::move(frame);
			size.serial = ++frame_serial;
			size.width = newest_frame->width;
			size.height = newest_frame->height;
		}
		bridge::Event("video_frame", json_io::Obj(
			"serial", size.serial,
			"width", size.width,
			"height", size.height
		));
	}

	void ClearFrame() {
		std::lock_guard<std::mutex> lock(frame_mutex);
		newest_frame.reset();
		++frame_serial;
	}

	FrameSize CopyFrame(void *dst, size_t capacity, int max_width, int max_height) {
		std::shared_ptr<VideoFrame> frame;
		FrameSize size;
		{
			std::lock_guard<std::mutex> lock(frame_mutex);
			frame = newest_frame;
			size.serial = frame_serial;
		}
		if (!frame || frame->width <= 0 || frame->height <= 0) return {};

		// Skip pixels evenly to fit, which is what a phone screen needs for
		// large videos; the app scales the rest of the way
		int step = 1;
		while ((max_width > 0 && frame->width / step > max_width) || (max_height > 0 && frame->height / step > max_height))
			++step;
		size.width = frame->width / step;
		size.height = frame->height / step;

		size_t needed = static_cast<size_t>(size.width) * size.height * 4;
		if (!dst || capacity < needed) return size;

		// The frames are BGRA, maybe upside down; Android's bitmaps are RGBA
		auto out = static_cast<unsigned char *>(dst);
		for (int y = 0; y < size.height; ++y) {
			int src_y = y * step;
			if (frame->flipped) src_y = frame->height - 1 - src_y;
			const unsigned char *row = frame->data.data() + static_cast<size_t>(src_y) * frame->pitch;
			for (int x = 0; x < size.width; ++x) {
				const unsigned char *px = row + static_cast<size_t>(x) * step * 4;
				out[0] = px[2];
				out[1] = px[1];
				out[2] = px[0];
				out[3] = 255;
				out += 4;
			}
		}
		return size;
	}

	json::Object State(agi::Context *c, Args) {
		return json_io::Obj("video", video_state(c), "audio", audio_state(c));
	}

	json::Object OpenVideo(agi::Context *c, Args args) {
		auto path = json_io::GetString(args, "path");
		if (path.empty()) throw agi::InvalidInputException("No video file given");
		c->project->LoadVideo(agi::fs::path(path));
		return State(c, args);
	}

	json::Object CloseVideo(agi::Context *c, Args args) {
		c->project->CloseVideo();
		return State(c, args);
	}

	json::Object OpenAudio(agi::Context *c, Args args) {
		agi::fs::path path;
		if (json_io::GetBool(args, "from_video"))
			path = c->project->VideoName();
		else
			path = json_io::GetString(args, "path");
		if (path.empty()) throw agi::InvalidInputException("No audio file given");
		c->project->LoadAudio(path);
		return State(c, args);
	}

	json::Object CloseAudio(agi::Context *c, Args args) {
		c->project->CloseAudio();
		return State(c, args);
	}

	json::Object Seek(agi::Context *c, Args args) {
		auto vc = c->videoController.get();
		if (!c->project->VideoProvider()) return State(c, args);

		if (args.count("frame"))
			vc->JumpToFrame(static_cast<int>(json_io::GetInt(args, "frame")));
		else if (args.count("time"))
			vc->JumpToTime(static_cast<int>(json_io::GetInt(args, "time")));
		else if (args.count("step")) {
			int step = static_cast<int>(json_io::GetInt(args, "step"));
			if (step == 1) vc->NextFrame();
			else if (step == -1) vc->PrevFrame();
			else vc->JumpToFrame(vc->GetFrameN() + step);
		}
		return State(c, args);
	}

	json::Object Play(agi::Context *c, Args args) {
		auto vc = c->videoController.get();
		if (c->project->VideoProvider()) {
			if (json_io::GetBool(args, "line")) vc->PlayLine();
			else vc->Play();
		}
		else if (c->project->AudioProvider()) {
			// Audio without video plays the line, as the audio box's button does
			if (c->audioController->IsPlaying()) c->audioController->Stop();
			else c->audioController->PlayPrimaryRange();
		}
		return State(c, args);
	}

	json::Object Stop(agi::Context *c, Args args) {
		c->videoController->Stop();
		c->audioController->Stop();
		return State(c, args);
	}

	json::Object Position(agi::Context *c, Args) {
		auto vc = c->videoController.get();
		int frame = vc->GetFrameN();
		return json_io::Obj(
			"frame", frame,
			"time", c->project->VideoProvider() ? vc->TimeAtFrame(frame) : 0,
			"playing", vc->IsPlaying(),
			"audio_playing", c->audioController->IsPlaying()
		);
	}

	void Connect(agi::Context *c) {
		++connection_generation;
		video_event_queued = audio_event_queued = false;
		auto video_changed = [c] { queue_state_event(c, video_event_queued, "video"); };
		connections.push_back(c->project->AddVideoProviderListener([=](AsyncVideoProvider *provider) {
			if (!provider) ClearFrame();
			video_changed();
		}));
		connections.push_back(c->project->AddKeyframesListener([=](std::vector<int> const&) { video_changed(); }));
		connections.push_back(c->project->AddTimecodesListener([=](agi::vfr::Framerate const&) { video_changed(); }));
		connections.push_back(c->project->AddAudioProviderListener([c](agi::AudioProvider *) {
			queue_state_event(c, audio_event_queued, "audio");
		}));
		connections.push_back(c->videoController->AddSeekListener([c](int frame) {
			bridge::Event("video_seek", seek_event(c, frame));
		}));
		connections.push_back(c->audioController->AddPlaybackPositionListener([](int ms) {
			// The playback timer is fast; the app only needs to move a cursor
			auto now = std::chrono::steady_clock::now();
			if (now - last_position_event < std::chrono::milliseconds(40)) return;
			last_position_event = now;
			bridge::Event("audio_position", json_io::Obj("time", ms));
		}));
		connections.push_back(c->audioController->AddPlaybackStopListener([] {
			bridge::Event("audio_stop");
		}));
	}

	void Disconnect() {
		++connection_generation;
		connections.clear();
		ClearFrame();
	}
}
