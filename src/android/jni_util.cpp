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

/// @file jni_util.cpp
/// @brief Helpers for talking to the Java virtual machine
/// @ingroup android

#include "jni_util.h"

#include <cstdint>
#include <vector>

namespace {
	JavaVM *java_vm = nullptr;

	/// Detaches threads attached by Env() when they end
	struct Attachment {
		JNIEnv *env = nullptr;
		bool attached = false;

		~Attachment() {
			if (attached)
				java_vm->DetachCurrentThread();
		}
	};

	thread_local Attachment attachment;

	void append_utf8(std::string& out, uint32_t cp) {
		if (cp < 0x80)
			out += static_cast<char>(cp);
		else if (cp < 0x800) {
			out += static_cast<char>(0xC0 | (cp >> 6));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else if (cp < 0x10000) {
			out += static_cast<char>(0xE0 | (cp >> 12));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
		else {
			out += static_cast<char>(0xF0 | (cp >> 18));
			out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
			out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
			out += static_cast<char>(0x80 | (cp & 0x3F));
		}
	}
}

namespace jni {
	void SetVM(JavaVM *vm) {
		java_vm = vm;
	}

	JNIEnv *Env() {
		if (attachment.env)
			return attachment.env;

		JNIEnv *env = nullptr;
		if (java_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) == JNI_OK) {
			attachment.env = env;
			return env;
		}

		// As a daemon, since the core's worker threads live as long as the
		// program and mustn't keep Java from shutting down
#ifdef __ANDROID__
		java_vm->AttachCurrentThreadAsDaemon(&env, nullptr);
#else
		java_vm->AttachCurrentThreadAsDaemon(reinterpret_cast<void **>(&env), nullptr);
#endif
		attachment.env = env;
		attachment.attached = env != nullptr;
		return env;
	}

	// Java's own UTF-8 functions use a modified encoding which breaks
	// characters outside the BMP (such as emoji), so convert from UTF-16
	std::string FromJava(JNIEnv *env, jstring str) {
		if (!str) return {};

		jsize len = env->GetStringLength(str);
		const jchar *chars = env->GetStringChars(str, nullptr);
		std::string out;
		out.reserve(len);
		for (jsize i = 0; i < len; ++i) {
			uint32_t cp = chars[i];
			if (cp >= 0xD800 && cp < 0xDC00 && i + 1 < len && chars[i + 1] >= 0xDC00 && chars[i + 1] < 0xE000) {
				cp = 0x10000 + ((cp - 0xD800) << 10) + (chars[i + 1] - 0xDC00);
				++i;
			}
			else if (cp >= 0xD800 && cp < 0xE000)
				cp = 0xFFFD;
			append_utf8(out, cp);
		}
		env->ReleaseStringChars(str, chars);
		return out;
	}

	jstring ToJava(JNIEnv *env, std::string_view str) {
		std::vector<jchar> out;
		out.reserve(str.size());
		auto byte = [&](size_t i) { return static_cast<unsigned char>(str[i]); };
		for (size_t pos = 0; pos < str.size(); ) {
			uint32_t cp = byte(pos++);
			int extra = 0;
			if (cp >= 0xF8) { cp = 0xFFFD; }
			else if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
			else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
			else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
			else if (cp >= 0x80) { cp = 0xFFFD; }
			for (; extra > 0; --extra) {
				if (pos >= str.size() || (byte(pos) & 0xC0) != 0x80) {
					cp = 0xFFFD;
					break;
				}
				cp = (cp << 6) | (byte(pos++) & 0x3F);
			}

			if (cp >= 0x10000) {
				cp -= 0x10000;
				out.push_back(static_cast<jchar>(0xD800 + (cp >> 10)));
				out.push_back(static_cast<jchar>(0xDC00 + (cp & 0x3FF)));
			}
			else
				out.push_back(static_cast<jchar>(cp));
		}
		return env->NewString(out.data(), static_cast<jsize>(out.size()));
	}
}
