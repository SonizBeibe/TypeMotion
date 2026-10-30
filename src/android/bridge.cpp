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
/// @brief Messages between the engine and the Android app
/// @ingroup android

#include "bridge.h"

#include "jni_util.h"
#include "json_io.h"

#include <libaegisub/log.h>

#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>

namespace {
	jobject callbacks = nullptr;
	jmethodID on_event = nullptr;
	jmethodID on_request = nullptr;

	std::mutex requests_mutex;
	std::condition_variable requests_cv;
	int64_t next_request_id = 1;
	/// Answers by request id; a request without an answer yet has nothing
	std::map<int64_t, std::optional<std::string>> requests;
	bool shutting_down = false;

	/// Report a Java exception thrown by a callback instead of leaving it
	/// pending, which would make the next JNI call abort
	void check_exception(JNIEnv *env, const char *callback) {
		if (!env->ExceptionCheck()) return;
		env->ExceptionDescribe();
		env->ExceptionClear();
		LOG_E("android/bridge") << callback << " threw an exception";
	}
}

namespace bridge {
	void SetCallbacks(JNIEnv *env, jobject new_callbacks) {
		if (callbacks)
			env->DeleteGlobalRef(callbacks);
		callbacks = env->NewGlobalRef(new_callbacks);

		jni::LocalRef<jclass> cls(env, env->GetObjectClass(callbacks));
		on_event = env->GetMethodID(cls.get(), "onEvent", "(Ljava/lang/String;Ljava/lang/String;)V");
		on_request = env->GetMethodID(cls.get(), "onRequest", "(JLjava/lang/String;Ljava/lang/String;)V");
		check_exception(env, "NativeCallbacks");
	}

	void Event(std::string_view kind, json::Object data) {
		if (!callbacks || !on_event) return;
		auto env = jni::Env();
		jni::LocalRef<jstring> jkind(env, jni::ToJava(env, kind));
		jni::LocalRef<jstring> jdata(env, jni::ToJava(env, json_io::Write(std::move(data))));
		env->CallVoidMethod(callbacks, on_event, jkind.get(), jdata.get());
		check_exception(env, "onEvent");
	}

	json::Object Request(std::string_view kind, json::Object data) {
		if (!callbacks || !on_request) return {};

		int64_t id;
		{
			std::lock_guard<std::mutex> lock(requests_mutex);
			if (shutting_down) return {};
			id = next_request_id++;
			requests[id];
		}

		auto env = jni::Env();
		{
			jni::LocalRef<jstring> jkind(env, jni::ToJava(env, kind));
			jni::LocalRef<jstring> jdata(env, jni::ToJava(env, json_io::Write(std::move(data))));
			env->CallVoidMethod(callbacks, on_request, static_cast<jlong>(id), jkind.get(), jdata.get());
			check_exception(env, "onRequest");
		}

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
