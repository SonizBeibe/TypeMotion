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

/// @file typemotion_ios.h
/// @brief The engine of the iPhone app, as C functions
/// @ingroup ios
///
/// The same engine as the Android app's (see ../android/android.h and
/// ../android/engine_api.h), for Kotlin/Native's cinterop. Everything is
/// JSON in UTF-8. The strings these functions return are the caller's, to
/// free with tm_free(). tm_init() and tm_call() block, so the app calls them
/// from a background thread; the callbacks are called on the engine's
/// threads with the user pointer given to tm_init().

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Something happened: kind and its data
typedef void (*tm_event_callback)(void *user, const char *kind, const char *json);
/// The engine asks something and waits until tm_respond(id, ...) answers it
typedef void (*tm_request_callback)(void *user, int64_t id, const char *kind, const char *json);

/// Start the engine; returns {} or {error}
char *tm_init(const char *config, tm_event_callback on_event, tm_request_callback on_request, void *user);
/// Run an API method and return its result
char *tm_call(const char *method, const char *args);
void tm_respond(int64_t id, const char *answer);
/// Copy the newest video frame into dst as RGBA, at most max_width by
/// max_height (0 for any size); returns {width, height, serial, copied}
char *tm_copy_frame(void *dst, size_t capacity, int max_width, int max_height);
void tm_cancel(void);
void tm_shutdown(void);
void tm_free(char *str);

#ifdef __cplusplus
}
#endif
