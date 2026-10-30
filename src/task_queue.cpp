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

/// @file task_queue.cpp
/// @brief The main thread of frontends which have no event loop of their own

#include "task_queue.h"

#include "ui_hooks.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace {
	std::thread::id main_thread;
	std::mutex main_mutex;
	std::condition_variable main_cv;
	std::deque<agi::dispatch::Thunk> main_tasks;
}

namespace task_queue {
	void InitDispatch() {
		main_thread = std::this_thread::get_id();
		agi::dispatch::Init([](agi::dispatch::Thunk task) {
			{
				std::lock_guard<std::mutex> lock(main_mutex);
				main_tasks.push_back(std::move(task));
			}
			main_cv.notify_all();
		});
	}

	void Run(std::function<bool()> const& done) {
		std::unique_lock<std::mutex> lock(main_mutex);
		while (true) {
			while (!main_tasks.empty()) {
				auto task = std::move(main_tasks.front());
				main_tasks.pop_front();
				lock.unlock();
				try {
					task();
				}
				catch (agi::Exception const& e) {
					ui::LogError(e.GetMessage());
				}
				catch (std::exception const& e) {
					ui::LogError(e.what());
				}
				lock.lock();
			}
			if (done()) return;
			main_cv.wait(lock, [&] { return !main_tasks.empty() || done(); });
		}
	}

	void Wake() {
		{
			// Taking the lock makes sure Run() is either checking done()
			// already or waiting, so the notification can't be missed
			std::lock_guard<std::mutex> lock(main_mutex);
		}
		main_cv.notify_all();
	}

	bool IsMainThread() {
		return std::this_thread::get_id() == main_thread;
	}
}
