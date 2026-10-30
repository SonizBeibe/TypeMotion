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

/// @file crash_report.cpp
/// @brief Writes down why the engine crashed, for the app to show
/// @ingroup android

#include "crash_report.h"

#include <libaegisub/exception.h>

#include <atomic>
#include <cinttypes>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <typeinfo>

#include <cxxabi.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

namespace {
	char report_path[1024];
	char report_header[1024];
	bool installed = false;
	/// Only the first of a crash's signals and aborts is written down
	std::atomic<bool> reported{false};

	std::terminate_handler previous_terminate = nullptr;
	const int fatal_signals[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
	constexpr size_t signal_count = sizeof(fatal_signals) / sizeof(fatal_signals[0]);
	struct sigaction previous_actions[signal_count];

	/// The report file, written with write() so that a signal handler can
	/// use it
	class Report {
		int fd;
	public:
		Report() : fd(open(report_path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600)) { }
		~Report() { if (fd >= 0) close(fd); }
		Report(Report const&) = delete;
		Report& operator=(Report const&) = delete;

		void put(const char *text) {
			if (fd < 0) return;
			size_t left = strlen(text);
			while (left > 0) {
				ssize_t written = write(fd, text, left);
				if (written <= 0) return;
				text += written;
				left -= static_cast<size_t>(written);
			}
		}

		__attribute__((format(printf, 2, 3)))
		void print(const char *format, ...) {
			char buffer[1024];
			va_list args;
			va_start(args, format);
			vsnprintf(buffer, sizeof(buffer), format, args);
			va_end(args);
			put(buffer);
		}
	};

	struct Frames {
		uintptr_t pcs[64];
		size_t count = 0;
	};

	_Unwind_Reason_Code add_frame(_Unwind_Context *context, void *arg) {
		auto frames = static_cast<Frames *>(arg);
		uintptr_t pc = _Unwind_GetIP(context);
		if (pc == 0) return _URC_END_OF_STACK;
		frames->pcs[frames->count++] = pc;
		return frames->count < sizeof(frames->pcs) / sizeof(frames->pcs[0]) ? _URC_NO_REASON : _URC_END_OF_STACK;
	}

	/// One line per frame, with the offset in its library so that it can be
	/// looked up in the build's unstripped libtypemotion.so
	void write_frame(Report& report, size_t index, uintptr_t pc) {
		Dl_info info{};
		if (!dladdr(reinterpret_cast<void *>(pc), &info) || !info.dli_fname) {
			report.print("#%02zu pc %016" PRIxPTR "\n", index, pc);
			return;
		}
		const char *library = strrchr(info.dli_fname, '/');
		library = library ? library + 1 : info.dli_fname;
		uintptr_t offset = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
		if (info.dli_sname)
			report.print("#%02zu pc %08" PRIxPTR " %s (%s+%" PRIuPTR ")\n", index, offset, library,
				info.dli_sname, pc - reinterpret_cast<uintptr_t>(info.dli_saddr));
		else
			report.print("#%02zu pc %08" PRIxPTR " %s\n", index, offset, library);
	}

	void write_backtrace(Report& report, uintptr_t fault_pc) {
		report.put("\nPila:\n");
		size_t index = 0;
		if (fault_pc)
			write_frame(report, index++, fault_pc);
		Frames frames;
		_Unwind_Backtrace(add_frame, &frames);
		for (size_t i = 0; i < frames.count; ++i)
			write_frame(report, index++, frames.pcs[i]);
	}

	void write_header(Report& report) {
		report.put(report_header);
#if defined(__aarch64__)
		report.put("Motor: arm64-v8a\n\n");
#elif defined(__x86_64__)
		report.put("Motor: x86_64\n\n");
#else
		report.put("\n");
#endif
	}

	/// What std::terminate was called for
	void write_exception(Report& report) {
		std::type_info *type = abi::__cxa_current_exception_type();
		if (!type) {
			// No C++ exception, or one from another language: LuaJIT throws
			// its errors through C++ code as those
			report.put("Se llamó a std::terminate sin una excepción de C++ (puede ser un error de Lua que nadie atrapó)\n");
			return;
		}

		int status = 0;
		char *demangled = abi::__cxa_demangle(type->name(), nullptr, nullptr, &status);
		const char *name = status == 0 && demangled ? demangled : type->name();
		try {
			throw;
		}
		catch (agi::Exception const& e) {
			report.print("Excepción sin atrapar %s: %s\n", name, e.GetMessage().c_str());
		}
		catch (std::exception const& e) {
			report.print("Excepción sin atrapar %s: %s\n", name, e.what());
		}
		catch (...) {
			report.print("Excepción sin atrapar %s\n", name);
		}
		free(demangled);
	}

	[[noreturn]] void on_terminate() {
		if (!reported.exchange(true)) {
			Report report;
			write_header(report);
			write_exception(report);
			write_backtrace(report, 0);
		}
		// The runtime's own handler writes its message to the log and aborts
		if (previous_terminate)
			previous_terminate();
		abort();
	}

	uintptr_t fault_pc(void *context) {
		auto uc = static_cast<ucontext_t *>(context);
		if (!uc) return 0;
#if defined(__aarch64__)
		return static_cast<uintptr_t>(uc->uc_mcontext.pc);
#elif defined(__x86_64__)
		return static_cast<uintptr_t>(uc->uc_mcontext.gregs[REG_RIP]);
#else
		return 0;
#endif
	}

	const char *signal_name(int signal) {
		switch (signal) {
			case SIGSEGV: return "SIGSEGV";
			case SIGBUS: return "SIGBUS";
			case SIGILL: return "SIGILL";
			case SIGFPE: return "SIGFPE";
			case SIGABRT: return "SIGABRT";
			default: return "?";
		}
	}

	void on_signal(int signal, siginfo_t *info, void *context) {
		if (!reported.exchange(true)) {
			Report report;
			write_header(report);
			report.print("Señal %d (%s), código %d, dirección %p\n", signal, signal_name(signal),
				info ? info->si_code : 0, info ? info->si_addr : nullptr);
			write_backtrace(report, fault_pc(context));
		}

		// Hand the signal to the handler from before, which writes Android's
		// own report
		size_t i = 0;
		while (i < signal_count && fatal_signals[i] != signal) ++i;
		if (i == signal_count) return;
		struct sigaction const& previous = previous_actions[i];
		sigaction(signal, &previous, nullptr);
		if (previous.sa_flags & SA_SIGINFO) {
			if (previous.sa_sigaction) {
				previous.sa_sigaction(signal, info, context);
				return;
			}
		}
		else if (previous.sa_handler != SIG_DFL && previous.sa_handler != SIG_IGN) {
			previous.sa_handler(signal);
			return;
		}
		// With the default action, a fault happens again on return; a signal
		// which was sent has to be sent again
		if (info && info->si_code <= 0)
			raise(signal);
	}
}

namespace android::crash {
	void Install(std::string const& path, std::string const& header) {
		snprintf(report_path, sizeof(report_path), "%s", path.c_str());
		snprintf(report_header, sizeof(report_header), "%s", header.c_str());
		if (installed) return;
		installed = true;

		previous_terminate = std::set_terminate(on_terminate);

		struct sigaction action{};
		action.sa_sigaction = on_signal;
		action.sa_flags = SA_SIGINFO | SA_ONSTACK;
		sigemptyset(&action.sa_mask);
		for (size_t i = 0; i < signal_count; ++i)
			sigaction(fatal_signals[i], &action, &previous_actions[i]);
	}
}
