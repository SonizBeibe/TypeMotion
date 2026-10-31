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

/// @file engine_api.h
/// @brief Starting, calling and stopping the engine
/// @ingroup android
///
/// What the app's NativeEngine calls, without anything of the system it
/// runs on: engine_jni.cpp makes these Android's native methods, and
/// ios/capi.cpp C functions for the iPhone app. See android.h.

#pragma once

#include "bridge.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace engine_api {
	/// Start the engine thread with the app's configuration (JSON)
	/// @return {} or {error}
	std::string Init(std::string const& config, bridge::Callbacks callbacks);

	/// Run an API method on the engine thread and wait for its result
	std::string Call(std::string const& method, std::string const& args);

	/// Answer one of the engine's requests
	void Respond(int64_t id, std::string const& answer);

	/// Copy the newest video frame into dst (RGBA), at most max_width by
	/// max_height (0 for any size)
	/// @return {width, height, serial, copied}
	std::string CopyFrame(void *dst, size_t capacity, int max_width, int max_height);

	/// Ask the running script to stop
	void Cancel();

	/// Stop the engine thread
	void Shutdown();
}
