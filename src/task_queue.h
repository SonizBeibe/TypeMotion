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

/// @file task_queue.h
/// @brief The main thread of frontends which have no event loop of their own
///
/// The core runs everything which touches the subtitles on one thread, which
/// it reaches through agi::dispatch::Main(). The desktop program uses the
/// wxWidgets event loop for that. Other frontends pick a thread of their own
/// and run the queued tasks on it with this.

#pragma once

#include <functional>

namespace task_queue {
	/// Make agi::dispatch::Main() queue its tasks until Run() runs them,
	/// and make the calling thread the main thread
	void InitDispatch();

	/// Run the queued tasks until done() returns true, waiting for more
	/// tasks when there are none. Must be called on the main thread.
	void Run(std::function<bool()> const& done);

	/// Wake up Run() so that it checks done() again
	void Wake();

	/// Is the calling thread the one InitDispatch() was called on?
	bool IsMainThread();
}
