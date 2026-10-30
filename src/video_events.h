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

/// @file video_events.h
/// @brief The wxWidgets events which AsyncVideoProvider's results are sent as

#pragma once

#include "async_video_provider.h"

#include <libaegisub/exception.h>

#include <memory>
#include <string>
#include <wx/event.h>

/// Event which signals that a requested frame is ready
struct FrameReadyEvent final : public wxEvent {
	/// Frame which is ready
	std::shared_ptr<VideoFrame> frame;
	/// Time which was used for subtitle rendering
	double time;
	wxEvent *Clone() const override { return new FrameReadyEvent(*this); };
	FrameReadyEvent(std::shared_ptr<VideoFrame> frame, double time)
	: frame(std::move(frame)), time(time) { }
};

// These exceptions are wxEvents so that they can be passed directly back to
// the parent thread as events
struct VideoProviderErrorEvent final : public wxEvent, public agi::Exception {
	wxEvent *Clone() const override { return new VideoProviderErrorEvent(*this); };
	VideoProviderErrorEvent(std::string const& msg);
};

struct SubtitlesProviderErrorEvent final : public wxEvent, public agi::Exception {
	wxEvent *Clone() const override { return new SubtitlesProviderErrorEvent(*this); };
	SubtitlesProviderErrorEvent(std::string const& msg);
};

wxDECLARE_EVENT(EVT_FRAME_READY, FrameReadyEvent);
wxDECLARE_EVENT(EVT_VIDEO_ERROR, VideoProviderErrorEvent);
wxDECLARE_EVENT(EVT_SUBTITLES_ERROR, SubtitlesProviderErrorEvent);

/// Create a listener for AsyncVideoProvider which sends what it receives to
/// the given event handler as the events above
std::unique_ptr<AsyncVideoProviderListener> MakeVideoEventSender(wxEvtHandler *handler);
