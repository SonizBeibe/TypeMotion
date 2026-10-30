// Copyright (c) 2013, Thomas Goyne <plorkyeran@aegisub.org>
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
//
// Aegisub Project http://www.aegisub.org/

#pragma once

#include "include/aegisub/video_provider.h"

#include <libaegisub/exception.h>
#include <libaegisub/fs.h>

#include <atomic>
#include <memory>
#include <set>

class AssDialogue;
class AssFile;
class SubtitlesProvider;
class VideoProvider;
class VideoProviderError;
struct AssDialogueBase;
struct VideoFrame;
namespace agi {
	class BackgroundRunner;
	namespace dispatch { class Queue; }
}

/// An error from the video provider when getting a frame
DEFINE_EXCEPTION(VideoFrameError, agi::Exception);
/// An error from the subtitles provider when rendering a frame's subtitles
DEFINE_EXCEPTION(SubtitlesFrameError, agi::Exception);

/// Receives the frames requested from an AsyncVideoProvider and any errors.
/// The functions are called on the provider's worker thread.
class AsyncVideoProviderListener {
public:
	virtual ~AsyncVideoProviderListener() = default;
	virtual void FrameReady(std::shared_ptr<VideoFrame> frame, double time) = 0;
	virtual void VideoError(std::string const& message) = 0;
	virtual void SubtitlesError(std::string const& message) = 0;
};

/// An asynchronous video decoding and subtitle rendering wrapper
class AsyncVideoProvider {
public:
	using Listener = AsyncVideoProviderListener;

private:
	/// Asynchronous work queue
	std::unique_ptr<agi::dispatch::Queue> worker;

	/// Subtitles provider
	std::unique_ptr<SubtitlesProvider> subs_provider;
	/// Video provider
	std::unique_ptr<VideoProvider> source_provider;
	/// Event handler to send FrameReady events to
	std::unique_ptr<Listener> listener;

	int frame_number = -1; ///< Last frame number requested
	double time = -1.; ///< Time of the frame to pass to the subtitle renderer

	/// Copy of the subtitles file to avoid having to touch the project context
	std::unique_ptr<AssFile> subs;

	/// If >= 0, the subtitles provider current has just the lines visible on
	/// that frame loaded. If -1, the entire file is loaded. If -2, the
	/// currently loaded file is out of date.
	int single_frame = -1;

	/// Last rendered frame number
	int last_rendered = -1;
	/// Last rendered subtitles on that frame
	std::vector<AssDialogueBase> last_lines;
	/// Check if we actually need to honor a frame request or if no visible
	/// lines have actually changed
	bool NeedUpdate(std::vector<AssDialogueBase const*> const& visible_lines);

	std::shared_ptr<VideoFrame> ProcFrame(int frame, double time, bool raw = false);

	/// Produce a frame if req_version is still the current version
	void ProcAsync(uint_fast32_t req_version, bool check_updated);

	/// Monotonic counter used to drop frames when changes arrive faster than
	/// they can be rendered
	std::atomic<uint_fast32_t> version{ 0 };

	std::vector<std::shared_ptr<VideoFrame>> buffers;

	// Returns a monochromatic frame with the current dimensions
	VideoFrame GetBlankFrame(bool white);

public:
	/// @brief Load the passed subtitle file
	/// @param subs File to load
	///
	/// This function blocks until is it is safe for the calling thread to
	/// modify subs
	void LoadSubtitles(const AssFile *subs) throw();

	/// @brief Update a previously loaded subtitle file
	/// @param changed Line that has changed
	///
	/// This function only supports changes to a single existing line, and not
	/// insertions or deletions.
	void UpdateSubtitles(const AssDialogue *changed) throw();

	/// @brief Queue a request for a frame
	/// @brief frame Frame number
	/// @brief time  Exact start time of the frame in seconds
	///
	/// This merely queues up a request and deletes any pending requests; there
	/// is no guarantee that the requested frame will ever actually be produced
	void RequestFrame(int frame, double time) throw();

	/// @brief Synchronously get a frame
	/// @brief frame Frame number
	/// @brief time  Exact start time of the frame in seconds
	/// @brief raw   Get raw frame without subtitles
	std::shared_ptr<VideoFrame> GetFrame(int frame, double time, bool raw = false);

	/// @brief Synchronously get the subtitles with transparent background
	/// @brief time  Exact start time of the frame in seconds
	///
	/// This function is not used for drawing the subtitles on the screen and is not
	/// guaranteed that drawing the resulting image on the current raw frame exactly
	/// results in the current rendered frame with subtitles. This function is for
	/// purposes like copying the current subtitles to the clipboard.
	VideoFrame GetSubtitles(double time);

	/// Ask the video provider to change YCbCr matrices
	void SetColorSpace(agi::ycbcr::Header matrix);

	int GetFrameCount() const             { return source_provider->GetFrameCount(); }
	int GetWidth() const                  { return source_provider->GetWidth(); }
	int GetHeight() const                 { return source_provider->GetHeight(); }
	double GetDAR() const                 { return source_provider->GetDAR(); }
	std::pair<int, int> GetDisplayResolution() const;
	agi::vfr::Framerate GetFPS() const    { return source_provider->GetFPS(); }
	std::vector<int> GetKeyFrames() const { return source_provider->GetKeyFrames(); }
	agi::ycbcr::header_colorspace GetColorSpace() const     { return source_provider->GetColorSpace(); }
	agi::ycbcr::header_colorspace GetRealColorSpace() const { return source_provider->GetRealColorSpace(); }
	bool IsHDRorWCG() const { return source_provider->IsHDRorWCG(); }
	std::string GetWarning() const        { return source_provider->GetWarning(); }
	std::string GetDecoderName() const    { return source_provider->GetDecoderName(); }
	bool ShouldSetVideoProperties() const { return source_provider->ShouldSetVideoProperties(); }
	bool HasAudio() const                 { return source_provider->HasAudio(); }

	/// @brief Constructor
	/// @param video_filename File to open
	/// @param listener Receives the requested frames and errors
	AsyncVideoProvider(agi::fs::path const& video_filename, agi::ycbcr::Header colormatrix, std::unique_ptr<Listener> listener, agi::BackgroundRunner *br);
	~AsyncVideoProvider();
};
