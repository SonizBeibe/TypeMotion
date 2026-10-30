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

/// @file headless_ui.cpp
/// @brief The user interface functions the core needs, without an interface
///
/// It answers every question the core asks the way that changes the least
/// (no to saving, no to loading scripts a file asks for, the first choice of a
/// list, the first button of a dialog) and prints messages to stderr. It
/// is used to test the core, and shows what a frontend has to implement.

#include "../ass_file.h"
#include "../async_video_provider.h"
#include "../auto4_base.h"
#include "../auto4_lua.h"
#include "../auto4_lua_dialog.h"
#include "../command/command.h"
#include "../dialogs.h"
#include "../fold_controller.h"
#include "../i18n.h"
#include "../include/aegisub/context.h"
#include "../initial_line_state.h"
#include "../options.h"
#include "../project.h"
#include "../search_replace_engine.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../task_queue.h"
#include "../text_selection_controller.h"
#include "../ui_hooks.h"

#include <libaegisub/background_runner.h>
#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/path.h>
#include <libaegisub/string.h>

#include <atomic>
#include <deque>
#include <iostream>
#include <thread>

namespace {
	/// Clipboard for scripts which copy text and then paste it
	std::string clipboard;
	int busy_count = 0;

	void print(std::string_view caption, std::string_view message) {
		if (caption.empty())
			std::cerr << message << '\n';
		else
			std::cerr << '[' << caption << "] " << message << '\n';
	}

	/// Runs tasks on a worker thread while the calling thread runs the
	/// tasks the worker queues for the main thread
	class Progress final : public agi::BackgroundRunner, public agi::ProgressSink {
		std::string title;

	public:
		Progress(std::string title) : title(std::move(title)) { }

		std::string const& GetTitle() const { return title; }

		void Run(std::function<void(agi::ProgressSink *)> task) override {
			std::exception_ptr error;
			std::atomic<bool> done{false};
			std::thread worker([&] {
				try {
					task(this);
				}
				catch (...) {
					error = std::current_exception();
				}
				done = true;
				task_queue::Wake();
			});
			task_queue::Run([&] { return done.load(); });
			worker.join();
			if (error)
				std::rethrow_exception(error);
		}

		void SetIndeterminate() override { }
		void SetTitle(std::string const& new_title) override { title = new_title; }
		void SetMessage(std::string const& msg) override { print(title, msg); }
		void SetProgress(int64_t, int64_t) override { }
		void Log(std::string const& str) override { std::cerr << str; }
		bool IsCancelled() override { return false; }
	};

	/// Progress runners which belong to a window, which lives as long as
	/// the program does
	std::deque<Progress> window_progress;

	class VideoListener final : public AsyncVideoProviderListener {
		void FrameReady(std::shared_ptr<VideoFrame>, double) override { }
		void VideoError(std::string const& message) override { print("Video", message); }
		void SubtitlesError(std::string const& message) override { print("Subtitles", message); }
	};

	class HeadlessTimer final : public ui::Timer {
		// Nothing runs for long enough to need an autosave
		void Start(int) override { }
		void Stop() override { }
	};
}

namespace agi::i18n {
	std::string Translate(const char *message) {
		return message;
	}

	std::string TranslateContext(const char *, const char *message) {
		return message;
	}

	std::string TranslatePlural(const char *singular, const char *plural, int n) {
		return n == 1 ? singular : plural;
	}
}

namespace Automation4 {
	void ProgressSink::ShowDialog(ScriptDialog *config_dialog) {
		auto dialog = dynamic_cast<LuaDialog *>(config_dialog);
		if (!dialog) return;

		using Kind = LuaDialogControl::Kind;
		for (auto const& control : dialog->GetControls()) {
			std::string value;
			switch (control->GetKind()) {
				case Kind::Label: value = static_cast<LuaControl::Label&>(*control).label; break;
				case Kind::Checkbox: value = static_cast<LuaControl::Checkbox&>(*control).label; break;
				default: value = control->CanSerialiseValue() ? control->SerialiseValue() : ""; break;
			}
			print("Dialog", agi::Str(control->name.empty() ? "-" : control->name, " = ", value));
		}

		if (!dialog->UsesButtons()) return;
		auto const& buttons = dialog->GetButtons();
		for (size_t i = 0; i < buttons.size(); ++i) {
			if (buttons[i].first != LuaDialog::BUTTON_CANCEL) {
				print("Dialog", "pressing " + (buttons[i].second.empty() ? std::string("OK") : buttons[i].second));
				dialog->SetButtonPushed(i);
				return;
			}
		}
	}
}

namespace ui {
	int ShowMessage(std::string const& message, std::string const& caption, long style, wxWindow *) {
		print(caption, message);
		if (style & NO) return NO;
		if (style & OK) return OK;
		return CANCEL;
	}

	void ShowMessage(std::string const& message) {
		print("", message);
	}

	void LogWarning(std::string const& message) {
		print("Warning", message);
	}

	void LogError(std::string const& message) {
		print("Error", message);
	}

	bool ConfirmLoadingScripts(std::string const& message, std::string const& caption) {
		print(caption, message + "\n(not loading them)");
		return false;
	}

	int GetSingleChoiceIndex(std::string const& message, std::string const& caption, std::vector<std::string> const& choices) {
		if (choices.empty()) return -1;
		print(caption, message + "\n(choosing " + choices[0] + ")");
		return 0;
	}

	std::unique_ptr<agi::BackgroundRunner> CreateProgress(wxWindow *, std::string const& title, std::string const&) {
		return std::make_unique<Progress>(title);
	}

	wxWindow *GetProgressWindow(agi::BackgroundRunner *) {
		return nullptr;
	}

	std::string GetProgressTitle(agi::BackgroundRunner *progress) {
		return static_cast<Progress *>(progress)->GetTitle();
	}

	std::optional<std::vector<std::string>> ShowOpenFileDialog(std::string const& message, std::string const&, std::string const&, std::string const&, bool, bool) {
		print("Open file", message + "\n(cancelled)");
		return std::nullopt;
	}

	std::optional<std::string> ShowSaveFileDialog(wxWindow *, std::string const& message, std::string const&, std::string const&, std::string const&, bool) {
		print("Save file", message + "\n(cancelled)");
		return std::nullopt;
	}

	std::string GetClipboardText() {
		return clipboard;
	}

	bool SetClipboardText(std::string const& text) {
		clipboard = text;
		return true;
	}

	std::optional<bool> ClipboardHasText() {
		return !clipboard.empty();
	}

	std::string GetSingleChoice(std::string const& message, std::string const& caption, std::vector<std::string> const& choices, wxWindow *, int, int) {
		int index = GetSingleChoiceIndex(message, caption, choices);
		return index >= 0 ? choices[index] : std::string();
	}

	bool HasFocusedTextEntry(agi::Context *) { return false; }
	void EditFocusedTextEntry(agi::Context *, TextEntryAction) { }

	bool IsCmdDown() { return false; }
	bool IsShiftDown() { return false; }

	void CloseWindow(agi::Context *) {
		print("Close", "(staying open)");
	}

	std::optional<FontSelection> GetFontFromUser(wxWindow *, FontSelection const& initial) {
		print("Font", initial.face + "\n(cancelled)");
		return std::nullopt;
	}

	bool ShowPlainTextImportDialog() {
		// Import with the settings in the options
		return true;
	}

	bool IsBusy() { return busy_count > 0; }
	void BeginBusyCursor() { ++busy_count; }
	void EndBusyCursor() { --busy_count; }

	bool IsMainThread() {
		return task_queue::IsMainThread();
	}

	bool HasStatusBar(const agi::Context *) {
		return false;
	}

	void StatusTimeout(agi::Context *, std::string const& text) {
		print("Status", text);
	}

	agi::BackgroundRunner *CreateWindowProgress(wxWindow *) {
		return &window_progress.emplace_back("");
	}

	std::unique_ptr<AsyncVideoProviderListener> CreateVideoListener(agi::Context *) {
		return std::make_unique<VideoListener>();
	}

	void UpdateVideoProperties(AssFile *, const AsyncVideoProvider *, wxWindow *) {
		// Keep the script's resolution and colour matrix as they are
	}

	void SetVideoZoom(agi::Context *, double) { }

	void ScrollGridTo(agi::Context *, int) { }

	void RunCommand(const char *name, agi::Context *c) {
		cmd::call(name, c);
	}

	std::unique_ptr<ui::Timer> CreateTimer(std::function<void()>) {
		return std::make_unique<HeadlessTimer>();
	}
}

// The dialogs the core's commands open
bool GetColorFromUser(wxWindow *, agi::Color original, bool, std::function<void (agi::Color)>) {
	print("Color", original.GetHexFormatted(true) + "\n(cancelled)");
	return false;
}

std::string PickAutosaveFile(wxWindow *) {
	print("Open autosave file", "(cancelled)");
	return "";
}

bool ShowPasteOverDialog(wxWindow *) {
	// Paste the fields chosen in the options
	return true;
}

bool PromptForResampleSettings(agi::Context *, ResampleSettings &) {
	print("Resample Resolution", "(cancelled)");
	return false;
}

namespace {
	void show_dialog(std::string_view name) {
		print("Dialog", std::string(name) + " (not available without an interface)");
	}
}

void ShowAttachmentsDialog(wxWindow *, AssFile *) { show_dialog("attachments"); }
void ShowAutomationDialog(agi::Context *) { show_dialog("automation"); }
void ShowExportDialog(agi::Context *) { show_dialog("export"); }
void ShowFontsCollectorDialog(agi::Context *) { show_dialog("fonts_collector"); }
void ShowKanjiTimerDialog(agi::Context *) { show_dialog("kanji_timer"); }
void ShowPropertiesDialog(agi::Context *) { show_dialog("properties"); }
void ShowSearchReplaceDialog(agi::Context *, bool replace) { show_dialog(replace ? "search_replace (replace)" : "search_replace"); }
void ShowSelectLinesDialog(agi::Context *) { show_dialog("select_lines"); }
void ShowShiftTimesDialog(agi::Context *) { show_dialog("shift_times"); }
void ShowSpellcheckerDialog(agi::Context *) { show_dialog("spellchecker"); }
void ShowStyleManagerDialog(agi::Context *) { show_dialog("style_manager"); }
void ShowTimingProcessorDialog(agi::Context *) { show_dialog("timing_processor"); }
