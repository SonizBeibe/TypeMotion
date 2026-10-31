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

/// @file bridge.h
/// @brief Messages between the engine and the app
/// @ingroup android
///
/// The app gives the engine its callbacks when it starts it: on Android an
/// object implementing app.typemotion.engine.NativeCallbacks (see
/// engine_jni.cpp), on iOS C functions (see ios/typemotion_ios.h):
///
///     interface NativeCallbacks {
///         fun onEvent(kind: String, json: String)
///         fun onRequest(id: Long, kind: String, json: String)
///     }
///
/// Events tell the app something happened. Requests ask the user something,
/// and the thread which asked waits until the app calls
/// NativeEngine.respond(id, json). Both are called on the engine's threads,
/// never on the app's main thread, and the app has to answer requests
/// without calling back into the engine first.

#pragma once

#include <libaegisub/cajun/elements.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace bridge {
	/// How the engine reaches the app
	struct Callbacks {
		/// (kind, json)
		std::function<void(std::string_view, std::string const&)> event;
		/// (id, kind, json)
		std::function<void(int64_t, std::string_view, std::string const&)> request;
	};

	/// Keep the app's callbacks
	void SetCallbacks(Callbacks callbacks);

	/// Tell the app something happened
	void Event(std::string_view kind, json::Object data = {});

	/// Ask the app something and wait for the answer
	/// @return The answer, or an empty object if the engine is shutting down
	json::Object Request(std::string_view kind, json::Object data = {});

	/// Answer a request, from the app
	void Respond(int64_t id, std::string const& answer);

	/// Stop waiting for answers, as the engine is shutting down
	void Shutdown();
}
