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

/// @file bridge.cpp
/// @brief Messages between the engine and the app
/// @ingroup android

#include "bridge.h"

#include "json_io.h"

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>

namespace {
	bridge::Callbacks callbacks;

	std::mutex requests_mutex;
	std::condition_variable requests_cv;
	int64_t next_request_id = 1;
	/// Answers by request id; a request without an answer yet has nothing
	std::map<int64_t, std::optional<std::string>> requests;
	bool shutting_down = false;
}

namespace bridge {
	void SetCallbacks(Callbacks new_callbacks) {
		callbacks = std::move(new_callbacks);
	}

	void Event(std::string_view kind, json::Object data) {
		if (!callbacks.event) return;
		callbacks.event(kind, json_io::Write(std::move(data)));
	}

	json::Object Request(std::string_view kind, json::Object data) {
		if (!callbacks.request) return {};

		int64_t id;
		{
			std::lock_guard<std::mutex> lock(requests_mutex);
			if (shutting_down) return {};
			id = next_request_id++;
			requests[id];
		}

		callbacks.request(id, kind, json_io::Write(std::move(data)));

		std::unique_lock<std::mutex> lock(requests_mutex);
		requests_cv.wait(lock, [&] { return shutting_down || requests[id].has_value(); });
		auto answer = std::move(requests[id]);
		requests.erase(id);
		lock.unlock();

		return answer ? json_io::ReadObject(*answer) : json::Object();
	}

	void Respond(int64_t id, std::string const& answer) {
		{
			std::lock_guard<std::mutex> lock(requests_mutex);
			auto it = requests.find(id);
			if (it == requests.end()) return;
			it->second = answer;
		}
		requests_cv.notify_all();
	}

	void Shutdown() {
		{
			std::lock_guard<std::mutex> lock(requests_mutex);
			shutting_down = true;
		}
		requests_cv.notify_all();
	}
}
