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

/// @file crash_report.h
/// @brief Writes down why the engine crashed, for the app to show
/// @ingroup android
///
/// A phone shows no more than "the app has stopped", so when the engine
/// aborts or crashes, what happened and where is written to a file. The app
/// shows it the next time it opens, for the user to copy and send.

#pragma once

#include <string>

namespace android::crash {
	/// Write reports to @p path from now on, starting with @p header (the
	/// app's version, the phone and so on)
	void Install(std::string const& path, std::string const& header);
}
