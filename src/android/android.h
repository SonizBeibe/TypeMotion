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

/// @file android.h
/// @brief The engine of the Android app
/// @ingroup android
///
/// The app talks to the subtitle core through app.typemotion.engine.NativeEngine:
///
///     object NativeEngine {
///         @JvmStatic external fun init(config: String, callbacks: NativeCallbacks): String
///         @JvmStatic external fun call(method: String, args: String): String
///         @JvmStatic external fun respond(id: Long, answer: String)
///         @JvmStatic external fun copyFrame(buffer: ByteBuffer, maxWidth: Int, maxHeight: Int): String
///         @JvmStatic external fun cancel()
///         @JvmStatic external fun shutdown()
///     }
///
/// Everything is JSON. init() starts the engine thread, which every change
/// to the subtitles runs on, and call() runs one of the methods in api.cpp
/// there and waits for its result. Both block, so the app calls them from a
/// background thread. copyFrame() copies the newest video frame into a
/// direct buffer (see media.h) on the thread that calls it.

#pragma once

#include <libaegisub/cajun/elements.h>
#include <libaegisub/fs.h>

#include <string>

namespace agi { struct Context; }

namespace android {
	/// Folders the engine uses, from the app's configuration
	struct Folders {
		/// Files which come with the app: automation/include, the official
		/// scripts in automation/autoload, and so on (?data)
		std::string data;
		/// The app's private files: configuration, recent files, autosaves (?user)
		std::string user;
		/// Temporary files (?temp)
		std::string cache;
		/// The TypeMotion folder in the phone's storage, where the user puts
		/// their own automation scripts and fonts
		std::string storage;
	};

	/// The TypeMotion folder in the phone's storage
	agi::fs::path const& StorageFolder();

	/// Translate the core's text with the messages of a .po file
	void LoadTranslations(agi::fs::path const& file);

	/// Ask the running script to stop
	void CancelScript();

	/// Run an API method on the engine thread
	/// @return The result, which has "error" set if it failed
	json::Object CallMethod(agi::Context *c, std::string const& method, json::Object const& args);

	/// Tell the app about changes to the context's subtitles from now on
	void ConnectEvents(agi::Context *c);

	/// Stop telling the app about changes, before the context is destroyed
	void DisconnectEvents();
}
