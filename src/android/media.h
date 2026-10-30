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

/// @file media.h
/// @brief The video and audio of the app
/// @ingroup android
///
/// The core opens, decodes and plays video and audio as in the desktop
/// program (VideoController, AudioController, the providers). The app shows
/// the frames: when one is ready the engine sends a "video_frame" event, and
/// the app copies it with NativeEngine.copyFrame().
///
/// Events: "video" and "audio" when one is opened or closed, with the same
/// fields as video_state; "video_seek" {frame, time, playing} when the
/// video moves to another frame; "video_frame" {serial, width, height};
/// "audio_position" {time} while audio plays, and "audio_stop".

#pragma once

#include <libaegisub/cajun/elements.h>

#include <cstddef>
#include <memory>

namespace agi { struct Context; }
struct VideoFrame;

namespace android::media {
	using Args = json::Object const&;

	/// Keep a frame the video provider made, for the app to copy
	void FrameReady(std::shared_ptr<VideoFrame> frame);

	/// The newest frame's size once it is made at most max_width by
	/// max_height (0 for any size) by skipping pixels evenly
	struct FrameSize {
		int width = 0;
		int height = 0;
		/// Counts up with each new frame
		int64_t serial = 0;
	};

	/// Copy the newest frame into dst as RGBA, made smaller as above
	/// @return The copied frame's size, or zero if there is no frame. If
	///         capacity is too small nothing is copied, and the size says
	///         how much is needed.
	FrameSize CopyFrame(void *dst, size_t capacity, int max_width, int max_height);

	/// Forget the frame of a video that was closed
	void ClearFrame();

	/// The open video and audio, and where the video is:
	/// {video: {path, width, height, display_width, display_height, frames,
	///  fps, frame, time, playing, keyframes} or null,
	///  audio: {path, duration, sample_rate, channels} or null}
	json::Object State(agi::Context *c, Args);

	/// {path}; the errors are shown as the desktop program shows them
	json::Object OpenVideo(agi::Context *c, Args args);
	json::Object CloseVideo(agi::Context *c, Args);
	/// {path}, or {from_video: true} for the video's audio
	json::Object OpenAudio(agi::Context *c, Args args);
	json::Object CloseAudio(agi::Context *c, Args);

	/// Go to {frame}, or to {time} in milliseconds, or {step} frames on
	json::Object Seek(agi::Context *c, Args args);
	/// Play from the current frame, or stop if it plays; {line: true} plays
	/// the active line instead
	json::Object Play(agi::Context *c, Args args);
	json::Object Stop(agi::Context *c, Args);
	/// {frame, time, playing, audio_playing}: what the app checks while
	/// something plays, since playback stops at the end without an event,
	/// as the desktop program's buttons check it
	json::Object Position(agi::Context *c, Args);

	/// Tell the app about the context's video and audio from now on
	void Connect(agi::Context *c);
	void Disconnect();
}
