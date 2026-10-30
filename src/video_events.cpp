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

#include "video_events.h"

namespace {
class VideoEventSender final : public AsyncVideoProviderListener {
	wxEvtHandler *handler;

public:
	VideoEventSender(wxEvtHandler *handler) : handler(handler) { }

	void FrameReady(std::shared_ptr<VideoFrame> frame, double time) override {
		auto evt = new FrameReadyEvent(std::move(frame), time);
		evt->SetEventType(EVT_FRAME_READY);
		handler->QueueEvent(evt);
	}

	void VideoError(std::string const& message) override {
		handler->QueueEvent(new VideoProviderErrorEvent(message));
	}

	void SubtitlesError(std::string const& message) override {
		handler->QueueEvent(new SubtitlesProviderErrorEvent(message));
	}
};
}

std::unique_ptr<AsyncVideoProviderListener> MakeVideoEventSender(wxEvtHandler *handler) {
	return std::make_unique<VideoEventSender>(handler);
}

wxDEFINE_EVENT(EVT_FRAME_READY, FrameReadyEvent);
wxDEFINE_EVENT(EVT_VIDEO_ERROR, VideoProviderErrorEvent);
wxDEFINE_EVENT(EVT_SUBTITLES_ERROR, SubtitlesProviderErrorEvent);

VideoProviderErrorEvent::VideoProviderErrorEvent(std::string const& err)
: agi::Exception(std::string(err))
{
	SetEventType(EVT_VIDEO_ERROR);
}
SubtitlesProviderErrorEvent::SubtitlesProviderErrorEvent(std::string const& err)
: agi::Exception(std::string(err))
{
	SetEventType(EVT_SUBTITLES_ERROR);
}
