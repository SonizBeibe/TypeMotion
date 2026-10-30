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

/// @file engine.cpp
/// @brief The native methods of app.typemotion.engine.NativeEngine
/// @ingroup android

#include "android.h"
#include "bridge.h"
#include "crash_report.h"
#include "jni_util.h"
#include "json_io.h"
#include "media.h"

#include "../ass_export_filter.h"
#include "../auto4_base.h"
#include "../auto4_lua_factory.h"
#include "../command/command.h"
#include "../export_fixstyle.h"
#include "../font_index.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../subtitles_provider_libass.h"
#include "../task_queue.h"
#include "../ui_hooks.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/path.h>
#include <libaegisub/util.h>

#include <atomic>
#include <clocale>
#include <cstdlib>
#include <future>
#include <thread>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace config {
	agi::Options *opt = nullptr;
	agi::MRUManager *mru = nullptr;
	agi::Path *path = nullptr;
	Automation4::AutoloadScriptManager *global_scripts = nullptr;
}

namespace {
	const char native_engine_class[] = "app/typemotion/engine/NativeEngine";

	std::thread engine_thread;
	std::atomic<bool> running{false};
	std::atomic<bool> stopping{false};
	std::unique_ptr<agi::Context> context;
	agi::fs::path storage_folder;

	/// Sends the core's log to logcat
	class LogcatEmitter final : public agi::log::Emitter {
	public:
		void log(agi::log::SinkMessage const& sm) override {
#ifdef __ANDROID__
			int priority = ANDROID_LOG_DEBUG;
			switch (sm.severity) {
				case agi::log::Exception: priority = ANDROID_LOG_ERROR; break;
				case agi::log::Assert: priority = ANDROID_LOG_ERROR; break;
				case agi::log::Warning: priority = ANDROID_LOG_WARN; break;
				case agi::log::Info: priority = ANDROID_LOG_INFO; break;
				case agi::log::Debug: priority = ANDROID_LOG_DEBUG; break;
			}
			__android_log_print(priority, "TypeMotion", "%s: %s", sm.section, sm.message.c_str());
#else
			fprintf(stderr, "%s: %s\n", sm.section, sm.message.c_str());
#endif
		}
	};

	void create_directory(agi::fs::path const& dir) {
		try {
			agi::fs::CreateDirectory(dir);
		}
		catch (agi::Exception const& e) {
			LOG_E("android/init") << "Could not create " << dir << ": " << e.GetMessage();
		}
	}

	/// Set up the core the way the desktop program's startup does, with the
	/// app's folders. Runs on the engine thread.
	void start(android::Folders const& folders, std::vector<std::string> const& font_dirs, std::string const& locale) {
		// The core finds its folders through these
		setenv("HOME", folders.user.c_str(), 1);
		setenv("TMPDIR", folders.cache.c_str(), 1);

		// Like the desktop program, use the phone's language for sorting and
		// changing case, in UTF-8. Apps don't get it from the environment.
		if (!locale.empty())
			setenv("LANG", (locale + ".UTF-8").c_str(), 1);
		agi::util::InitLocale();
		setlocale(LC_NUMERIC, "C");
		setlocale(LC_CTYPE, "C");

		agi::util::SetThreadName("TypeMotion engine");
		task_queue::InitDispatch();

		agi::log::log = new agi::log::LogSink;
		agi::log::log->Subscribe(std::make_unique<LogcatEmitter>());

		// The app is in Spanish, as the user's desktop program is, so the
		// core's commands and messages are too
		android::LoadTranslations(agi::fs::path(folders.data) / "locale" / "es.po");

		agi::fs::path storage(folders.storage);
		storage_folder = storage;
		create_directory(folders.user);
		create_directory(folders.cache);
		create_directory(storage / "automation/autoload");
		create_directory(storage / "automation/include");
		create_directory(storage / "fonts");

		config::path = new agi::Path;
		config::path->SetToken("?data", agi::fs::path(folders.data));
		config::path->SetToken("?user", agi::fs::path(folders.user));
		config::path->SetToken("?local", agi::fs::path(folders.user));
		config::path->SetToken("?temp", agi::fs::path(folders.cache));
		config::path->SetToken("?dictionary", agi::fs::path(folders.data) / "dictionaries");

		try {
			config::opt = new agi::Options(config::path->Decode("?user/config.json"), GET_DEFAULT_CONFIG(default_config));
		}
		catch (agi::Exception const& e) {
			LOG_E("config/init") << "Caught exception: " << e.GetMessage();
		}
		try {
			config::opt->ConfigUser();
		}
		catch (agi::Exception const& e) {
			ui::LogError("Configuration file is invalid. Error reported:\n" + e.GetMessage());
		}

		// Modules in the user's folder are found before the official ones,
		// like the desktop version's ?user folder
		OPT_SET("Path/Automation/Include")->SetString((storage / "automation/include/").string() + "|?user/automation/include/|?data/automation/include/");

		// Before the scripts, which add their macros to the commands
		cmd::init_builtin_commands();

		config::mru = new agi::MRUManager(config::path->Decode("?user/mru.json"), GET_DEFAULT_CONFIG(default_mru), config::opt);

		// The user's fonts first, so that they win over system fonts with the same name
		fonts::AddDirectory(storage / "fonts");
		for (auto const& dir : font_dirs)
			fonts::AddDirectory(agi::fs::path(dir));

		Automation4::ScriptFactory::Register(std::make_unique<Automation4::LuaScriptFactory>());
		// As the desktop program's startup does; the subtitle renderer of
		// the video needs it
		libass::CacheFonts();
		// The official scripts come with the app; the user's own scripts are
		// loaded from the TypeMotion folder as they are
		config::global_scripts = new Automation4::AutoloadScriptManager(
			OPT_GET("Path/Automation/Autoload")->GetString(),
			(storage / "automation/autoload/").string());

		AssExportFilterChain::Register(std::make_unique<AssFixStylesFilter>());

		context = std::make_unique<agi::Context>();
		android::ConnectEvents(context.get());
	}

	void stop() {
		android::DisconnectEvents();
		context.reset();
		delete config::global_scripts;
		config::global_scripts = nullptr;
		cmd::clear();
		if (config::opt) config::opt->Flush();
		delete config::mru;
		config::mru = nullptr;
		delete config::opt;
		config::opt = nullptr;
		delete config::path;
		config::path = nullptr;
	}

	json::Object error_result(std::string message) {
		return json_io::Obj("error", std::move(message));
	}

	jstring native_init(JNIEnv *env, jclass, jstring jconfig, jobject callbacks) {
		if (running)
			return jni::ToJava(env, json_io::Write(error_result("The engine is already running")));

		auto config = json_io::ReadObject(jni::FromJava(env, jconfig));

		// First of all, so that a crash while starting is written down too
		auto crash_report = json_io::GetString(config, "crash_report");
		if (!crash_report.empty())
			android::crash::Install(crash_report, json_io::GetString(config, "crash_header"));

		android::Folders folders;
		folders.data = json_io::GetString(config, "data_dir");
		folders.user = json_io::GetString(config, "user_dir");
		folders.cache = json_io::GetString(config, "cache_dir");
		folders.storage = json_io::GetString(config, "storage_dir");
		if (folders.data.empty() || folders.user.empty() || folders.cache.empty() || folders.storage.empty())
			return jni::ToJava(env, json_io::Write(error_result("data_dir, user_dir, cache_dir and storage_dir are required")));

		// The phone's language, such as es_MX
		auto locale = json_io::GetString(config, "locale");

		std::vector<std::string> font_dirs;
		if (auto dirs = json_io::GetArray(config, "font_dirs")) {
			for (auto const& dir : *dirs) {
				if (json_io::TypeOf(dir) == json_io::Type::String)
					font_dirs.push_back(static_cast<json::String const&>(dir));
			}
		}

		bridge::SetCallbacks(env, callbacks);

		std::promise<std::string> started;
		auto started_future = started.get_future();
		stopping = false;
		running = true;
		engine_thread = std::thread([&] {
			std::string error;
			try {
				start(folders, font_dirs, locale);
			}
			catch (agi::Exception const& e) {
				error = e.GetMessage();
			}
			catch (std::exception const& e) {
				error = e.what();
			}
			if (!error.empty()) {
				stop();
				started.set_value(error);
				return;
			}

			started.set_value("");
			task_queue::Run([] { return stopping.load(); });
			stop();
		});

		auto error = started_future.get();
		if (!error.empty()) {
			engine_thread.join();
			running = false;
			return jni::ToJava(env, json_io::Write(error_result(error)));
		}
		return jni::ToJava(env, "{}");
	}

	jstring native_call(JNIEnv *env, jclass, jstring jmethod, jstring jargs) {
		if (!running || stopping)
			return jni::ToJava(env, json_io::Write(error_result("The engine isn't running")));

		auto method = jni::FromJava(env, jmethod);
		auto args = json_io::ReadObject(jni::FromJava(env, jargs));
		json::Object result;
		agi::dispatch::Main().Sync([&] {
			try {
				result = android::CallMethod(context.get(), method, args);
			}
			catch (agi::Exception const& e) {
				result = error_result(e.GetMessage());
			}
			catch (std::exception const& e) {
				result = error_result(e.what());
			}
		});
		return jni::ToJava(env, json_io::Write(std::move(result)));
	}

	void native_respond(JNIEnv *env, jclass, jlong id, jstring answer) {
		bridge::Respond(id, jni::FromJava(env, answer));
	}

	jstring native_copy_frame(JNIEnv *env, jclass, jobject buffer, jint max_width, jint max_height) {
		void *dst = buffer ? env->GetDirectBufferAddress(buffer) : nullptr;
		jlong capacity = buffer ? env->GetDirectBufferCapacity(buffer) : 0;
		auto size = android::media::CopyFrame(dst, capacity > 0 ? static_cast<size_t>(capacity) : 0, max_width, max_height);
		return jni::ToJava(env, json_io::Write(json_io::Obj(
			"width", size.width,
			"height", size.height,
			"serial", size.serial,
			"copied", dst && size.width > 0 && static_cast<size_t>(capacity) >= static_cast<size_t>(size.width) * size.height * 4
		)));
	}

	void native_cancel(JNIEnv *, jclass) {
		android::CancelScript();
	}

	void native_shutdown(JNIEnv *, jclass) {
		if (!running) return;
		bridge::Shutdown();
		android::CancelScript();
		stopping = true;
		task_queue::Wake();
		engine_thread.join();
		running = false;
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

namespace android {
	agi::fs::path const& StorageFolder() {
		return storage_folder;
	}
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
