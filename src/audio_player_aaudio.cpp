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

/// @file audio_player_aaudio.cpp
/// @brief Audio output on Android, with AAudio
/// @ingroup audio_output

#ifdef WITH_AAUDIO
#include "include/aegisub/audio_player.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/log.h>

#include <aaudio/AAudio.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
class AAudioPlayer final : public AudioPlayer {
	AAudioStream *stream = nullptr;
	int frame_size;

	/// The range being played, in samples
	std::atomic<int64_t> start_frame{0};
	std::atomic<int64_t> end_frame{0};
	/// The next sample the callback gives the stream
	std::atomic<int64_t> next_frame{0};
	/// The stream's count of frames written when the range started, so that
	/// the frames it has read since tell where playback is
	std::atomic<int64_t> base_written{0};
	std::atomic<bool> playing{false};
	std::atomic<double> volume{1.0};
	/// Callbacks of silence since playback stopped; the stream stops itself
	/// after a while, so that it doesn't run while nothing plays
	int idle_callbacks = 0;

	std::mutex start_mutex;

	static aaudio_data_callback_result_t OnData(AAudioStream *, void *user, void *audio, int32_t frames) {
		return static_cast<AAudioPlayer *>(user)->Fill(static_cast<char *>(audio), frames);
	}

	static void OnError(AAudioStream *, void *, aaudio_result_t error) {
		LOG_E("audio/player/aaudio") << "Stream error: " << AAudio_convertResultToText(error);
	}

	aaudio_data_callback_result_t Fill(char *audio, int32_t frames) {
		int64_t count = 0;
		if (playing) {
			int64_t next = next_frame;
			count = std::max<int64_t>(0, std::min<int64_t>(frames, end_frame - next));
			if (count > 0) {
				provider->GetAudioWithVolume(audio, next, count, volume);
				next_frame = next + count;
			}
			idle_callbacks = 0;
		}
		else
			++idle_callbacks;

		if (count < frames)
			memset(audio + count * frame_size, 0, (frames - count) * frame_size);

		// About half a second of silence at the usual buffer sizes
		return idle_callbacks > 50 ? AAUDIO_CALLBACK_RESULT_STOP : AAUDIO_CALLBACK_RESULT_CONTINUE;
	}

	void StartStream() {
		aaudio_stream_state_t state = AAudioStream_getState(stream);
		if (state == AAUDIO_STREAM_STATE_STARTING || state == AAUDIO_STREAM_STATE_STARTED)
			return;
		// It may still be stopping on its own after a pause in playback
		for (int i = 0; i < 10 && (state == AAUDIO_STREAM_STATE_STOPPING || state == AAUDIO_STREAM_STATE_PAUSING); ++i)
			AAudioStream_waitForStateChange(stream, state, &state, 50'000'000);
		auto result = AAudioStream_requestStart(stream);
		if (result != AAUDIO_OK)
			LOG_E("audio/player/aaudio") << "Could not start the stream: " << AAudio_convertResultToText(result);
	}

public:
	AAudioPlayer(agi::AudioProvider *provider)
	: AudioPlayer(provider)
	, frame_size(provider->GetChannels() * provider->GetBytesPerSample())
	{
		aaudio_format_t format;
		if (provider->AreSamplesFloat() && provider->GetBytesPerSample() == 4)
			format = AAUDIO_FORMAT_PCM_FLOAT;
		else if (!provider->AreSamplesFloat() && provider->GetBytesPerSample() == 2)
			format = AAUDIO_FORMAT_PCM_I16;
		else
			throw AudioPlayerOpenError("AAudio can't play this sample format");

		AAudioStreamBuilder *builder = nullptr;
		if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK)
			throw AudioPlayerOpenError("Could not create an AAudio stream builder");
		AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
		AAudioStreamBuilder_setFormat(builder, format);
		AAudioStreamBuilder_setChannelCount(builder, provider->GetChannels());
		AAudioStreamBuilder_setSampleRate(builder, provider->GetSampleRate());
		AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
		AAudioStreamBuilder_setDataCallback(builder, OnData, this);
		AAudioStreamBuilder_setErrorCallback(builder, OnError, this);
		auto result = AAudioStreamBuilder_openStream(builder, &stream);
		AAudioStreamBuilder_delete(builder);
		if (result != AAUDIO_OK)
			throw AudioPlayerOpenError(std::string("Could not open an AAudio stream: ") + AAudio_convertResultToText(result));
	}

	~AAudioPlayer() {
		AAudioStream_requestStop(stream);
		AAudioStream_close(stream);
	}

	void Play(int64_t start, int64_t count) override {
		std::lock_guard<std::mutex> lock(start_mutex);
		playing = false;
		start_frame = start;
		end_frame = start + count;
		next_frame = start;
		base_written = AAudioStream_getFramesWritten(stream);
		playing = true;
		StartStream();
	}

	void Stop() override {
		playing = false;
	}

	bool IsPlaying() override {
		return playing && GetCurrentPosition() < end_frame;
	}

	void SetVolume(double new_volume) override {
		volume = new_volume;
	}

	int64_t GetEndPosition() override {
		return end_frame;
	}

	int64_t GetCurrentPosition() override {
		int64_t played = AAudioStream_getFramesRead(stream) - base_written;
		return std::clamp<int64_t>(start_frame + played, start_frame, std::max<int64_t>(start_frame, next_frame));
	}

	void SetEndPosition(int64_t pos) override {
		end_frame = pos;
	}
};
}

std::unique_ptr<AudioPlayer> CreateAAudioPlayer(agi::AudioProvider *provider, wxWindow *) {
	return std::make_unique<AAudioPlayer>(provider);
}
#endif // WITH_AAUDIO
