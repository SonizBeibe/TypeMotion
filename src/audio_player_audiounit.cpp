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

/// @file audio_player_audiounit.cpp
/// @brief Audio output on the iPhone, with a RemoteIO audio unit
/// @ingroup audio_output

#ifdef WITH_AUDIOUNIT
#include "include/aegisub/audio_player.h"

#include <libaegisub/audio/provider.h>
#include <libaegisub/log.h>

#include <AudioToolbox/AudioToolbox.h>
#include <TargetConditionals.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

namespace {
class AudioUnitPlayer final : public AudioPlayer {
	AudioComponentInstance unit = nullptr;
	int frame_size;

	/// The range being played, in samples
	std::atomic<int64_t> start_frame{0};
	std::atomic<int64_t> end_frame{0};
	/// The next sample the callback gives the unit
	std::atomic<int64_t> next_frame{0};
	std::atomic<bool> playing{false};
	std::atomic<bool> running{false};
	std::atomic<double> volume{1.0};
	std::mutex start_mutex;

	static OSStatus OnRender(void *user, AudioUnitRenderActionFlags *flags, const AudioTimeStamp *,
	                         UInt32, UInt32 frames, AudioBufferList *data) {
		auto self = static_cast<AudioUnitPlayer *>(user);
		auto audio = static_cast<char *>(data->mBuffers[0].mData);
		int64_t count = 0;
		if (self->playing) {
			int64_t next = self->next_frame;
			count = std::max<int64_t>(0, std::min<int64_t>(frames, self->end_frame - next));
			if (count > 0) {
				self->provider->GetAudioWithVolume(audio, next, count, self->volume);
				self->next_frame = next + count;
			}
		}

		if (count < frames)
			memset(audio + count * self->frame_size, 0, (frames - count) * self->frame_size);
		if (count == 0)
			*flags |= kAudioUnitRenderAction_OutputIsSilence;

		// After the end of the range the unit plays silence until Stop(), as
		// stopping it from its own render thread isn't allowed
		return noErr;
	}

	void Check(OSStatus status, const char *what) {
		if (status != noErr)
			throw AudioPlayerOpenError(std::string(what) + " failed with error " + std::to_string(status));
	}

public:
	AudioUnitPlayer(agi::AudioProvider *provider)
	: AudioPlayer(provider)
	, frame_size(provider->GetChannels() * provider->GetBytesPerSample())
	{
		AudioStreamBasicDescription format{};
		format.mSampleRate = provider->GetSampleRate();
		format.mFormatID = kAudioFormatLinearPCM;
		if (provider->AreSamplesFloat() && provider->GetBytesPerSample() == 4)
			format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
		else if (!provider->AreSamplesFloat() && provider->GetBytesPerSample() == 2)
			format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
		else
			throw AudioPlayerOpenError("The audio unit can't play this sample format");
		format.mChannelsPerFrame = provider->GetChannels();
		format.mBitsPerChannel = provider->GetBytesPerSample() * 8;
		format.mBytesPerFrame = frame_size;
		format.mFramesPerPacket = 1;
		format.mBytesPerPacket = frame_size;

		AudioComponentDescription description{};
		description.componentType = kAudioUnitType_Output;
#if TARGET_OS_IPHONE
		description.componentSubType = kAudioUnitSubType_RemoteIO;
#else
		description.componentSubType = kAudioUnitSubType_DefaultOutput;
#endif
		description.componentManufacturer = kAudioUnitManufacturer_Apple;
		AudioComponent component = AudioComponentFindNext(nullptr, &description);
		if (!component)
			throw AudioPlayerOpenError("There is no output audio unit");
		Check(AudioComponentInstanceNew(component, &unit), "AudioComponentInstanceNew");

		try {
			Check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
			                           &format, sizeof(format)), "Setting the stream format");
			AURenderCallbackStruct callback{OnRender, this};
			Check(AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
			                           &callback, sizeof(callback)), "Setting the render callback");
			Check(AudioUnitInitialize(unit), "AudioUnitInitialize");
		}
		catch (...) {
			AudioComponentInstanceDispose(unit);
			throw;
		}
	}

	~AudioUnitPlayer() {
		AudioOutputUnitStop(unit);
		AudioUnitUninitialize(unit);
		AudioComponentInstanceDispose(unit);
	}

	void Play(int64_t start, int64_t count) override {
		std::lock_guard<std::mutex> lock(start_mutex);
		playing = false;
		start_frame = start;
		end_frame = start + count;
		next_frame = start;
		playing = true;
		if (!running) {
			auto status = AudioOutputUnitStart(unit);
			if (status != noErr)
				LOG_E("audio/player/audiounit") << "Could not start the audio unit: " << status;
			else
				running = true;
		}
	}

	void Stop() override {
		std::lock_guard<std::mutex> lock(start_mutex);
		playing = false;
		if (running) {
			AudioOutputUnitStop(unit);
			running = false;
		}
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
		// What has been given to the unit, which plays it a few milliseconds later
		return std::clamp<int64_t>(next_frame, start_frame, std::max<int64_t>(start_frame, end_frame));
	}

	void SetEndPosition(int64_t pos) override {
		end_frame = pos;
	}
};
}

std::unique_ptr<AudioPlayer> CreateAudioUnitPlayer(agi::AudioProvider *provider, wxWindow *) {
	return std::make_unique<AudioUnitPlayer>(provider);
}
#endif // WITH_AUDIOUNIT
