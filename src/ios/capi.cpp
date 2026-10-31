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

/// @file capi.cpp
/// @brief The engine of the iPhone app, as C functions
/// @ingroup ios

#include "typemotion_ios.h"

#include "../android/engine_api.h"

#include <cstdlib>
#include <cstring>

namespace {
	char *copy(std::string const& str) {
		auto out = static_cast<char *>(std::malloc(str.size() + 1));
		std::memcpy(out, str.c_str(), str.size() + 1);
		return out;
	}

	std::string from(const char *str) {
		return str ? str : "";
	}
}

extern "C" {
char *tm_init(const char *config, tm_event_callback on_event, tm_request_callback on_request, void *user) {
	bridge::Callbacks callbacks;
	if (on_event)
		callbacks.event = [=](std::string_view kind, std::string const& json) {
			on_event(user, std::string(kind).c_str(), json.c_str());
		};
	if (on_request)
		callbacks.request = [=](int64_t id, std::string_view kind, std::string const& json) {
			on_request(user, id, std::string(kind).c_str(), json.c_str());
		};
	return copy(engine_api::Init(from(config), std::move(callbacks)));
}

char *tm_call(const char *method, const char *args) {
	return copy(engine_api::Call(from(method), from(args)));
}

void tm_respond(int64_t id, const char *answer) {
	engine_api::Respond(id, from(answer));
}

char *tm_copy_frame(void *dst, size_t capacity, int max_width, int max_height) {
	return copy(engine_api::CopyFrame(dst, capacity, max_width, max_height));
}

void tm_cancel(void) {
	engine_api::Cancel();
}

void tm_shutdown(void) {
	engine_api::Shutdown();
}

void tm_free(char *str) {
	std::free(str);
}
}
