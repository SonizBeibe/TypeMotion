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

/// @file edit_box.cpp
/// @brief The engine side of the app's edit box
/// @ingroup android

#include "edit_box.h"

#include "bridge.h"
#include "json_io.h"

#include "../ass_dialogue.h"
#include "../ass_file.h"
#include "../i18n.h"
#include "../include/aegisub/context.h"
#include "../initial_line_state.h"
#include "../options.h"
#include "../selection_controller.h"
#include "../text_selection_controller.h"
#include "../ui_hooks.h"

#include <libaegisub/character_count.h>
#include <libaegisub/exception.h>
#include <libaegisub/signal.h>

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace {
	using android::edit_box::Args;

	enum TimeField { TIME_START, TIME_END, TIME_DURATION };

	/// The app's text field, which follows the cursor the core sets
	class AppTextControl final : public TextSelectionController::Control {
		TextSelectionController *controller;

		void Send() {
			bridge::Event("text_selection", json_io::Obj(
				"start", static_cast<int64_t>(controller->GetSelectionStart()),
				"end", static_cast<int64_t>(controller->GetSelectionEnd()),
				"insertion", static_cast<int64_t>(controller->GetInsertionPoint())
			));
		}

	public:
		AppTextControl(TextSelectionController *controller) : controller(controller) { }
		void SetInsertionPoint(int) override { Send(); }
		void SetSelectionStart(int) override { Send(); }
		void SetSelectionEnd(int) override { Send(); }
	};

	json::Array to_array(std::vector<std::string> const& strings) {
		json::Array array;
		for (auto const& str : strings)
			array.emplace_back(str);
		return array;
	}

	/// The values of a field in all lines, sorted, as the desktop actor and
	/// effect boxes list them
	json::Array field_values(agi::Context *c, boost::flyweight<std::string> AssDialogue::*field) {
		std::set<std::string> values;
		for (auto const& line : c->ass->Events) {
			auto const& value = (line.*field).get();
			if (!value.empty())
				values.insert(value);
		}
		return to_array(std::vector<std::string>(values.begin(), values.end()));
	}

	class EditBox {
		agi::Context *c;
		/// The active line
		AssDialogue *line = nullptr;
		/// The text the app shows
		std::string text;

		int commit_id = -1;
		int last_time_commit_type = -1;
		/// The times the selected lines had before the time fields changed them
		std::map<AssDialogue *, std::pair<agi::Time, agi::Time>> initial_times;
		/// Starts a new undo step after 30 seconds without changes
		std::unique_ptr<ui::Timer> undo_timer;

		agi::signal::Connection file_changed_slot;
		std::vector<agi::signal::Connection> connections;

		void SendState(bool text_changed) {
			auto state = State();
			state["text_changed"] = text_changed;
			bridge::Event("edit_box", std::move(state));
		}

		/// Show new text, keeping the cursor after the same number of
		/// characters, as the desktop text control does
		void SetTextTo(std::string const& new_text) {
			auto& selection = *c->textSelectionController;
			size_t insertion_point = std::min<size_t>(std::max(selection.GetInsertionPoint(), 0), text.size());
			auto old_pos = agi::CharacterCount(std::string_view(text).substr(0, insertion_point), 0);
			text = new_text;

			selection.SetSelection(0, 0);
			int pos = static_cast<int>(agi::IndexOfCharacter(text, old_pos));
			selection.SetSelection(pos, pos);
			// The desktop control then reports its cursor moved there
			selection.ControlUpdated(pos, true, pos, pos);
		}

		void OnCommit(int type) {
			initial_times.clear();

			// A file was opened, and the active line change which follows
			// sends the fields
			if (type == AssFile::COMMIT_NEW) {
				SendState(false);
				return;
			}
			if (!(type ^ AssFile::COMMIT_ORDER)) return;

			if (!line) {
				text.clear();
				SendState(true);
				return;
			}
			UpdateFields(type);
		}

		void UpdateFields(int type) {
			bool text_changed = false;
			if (type & AssFile::COMMIT_DIAG_TEXT) {
				SetTextTo(line->Text);
				text_changed = true;
			}
			SendState(text_changed);
		}

		void OnActiveLineChanged(AssDialogue *new_line) {
			line = new_line;
			commit_id = -1;
			if (line)
				UpdateFields(AssFile::COMMIT_DIAG_FULL);
			else {
				text.clear();
				SendState(true);
			}
		}

		void OnSelectedSetChanged() {
			initial_times.clear();
		}

		void Commit(std::string const& desc, int type, bool amend, AssDialogue *single_line) {
			file_changed_slot.Block();
			commit_id = c->ass->Commit(desc, type, amend ? commit_id : -1, single_line);
			file_changed_slot.Unblock();
			last_time_commit_type = -1;
			initial_times.clear();
			undo_timer->Start(30000);
		}

		template<typename Setter>
		void SetSelectedRows(Setter set, std::string const& desc, int type, bool amend = false) {
			auto const& sel = c->selectionController->GetSelectedSet();
			for (auto line : sel)
				set(line);
			Commit(desc, type, amend, sel.size() == 1 ? *sel.begin() : nullptr);
		}

		void CommitTimes(TimeField field, agi::Time value) {
			auto const& sel = c->selectionController->GetSelectedSet();
			for (AssDialogue *d : sel) {
				if (!initial_times.count(d))
					initial_times[d] = {d->Start, d->End};

				switch (field) {
					case TIME_START:
						initial_times[d].first = d->Start = value;
						d->End = std::max(d->Start, initial_times[d].second);
						break;

					case TIME_END:
						initial_times[d].second = d->End = value;
						d->Start = std::min(d->End, initial_times[d].first);
						break;

					case TIME_DURATION:
						d->End = d->Start + value;
						initial_times[d].second = d->End;
						break;
				}
			}

			if (field != last_time_commit_type)
				commit_id = -1;

			last_time_commit_type = field;
			file_changed_slot.Block();
			commit_id = c->ass->Commit(_("modify times"), AssFile::COMMIT_DIAG_TIME, commit_id, sel.size() == 1 ? *sel.begin() : nullptr);
			file_changed_slot.Unblock();
		}

	public:
		EditBox(agi::Context *c)
		: c(c)
		, undo_timer(ui::CreateTimer([this] {
			undo_timer->Stop();
			commit_id = -1;
		}))
		, file_changed_slot(c->ass->AddCommitListener(&EditBox::OnCommit, this))
		{
			connections.push_back(c->selectionController->AddActiveLineListener(&EditBox::OnActiveLineChanged, this));
			connections.push_back(c->selectionController->AddSelectionListener(&EditBox::OnSelectedSetChanged, this));
			connections.push_back(c->initialLineState->AddChangeListener([this](std::string const&) {
				if (OPT_GET("Subtitle/Show Original")->GetBool())
					SendState(false);
			}));
			c->textSelectionController->SetControl(std::make_unique<AppTextControl>(c->textSelectionController.get()));

			line = c->selectionController->GetActiveLine();
			if (line)
				text = line->Text;
		}

		~EditBox() {
			c->textSelectionController->SetControl(std::unique_ptr<TextSelectionController::Control>());
		}

		json::Object State() {
			json::Object state = json_io::Obj(
				"active", line != nullptr,
				"text", text,
				"styles", to_array(c->ass->GetStyles()),
				"actors", field_values(c, &AssDialogue::Actor),
				"effects", field_values(c, &AssDialogue::Effect),
				"initial_text", c->initialLineState->GetInitialText(),
				"show_original", OPT_GET("Subtitle/Show Original")->GetBool(),
				"selected", static_cast<int64_t>(c->selectionController->GetSelectedSet().size())
			);
			if (!line) return state;

			int ignore = agi::IGNORE_BLOCKS;
			if (OPT_GET("Subtitle/Character Counter/Ignore Whitespace")->GetBool())
				ignore |= agi::IGNORE_WHITESPACE;
			if (OPT_GET("Subtitle/Character Counter/Ignore Punctuation")->GetBool())
				ignore |= agi::IGNORE_PUNCTUATION;
			auto length = static_cast<int64_t>(agi::MaxLineLength(line->Text.get(), ignore));
			auto limit = OPT_GET("Subtitle/Character Limit")->GetInt();

			json_io::Add(state,
				"row", static_cast<int64_t>(line->Row),
				"start", static_cast<int64_t>(static_cast<int>(line->Start)),
				"end", static_cast<int64_t>(static_cast<int>(line->End)),
				"duration", static_cast<int64_t>(line->End - line->Start),
				"layer", static_cast<int64_t>(line->Layer),
				"comment", line->Comment,
				"style", line->Style.get(),
				"actor", line->Actor.get(),
				"effect", line->Effect.get(),
				"margin_l", static_cast<int64_t>(line->Margin[0]),
				"margin_r", static_cast<int64_t>(line->Margin[1]),
				"margin_v", static_cast<int64_t>(line->Margin[2]),
				"style_exists", c->ass->GetStyle(line->Style) != nullptr,
				"char_count", length,
				"char_limit_exceeded", limit > 0 && length > limit
			);
			return state;
		}

		void Set(Args args) {
			if (!line) return;
			auto field = json_io::GetString(args, "field");

			if (field == "text") {
				text = json_io::GetString(args, "value");
				if (text == line->Text.get()) return;
				if (json_io::GetBool(args, "new_action"))
					commit_id = -1;
				boost::flyweight<std::string> value(text);
				SetSelectedRows([&](AssDialogue *d) { d->Text = value; }, _("modify text"), AssFile::COMMIT_DIAG_TEXT, true);
			}
			else if (field == "start" || field == "end" || field == "duration") {
				agi::Time value(static_cast<int>(json_io::GetInt(args, "value")));
				CommitTimes(field == "start" ? TIME_START : field == "end" ? TIME_END : TIME_DURATION, value);
			}
			else if (field == "layer") {
				int value = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "value"), 0, 999));
				SetSelectedRows([&](AssDialogue *d) { d->Layer = value; }, _("layer change"), AssFile::COMMIT_DIAG_META);
			}
			else if (field == "comment") {
				bool value = json_io::GetBool(args, "value");
				SetSelectedRows([&](AssDialogue *d) { d->Comment = value; }, _("comment change"), AssFile::COMMIT_DIAG_META);
			}
			else if (field == "style") {
				boost::flyweight<std::string> value(json_io::GetString(args, "value"));
				SetSelectedRows([&](AssDialogue *d) { d->Style = value; }, _("style change"), AssFile::COMMIT_DIAG_META);
			}
			else if (field == "actor" || field == "effect") {
				boost::flyweight<std::string> value(json_io::GetString(args, "value"));
				bool amend = json_io::GetBool(args, "typed");
				if (field == "actor")
					SetSelectedRows([&](AssDialogue *d) { d->Actor = value; }, _("actor change"), AssFile::COMMIT_DIAG_META, amend);
				else
					SetSelectedRows([&](AssDialogue *d) { d->Effect = value; }, _("effect change"), AssFile::COMMIT_DIAG_META, amend);
			}
			else if (field == "margin_l" || field == "margin_r" || field == "margin_v") {
				int margin = field == "margin_l" ? 0 : field == "margin_r" ? 1 : 2;
				const char *descriptions[] = {"left margin change", "right margin change", "vertical margin change"};
				int value = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "value"), -9999, 99999));
				SetSelectedRows([&](AssDialogue *d) { d->Margin[margin] = value; }, _(descriptions[margin]), AssFile::COMMIT_DIAG_META);
			}
			else
				throw agi::InvalidInputException("Unknown edit box field: " + field);
		}

		void SetTextSelection(Args args) {
			int size = static_cast<int>(text.size());
			auto position = [&](const char *key) {
				return static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, key), 0, size));
			};
			int start = position("start"), end = position("end");
			int insertion = args.find("insertion") != args.end() ? position("insertion") : end;
			c->textSelectionController->ControlUpdated(insertion, true, std::min(start, end), std::max(start, end));
		}
	};

	std::unique_ptr<EditBox> edit_box;

	EditBox& get() {
		if (!edit_box)
			throw agi::InternalError("The edit box isn't connected");
		return *edit_box;
	}
}

namespace android::edit_box {
	void Connect(agi::Context *c) {
		::edit_box = std::make_unique<EditBox>(c);
	}

	void Disconnect() {
		::edit_box.reset();
	}

	json::Object State(agi::Context *, Args) {
		return get().State();
	}

	json::Object Set(agi::Context *, Args args) {
		get().Set(args);
		return get().State();
	}

	json::Object SetTextSelection(agi::Context *, Args args) {
		get().SetTextSelection(args);
		return {};
	}
}
