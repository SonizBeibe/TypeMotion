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

/// @file engine_jni.cpp
/// @brief The native methods of app.typemotion.engine.NativeEngine
/// @ingroup android

#include "engine_api.h"
#include "jni_util.h"

#include <libaegisub/log.h>

#include <jni.h>

namespace {
	const char native_engine_class[] = "app/typemotion/engine/NativeEngine";

	/// The app's NativeCallbacks object
	jobject callbacks = nullptr;
	jmethodID on_event = nullptr;
	jmethodID on_request = nullptr;

	/// Report a Java exception thrown by a callback instead of leaving it
	/// pending, which would make the next JNI call abort
	void check_exception(JNIEnv *env, const char *callback) {
		if (!env->ExceptionCheck()) return;
		env->ExceptionDescribe();
		env->ExceptionClear();
		LOG_E("android/bridge") << callback << " threw an exception";
	}

	void set_callbacks(JNIEnv *env, jobject new_callbacks) {
		if (callbacks)
			env->DeleteGlobalRef(callbacks);
		callbacks = env->NewGlobalRef(new_callbacks);

		jni::LocalRef<jclass> cls(env, env->GetObjectClass(callbacks));
		on_event = env->GetMethodID(cls.get(), "onEvent", "(Ljava/lang/String;Ljava/lang/String;)V");
		on_request = env->GetMethodID(cls.get(), "onRequest", "(JLjava/lang/String;Ljava/lang/String;)V");
		check_exception(env, "NativeCallbacks");
	}

	void event(std::string_view kind, std::string const& json) {
		if (!callbacks || !on_event) return;
		auto env = jni::Env();
		jni::LocalRef<jstring> jkind(env, jni::ToJava(env, kind));
		jni::LocalRef<jstring> jdata(env, jni::ToJava(env, json));
		env->CallVoidMethod(callbacks, on_event, jkind.get(), jdata.get());
		check_exception(env, "onEvent");
	}

	void request(int64_t id, std::string_view kind, std::string const& json) {
		if (!callbacks || !on_request) return;
		auto env = jni::Env();
		jni::LocalRef<jstring> jkind(env, jni::ToJava(env, kind));
		jni::LocalRef<jstring> jdata(env, jni::ToJava(env, json));
		env->CallVoidMethod(callbacks, on_request, static_cast<jlong>(id), jkind.get(), jdata.get());
		check_exception(env, "onRequest");
	}

	jstring native_init(JNIEnv *env, jclass, jstring config, jobject new_callbacks) {
		set_callbacks(env, new_callbacks);
		return jni::ToJava(env, engine_api::Init(jni::FromJava(env, config), {event, request}));
	}

	jstring native_call(JNIEnv *env, jclass, jstring method, jstring args) {
		return jni::ToJava(env, engine_api::Call(jni::FromJava(env, method), jni::FromJava(env, args)));
	}

	void native_respond(JNIEnv *env, jclass, jlong id, jstring answer) {
		engine_api::Respond(id, jni::FromJava(env, answer));
	}

	jstring native_copy_frame(JNIEnv *env, jclass, jobject buffer, jint max_width, jint max_height) {
		void *dst = buffer ? env->GetDirectBufferAddress(buffer) : nullptr;
		jlong capacity = buffer ? env->GetDirectBufferCapacity(buffer) : 0;
		return jni::ToJava(env, engine_api::CopyFrame(dst, capacity > 0 ? static_cast<size_t>(capacity) : 0, max_width, max_height));
	}

	void native_cancel(JNIEnv *, jclass) {
		engine_api::Cancel();
	}

	void native_shutdown(JNIEnv *, jclass) {
		engine_api::Shutdown();
	}

	const JNINativeMethod native_methods[] = {
		{const_cast<char *>("init"), const_cast<char *>("(Ljava/lang/String;Lapp/typemotion/engine/NativeCallbacks;)Ljava/lang/String;"), reinterpret_cast<void *>(native_init)},
		{const_cast<char *>("call"), const_cast<char *>("(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"), reinterpret_cast<void *>(native_call)},
		{const_cast<char *>("respond"), const_cast<char *>("(JLjava/lang/String;)V"), reinterpret_cast<void *>(native_respond)},
		{const_cast<char *>("copyFrame"), const_cast<char *>("(Ljava/nio/ByteBuffer;II)Ljava/lang/String;"), reinterpret_cast<void *>(native_copy_frame)},
		{const_cast<char *>("cancel"), const_cast<char *>("()V"), reinterpret_cast<void *>(native_cancel)},
		{const_cast<char *>("shutdown"), const_cast<char *>("()V"), reinterpret_cast<void *>(native_shutdown)},
	};
}

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *) {
	jni::SetVM(vm);
	JNIEnv *env = nullptr;
	if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK)
		return JNI_ERR;

	jclass cls = env->FindClass(native_engine_class);
	if (!cls) return JNI_ERR;
	if (env->RegisterNatives(cls, native_methods, sizeof(native_methods) / sizeof(native_methods[0])) != JNI_OK)
		return JNI_ERR;
	env->DeleteLocalRef(cls);
	return JNI_VERSION_1_6;
}
