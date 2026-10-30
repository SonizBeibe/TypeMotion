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

/// @file android_ui.cpp
/// @brief The user interface functions the core needs, answered by the app
/// @ingroup android
///
/// Everything the desktop program shows a window for becomes a request to
/// the app (see bridge.h), which shows its own screen and sends the answer
/// back.

#include "android.h"
#include "bridge.h"
#include "json_io.h"
#include "media.h"

#include "../ass_file.h"
#include "../async_video_provider.h"
#include "../auto4_base.h"
#include "../auto4_lua.h"
#include "../auto4_lua_dialog.h"
#include "../command/command.h"
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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace {
	std::atomic<bool> cancel_requested{false};
	int busy_count = 0;

	json::Array to_array(std::vector<std::string> const& strings) {
		json::Array array;
		for (auto const& str : strings)
			array.emplace_back(str);
		return array;
	}

	/// Runs tasks on a worker thread while the engine thread keeps running
	/// the tasks queued for it, and tells the app how it's going
	class Progress final : public agi::BackgroundRunner, public agi::ProgressSink {
		std::string title;
		std::string message;
		int last_percent = -1;
		bool indeterminate = true;

		void update() {
			bridge::Event("progress", json_io::Obj(
				"state", "update",
				"title", title,
				"message", message,
				"indeterminate", indeterminate,
				"percent", static_cast<int64_t>(std::max(last_percent, 0))
			));
		}

	public:
		Progress(std::string title, std::string message = "")
		: title(std::move(title)), message(std::move(message)) { }

		std::string const& GetTitle() const { return title; }

		void Run(std::function<void(agi::ProgressSink *)> task) override {
			cancel_requested = false;
			bridge::Event("progress", json_io::Obj(
				"state", "start",
				"title", title,
				"message", message
			));

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

			bridge::Event("progress", json_io::Obj(
				"state", "end",
				"title", title,
				"cancelled", cancel_requested.load()
			));
			if (error)
				std::rethrow_exception(error);
		}

		void SetIndeterminate() override {
			indeterminate = true;
			update();
		}

		void SetTitle(std::string const& new_title) override {
			title = new_title;
			update();
		}

		void SetMessage(std::string const& msg) override {
			message = msg;
			update();
		}

		void SetProgress(int64_t cur, int64_t max) override {
			int percent = max > 0 ? static_cast<int>(std::clamp<int64_t>(cur * 100 / max, 0, 100)) : 0;
			// Scripts report progress for every line, which is far more
			// often than the app can draw it
			if (!indeterminate && percent == last_percent) return;
			indeterminate = false;
			last_percent = percent;
			update();
		}

		void Log(std::string const& str) override {
			bridge::Event("progress_log", json_io::Obj("text", str));
		}

		bool IsCancelled() override {
			return cancel_requested;
		}
	};

	/// Progress runners which belong to a window, which lives as long as
	/// the program does
	std::deque<Progress> window_progress;

	class VideoListener final : public AsyncVideoProviderListener {
		void FrameReady(std::shared_ptr<VideoFrame> frame, double) override {
			android::media::FrameReady(std::move(frame));
		}
		void VideoError(std::string const& message) override {
			bridge::Event("log", json_io::Obj("level", "error", "message", message));
		}
		void SubtitlesError(std::string const& message) override {
			bridge::Event("log", json_io::Obj("level", "warning", "message", message));
		}
	};

	/// A repeating timer which runs its function on the engine thread
	class EngineTimer final : public ui::Timer {
		std::function<void()> on_timer;
		/// Cleared when the timer is stopped, so that ticks still queued for
		/// the engine thread do nothing
		std::shared_ptr<std::atomic<bool>> active;
		std::mutex mutex;
		std::condition_variable cv;
		std::thread thread;
		bool running = false;

	public:
		EngineTimer(std::function<void()> on_timer) : on_timer(std::move(on_timer)) { }

		~EngineTimer() {
			Stop();
		}

		void Start(int interval) override {
			Stop();
			if (interval <= 0) return;
			running = true;
			active = std::make_shared<std::atomic<bool>>(true);
			thread = std::thread([this, interval, active = active, on_timer = on_timer] {
				std::unique_lock<std::mutex> lock(mutex);
				while (!cv.wait_for(lock, std::chrono::milliseconds(interval), [&] { return !running; })) {
					agi::dispatch::Main().Async([active, on_timer] {
						if (*active) on_timer();
					});
				}
			});
		}

		void Stop() override {
			if (active)
				*active = false;
			{
				std::lock_guard<std::mutex> lock(mutex);
				running = false;
			}
			cv.notify_all();
			if (thread.joinable())
				thread.join();
		}
	};

	const char *button_name(int id) {
		using Automation4::LuaDialog;
		switch (id) {
			case LuaDialog::BUTTON_OK: return "ok";
			case LuaDialog::BUTTON_YES: return "yes";
			case LuaDialog::BUTTON_SAVE: return "save";
			case LuaDialog::BUTTON_APPLY: return "apply";
			case LuaDialog::BUTTON_CLOSE: return "close";
			case LuaDialog::BUTTON_NO: return "no";
			case LuaDialog::BUTTON_CANCEL: return "cancel";
			case LuaDialog::BUTTON_HELP: return "help";
			case LuaDialog::BUTTON_CONTEXT_HELP: return "context_help";
			default: return "custom";
		}
	}

	const char *kind_name(Automation4::LuaDialogControl::Kind kind) {
		using Kind = Automation4::LuaDialogControl::Kind;
		switch (kind) {
			case Kind::Label: return "label";
			case Kind::Edit: return "edit";
			case Kind::Textbox: return "textbox";
			case Kind::IntEdit: return "intedit";
			case Kind::FloatEdit: return "floatedit";
			case Kind::Dropdown: return "dropdown";
			case Kind::Checkbox: return "checkbox";
			case Kind::Color: return "color";
			case Kind::ColorAlpha: return "coloralpha";
		}
		return "label";
	}

	json::Object control_to_json(Automation4::LuaDialogControl const& control) {
		using namespace Automation4;
		using Kind = LuaDialogControl::Kind;

		auto obj = json_io::Obj(
			"class", kind_name(control.GetKind()),
			"name", control.name,
			"hint", control.hint,
			"x", static_cast<int64_t>(control.x),
			"y", static_cast<int64_t>(control.y),
			"width", static_cast<int64_t>(control.width),
			"height", static_cast<int64_t>(control.height));

		switch (control.GetKind()) {
			case Kind::Label:
				obj["label"] = static_cast<LuaControl::Label const&>(control).label;
				break;
			case Kind::Edit:
			case Kind::Textbox:
				obj["value"] = static_cast<LuaControl::Edit const&>(control).text;
				break;
			case Kind::IntEdit: {
				auto& c = static_cast<LuaControl::IntEdit const&>(control);
				obj["value"] = static_cast<int64_t>(c.value);
				obj["min"] = static_cast<int64_t>(c.min);
				obj["max"] = static_cast<int64_t>(c.max);
				break;
			}
			case Kind::FloatEdit: {
				auto& c = static_cast<LuaControl::FloatEdit const&>(control);
				obj["value"] = c.value;
				obj["min"] = c.min;
				obj["max"] = c.max;
				obj["step"] = c.step;
				break;
			}
			case Kind::Dropdown: {
				auto& c = static_cast<LuaControl::Dropdown const&>(control);
				obj["value"] = c.value;
				obj["items"] = to_array(c.items);
				break;
			}
			case Kind::Checkbox: {
				auto& c = static_cast<LuaControl::Checkbox const&>(control);
				obj["label"] = c.label;
				obj["value"] = c.value;
				break;
			}
			case Kind::Color:
			case Kind::ColorAlpha: {
				auto& c = static_cast<LuaControl::Color const&>(control);
				obj["value"] = c.color.GetHexFormatted(c.alpha);
				obj["ass"] = c.color.GetAssStyleFormatted();
				break;
			}
		}
		return obj;
	}

	/// Store a value the user entered in a control, the way the desktop
	/// version's widgets do
	void control_from_json(Automation4::LuaDialogControl& control, json::Object const& values) {
		using namespace Automation4;
		using Kind = LuaDialogControl::Kind;

		if (values.find(control.name) == values.end()) return;

		switch (control.GetKind()) {
			case Kind::Label:
				break;
			case Kind::Edit:
			case Kind::Textbox: {
				auto& c = static_cast<LuaControl::Edit&>(control);
				c.text = json_io::GetString(values, control.name, c.text);
				break;
			}
			case Kind::IntEdit: {
				auto& c = static_cast<LuaControl::IntEdit&>(control);
				c.value = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(values, control.name, c.value), c.min, c.max));
				break;
			}
			case Kind::FloatEdit: {
				auto& c = static_cast<LuaControl::FloatEdit&>(control);
				c.value = std::clamp(json_io::GetDouble(values, control.name, c.value), c.min, c.max);
				break;
			}
			case Kind::Dropdown: {
				auto& c = static_cast<LuaControl::Dropdown&>(control);
				c.value = json_io::GetString(values, control.name, c.value);
				break;
			}
			case Kind::Checkbox: {
				auto& c = static_cast<LuaControl::Checkbox&>(control);
				c.value = json_io::GetBool(values, control.name, c.value);
				break;
			}
			case Kind::Color:
			case Kind::ColorAlpha: {
				auto& c = static_cast<LuaControl::Color&>(control);
				auto value = json_io::GetString(values, control.name);
				if (!value.empty())
					c.color = std::string_view(value);
				break;
			}
		}
	}

	/// The button the desktop version presses when the dialog is closed
	/// without pressing one (with Escape), or -1 if there is none
	int escape_button(std::vector<std::pair<int, std::string>> const& buttons) {
		using Automation4::LuaDialog;
		int escape = -1;
		for (size_t i = 0; i < buttons.size(); ++i) {
			if (buttons[i].first == LuaDialog::BUTTON_CLOSE || buttons[i].first == LuaDialog::BUTTON_NO)
				escape = static_cast<int>(i);
		}
		if (escape != -1) return escape;
		for (size_t i = 0; i < buttons.size(); ++i) {
			if (buttons[i].first == LuaDialog::BUTTON_CANCEL)
				return static_cast<int>(i);
		}
		return -1;
	}

	int answer_button(json::Object const& answer, long style) {
		auto button = json_io::GetString(answer, "button");
		if (button == "yes") return ui::YES;
		if (button == "no") return ui::NO;
		if (button == "ok") return ui::OK;
		if (button == "cancel") return ui::CANCEL;
		// Closed without pressing a button
		if (style & ui::CANCEL) return ui::CANCEL;
		if (style & ui::NO) return ui::NO;
		return ui::OK;
	}
}

namespace android {
	void CancelScript() {
		cancel_requested = true;
	}
}

namespace Automation4 {
	void ProgressSink::ShowDialog(ScriptDialog *config_dialog) {
		auto dialog = dynamic_cast<LuaDialog *>(config_dialog);
		if (!dialog) return;

		json::Array controls;
		for (auto const& control : dialog->GetControls())
			controls.push_back(control_to_json(*control));

		json::Array buttons;
		if (dialog->UsesButtons()) {
			for (auto const& button : dialog->GetButtons()) {
				int id = button.first;
				buttons.push_back(json_io::Obj(
					"id", button_name(id),
					"label", button.second,
					"default", id == LuaDialog::BUTTON_OK || id == LuaDialog::BUTTON_YES || id == LuaDialog::BUTTON_SAVE
				));
			}
		}

		auto answer = bridge::Request("dialog", json_io::Obj(
			"title", bsr->GetTitle(),
			"controls", std::move(controls),
			"buttons", std::move(buttons)
		));

		int button = static_cast<int>(json_io::GetInt(answer, "button", -1));
		if (button < 0 || button >= static_cast<int>(dialog->GetButtons().size()))
			button = escape_button(dialog->GetButtons());
		// Closing the dialog without a button keeps the values the script gave
		if (button < 0) return;

		if (auto values = json_io::GetObject(answer, "values")) {
			for (auto const& control : dialog->GetControls())
				control_from_json(*control, *values);
		}
		dialog->SetButtonPushed(button);
	}
}

namespace ui {
	int ShowMessage(std::string const& message, std::string const& caption, long style, wxWindow *) {
		auto answer = bridge::Request("message", json_io::Obj(
			"message", message,
			"caption", caption,
			"yes", (style & YES) != 0,
			"no", (style & NO) != 0,
			"ok", (style & OK) != 0 || !(style & (YES | NO)),
			"cancel", (style & CANCEL) != 0,
			"icon", style & ICON_ERROR ? "error" : style & ICON_WARNING ? "warning" : style & ICON_QUESTION ? "question" : "information"
		));
		return answer_button(answer, style);
	}

	void ShowMessage(std::string const& message) {
		ShowMessage(message, "TypeMotion", OK);
	}

	void LogWarning(std::string const& message) {
		bridge::Event("log", json_io::Obj("level", "warning", "message", message));
	}

	void LogError(std::string const& message) {
		bridge::Event("log", json_io::Obj("level", "error", "message", message));
	}

	bool ConfirmLoadingScripts(std::string const& message, std::string const& caption) {
		auto answer = bridge::Request("confirm_scripts", json_io::Obj(
			"message", message,
			"caption", caption
		));
		return json_io::GetBool(answer, "yes");
	}

	int GetSingleChoiceIndex(std::string const& message, std::string const& caption, std::vector<std::string> const& choices) {
		auto answer = bridge::Request("choice", json_io::Obj(
			"message", message,
			"caption", caption,
			"choices", to_array(choices)
		));
		int index = static_cast<int>(json_io::GetInt(answer, "index", -1));
		return index >= 0 && index < static_cast<int>(choices.size()) ? index : -1;
	}

	std::unique_ptr<agi::BackgroundRunner> CreateProgress(wxWindow *, std::string const& title, std::string const& message) {
		return std::make_unique<Progress>(title, message);
	}

	wxWindow *GetProgressWindow(agi::BackgroundRunner *) {
		return nullptr;
	}

	std::string GetProgressTitle(agi::BackgroundRunner *progress) {
		return static_cast<Progress *>(progress)->GetTitle();
	}

	std::optional<std::vector<std::string>> ShowOpenFileDialog(std::string const& message, std::string const& dir, std::string const& file, std::string const& wildcard, bool multiple, bool must_exist) {
		auto answer = bridge::Request("open_file", json_io::Obj(
			"message", message,
			"dir", dir,
			"file", file,
			"wildcard", wildcard,
			"multiple", multiple,
			"must_exist", must_exist
		));
		auto paths = json_io::GetArray(answer, "paths");
		if (!paths || paths->empty()) return std::nullopt;

		std::vector<std::string> result;
		for (auto const& path : *paths) {
			if (json_io::TypeOf(path) == json_io::Type::String)
				result.push_back(static_cast<json::String const&>(path));
		}
		if (result.empty()) return std::nullopt;
		return result;
	}

	std::optional<std::string> ShowSaveFileDialog(wxWindow *, std::string const& message, std::string const& dir, std::string const& file, std::string const& wildcard, bool prompt_overwrite) {
		auto answer = bridge::Request("save_file", json_io::Obj(
			"message", message,
			"dir", dir,
			"file", file,
			"wildcard", wildcard,
			"prompt_overwrite", prompt_overwrite
		));
		auto path = json_io::GetString(answer, "path");
		if (path.empty()) return std::nullopt;
		return path;
	}

	std::string GetClipboardText() {
		return json_io::GetString(bridge::Request("clipboard_get"), "text");
	}

	bool SetClipboardText(std::string const& text) {
		bridge::Event("clipboard_set", json_io::Obj("text", text));
		return true;
	}

	std::optional<bool> ClipboardHasText() {
		return !GetClipboardText().empty();
	}

	std::string GetSingleChoice(std::string const& message, std::string const& caption, std::vector<std::string> const& choices, wxWindow *, int, int) {
		int index = GetSingleChoiceIndex(message, caption, choices);
		return index >= 0 ? choices[index] : std::string();
	}

	// The app's text boxes copy and paste by themselves, so the commands only
	// work on lines
	bool HasFocusedTextEntry(agi::Context *) { return false; }
	void EditFocusedTextEntry(agi::Context *, TextEntryAction) { }

	// Phones have no keys to hold down while tapping
	bool IsCmdDown() { return false; }
	bool IsShiftDown() { return false; }

	void CloseWindow(agi::Context *) {
		bridge::Event("close_window");
	}

	std::optional<FontSelection> GetFontFromUser(wxWindow *, FontSelection const& initial) {
		auto answer = bridge::Request("font", json_io::Obj(
			"face", initial.face,
			"size", static_cast<int64_t>(initial.size),
			"bold", initial.bold,
			"italic", initial.italic,
			"underline", initial.underline
		));
		if (!json_io::GetBool(answer, "ok"))
			return std::nullopt;

		FontSelection font;
		font.face = json_io::GetString(answer, "face", initial.face);
		font.size = static_cast<int>(json_io::GetInt(answer, "size", initial.size));
		font.bold = json_io::GetBool(answer, "bold", initial.bold);
		font.italic = json_io::GetBool(answer, "italic", initial.italic);
		font.underline = json_io::GetBool(answer, "underline", initial.underline);
		// Like wxGetFontFromUser, an invalid font counts as cancelling
		if (font.face.empty() || font.size <= 0)
			return std::nullopt;
		return font;
	}

	bool ShowPlainTextImportDialog() {
		auto answer = bridge::Request("plain_text_import", json_io::Obj(
			"actor_separator", OPT_GET("Tool/Import/Text/Actor Separator")->GetString(),
			"comment_starter", OPT_GET("Tool/Import/Text/Comment Starter")->GetString(),
			"include_blank", OPT_GET("Tool/Import/Text/Include Blank")->GetBool()
		));
		if (!json_io::GetBool(answer, "ok"))
			return false;

		OPT_SET("Tool/Import/Text/Actor Separator")->SetString(json_io::GetString(answer, "actor_separator", OPT_GET("Tool/Import/Text/Actor Separator")->GetString()));
		OPT_SET("Tool/Import/Text/Comment Starter")->SetString(json_io::GetString(answer, "comment_starter", OPT_GET("Tool/Import/Text/Comment Starter")->GetString()));
		OPT_SET("Tool/Import/Text/Include Blank")->SetBool(json_io::GetBool(answer, "include_blank", OPT_GET("Tool/Import/Text/Include Blank")->GetBool()));
		return true;
	}

	bool IsBusy() { return busy_count > 0; }

	void BeginBusyCursor() {
		if (busy_count++ == 0)
			bridge::Event("busy", json_io::Obj("busy", true));
	}

	void EndBusyCursor() {
		if (--busy_count == 0)
			bridge::Event("busy", json_io::Obj("busy", false));
	}

	bool IsMainThread() {
		return task_queue::IsMainThread();
	}

	bool HasStatusBar(const agi::Context *) {
		return true;
	}

	void StatusTimeout(agi::Context *, std::string const& text) {
		bridge::Event("status", json_io::Obj("text", text));
	}

	agi::BackgroundRunner *CreateWindowProgress(wxWindow *) {
		return &window_progress.emplace_back("");
	}

	std::unique_ptr<AsyncVideoProviderListener> CreateVideoListener(agi::Context *) {
		return std::make_unique<VideoListener>();
	}

	void UpdateVideoProperties(AssFile *, const AsyncVideoProvider *, wxWindow *) {
		// Keep the script's resolution and colour matrix as they are until
		// the app shows video
	}

	void SetVideoZoom(agi::Context *, double) { }

	void ScrollGridTo(agi::Context *, int row) {
		bridge::Event("scroll_to", json_io::Obj("row", static_cast<int64_t>(row)));
	}

	void RunCommand(const char *name, agi::Context *c) {
		cmd::call(name, c);
	}

	std::unique_ptr<ui::Timer> CreateTimer(std::function<void()> on_timer) {
		return std::make_unique<EngineTimer>(std::move(on_timer));
	}
}
