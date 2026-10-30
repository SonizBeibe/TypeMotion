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

/// @file jni_util.h
/// @brief Helpers for talking to the Java virtual machine
/// @ingroup android

#pragma once

#include <jni.h>

#include <string>
#include <string_view>

namespace jni {
	/// Remember the virtual machine the library was loaded into
	void SetVM(JavaVM *vm);

	/// The JNI environment of the calling thread. Threads the virtual machine
	/// didn't start are attached to it, and detached when they end.
	JNIEnv *Env();

	/// Convert a Java string to UTF-8
	std::string FromJava(JNIEnv *env, jstring str);

	/// Convert UTF-8 to a new Java string
	jstring ToJava(JNIEnv *env, std::string_view str);

	/// A local reference which is deleted when it goes out of scope, for
	/// threads which call into Java without returning to it
	template<typename T>
	class LocalRef {
		JNIEnv *env;
		T ref;

	public:
		LocalRef(JNIEnv *env, T ref) : env(env), ref(ref) { }
		~LocalRef() { if (ref) env->DeleteLocalRef(ref); }
		LocalRef(LocalRef const&) = delete;
		LocalRef& operator=(LocalRef const&) = delete;

		T get() const { return ref; }
	};
}
