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

/// @file dialog_methods.cpp
/// @brief What the desktop dialogs do, for the app's screens
/// @ingroup android
///
/// Each method follows the dialog it names (dialog_shift_times.cpp,
/// dialog_selection.cpp, dialog_search_replace.cpp, dialog_properties.cpp,
/// dialog_style_manager.cpp and dialog_style_editor.cpp), so that the app's
/// screens change the file in the same way.

#include "dialog_methods.h"

#include "json_io.h"

#include "../ass_dialogue.h"
#include "../ass_file.h"
#include "../ass_style.h"
#include "../ass_style_storage.h"
#include "../async_video_provider.h"
#include "../i18n.h"
#include "../include/aegisub/context.h"
#include "../options.h"
#include "../project.h"
#include "../search_replace_engine.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../ui_hooks.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/cajun/reader.h>
#include <libaegisub/cajun/writer.h>
#include <libaegisub/exception.h>
#include <libaegisub/fs.h>
#include <libaegisub/io.h>
#include <libaegisub/log.h>
#include <libaegisub/of_type_adaptor.h>
#include <libaegisub/path.h>
#include <libaegisub/vfr.h>
#include <libaegisub/ycbcr.h>

#include <algorithm>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/range/algorithm/set_algorithm.hpp>
#include <iterator>
#include <set>

namespace {
	using Args = json::Object const&;

	bool has(Args args, const char *key) {
		return args.find(key) != args.end();
	}

	std::vector<std::string> strings(Args args, const char *key) {
		std::vector<std::string> result;
		if (auto array = json_io::GetArray(args, key)) {
			for (auto const& value : *array) {
				if (json_io::TypeOf(value) == json_io::Type::String)
					result.push_back(static_cast<json::String const&>(value));
			}
		}
		return result;
	}

	json::Array to_array(std::vector<std::string> const& values) {
		json::Array array;
		for (auto const& value : values)
			array.emplace_back(value);
		return array;
	}

	// ---- Shift Times ----

	agi::fs::path shift_history_file() {
		return config::path->Decode("?user/shift_history.json");
	}

	json::Array load_shift_history() {
		try {
			json::UnknownElement root;
			json::Reader::Read(root, *agi::io::Open(shift_history_file()));
			return std::move(static_cast<json::Array&>(root));
		}
		catch (agi::fs::FileSystemError const& e) {
			LOG_D("dialog_shift_times/load_history") << "Cannot load shift times history: " << e.GetMessage();
		}
		catch (json::Exception const& e) {
			LOG_D("dialog_shift_times/load_history") << "Cannot load shift times history: " << e.what();
		}
		return {};
	}

	/// The text the desktop dialog shows for an entry of the history
	std::string history_string(json::Object const& obj) {
		auto filename = json_io::GetString(obj, "filename");
		if (filename.empty())
			filename = _("unsaved");

		auto shift_amount = json_io::GetString(obj, "amount");
		if (!json_io::GetBool(obj, "is by time"))
			shift_amount = fmt_plural(atoi(shift_amount.c_str()), "1 frame", "%s frames", shift_amount);

		std::string shift_direction = json_io::GetBool(obj, "is backward") ? _("backward") : _("forward");

		int64_t time_field = json_io::GetInt(obj, "fields");
		std::string fields =
			time_field == 0 ? _("s+e") :
			time_field == 1 ? _("s")   :
			                  _("e")   ;

		std::string lines;
		int64_t sel_mode = json_io::GetInt(obj, "mode");
		auto sel = json_io::GetArray(obj, "selection");
		if (sel_mode == 0)
			lines = _("all");
		else if (sel_mode == 2) {
			if (sel && !sel->empty() && json_io::TypeOf(sel->front()) == json_io::Type::Object)
				lines = fmt_tl("from %d onward", json_io::GetInt(static_cast<json::Object const&>(sel->front()), "start"));
		}
		else {
			lines += _("sel ");
			if (sel) {
				for (auto it = sel->begin(); it != sel->end(); ++it) {
					if (json_io::TypeOf(*it) != json_io::Type::Object) continue;
					auto const& range = static_cast<json::Object const&>(*it);
					int64_t beg = json_io::GetInt(range, "start");
					int64_t end = json_io::GetInt(range, "end");
					if (beg == end)
						lines += std::to_string(beg);
					else
						lines += agi::format("%d-%d", beg, end);
					if (it + 1 != sel->end())
						lines += ";";
				}
			}
		}
		return agi::format("%s, %s %s, %s, %s", filename, shift_amount, shift_direction, fields, lines);
	}

	int shift_time(agi::vfr::Framerate const& fps, int initial_time, int shift, bool by_time, agi::vfr::Time type) {
		if (by_time)
			return initial_time + shift;
		else
			return fps.TimeAtFrame(shift + fps.FrameAtTime(initial_time, type), type);
	}

	// ---- Select Lines ----

	enum class Action {
		SET = 0,
		ADD,
		SUB,
		INTERSECT
	};

	enum Mode {
		EXACT = 0,
		CONTAINS,
		REGEXP
	};

	std::set<AssDialogue*> process(std::string const& match_text, bool match_case, Mode mode, bool invert, bool comments, bool dialogue, int field_n, AssFile *ass) {
		SearchReplaceSettings settings = {
			match_text,
			std::string(),
			static_cast<SearchReplaceSettings::Field>(field_n),
			SearchReplaceSettings::Limit::ALL,
			match_case,
			mode == Mode::REGEXP,
			false,
			false,
			mode == Mode::EXACT
		};

		auto predicate = SearchReplaceEngine::GetMatcher(settings);

		std::set<AssDialogue*> matches;
		for (auto& diag : ass->Events) {
			if (diag.Comment && !comments) continue;
			if (!diag.Comment && !dialogue) continue;

			if (invert != predicate(&diag, 0))
				matches.insert(&diag);
		}

		return matches;
	}

	// ---- Find and Replace ----

	json::Array mru_list(const char *name) {
		json::Array list;
		if (auto entries = config::mru->Get(name)) {
			for (auto const& entry : *entries)
				list.emplace_back(entry.string());
		}
		return list;
	}

	// ---- Properties ----

	/// The script info fields of the desktop dialog's text boxes, in its order
	const char *text_properties[] = {
		"Title", "Original Script", "Original Translation", "Original Editing",
		"Original Timing", "Synch Point", "Script Updated By", "Update Details"
	};

	int set_info_if_different(agi::Context *c, std::string_view key, std::string_view value) {
		if (c->ass->GetScriptInfo(key) != value) {
			c->ass->SetScriptInfo(key, value);
			return 1;
		}
		return 0;
	}

	// ---- Styles ----

	/// The styles of the script or of a catalog, which is saved after every
	/// change like the desktop styles manager does
	class StyleList {
		agi::Context *c;
		std::unique_ptr<AssStyleStorage> store;

	public:
		StyleList(agi::Context *c, Args args) : c(c) {
			auto catalog = json_io::GetString(args, "catalog");
			if (!catalog.empty()) {
				store = std::make_unique<AssStyleStorage>();
				store->LoadCatalog(catalog);
			}
		}

		bool IsCatalog() const { return !!store; }
		AssStyleStorage *Store() { return store.get(); }

		AssStyle *Get(std::string const& name) {
			return store ? store->GetStyle(name) : c->ass->GetStyle(name);
		}

		std::vector<AssStyle *> All() {
			std::vector<AssStyle *> styles;
			if (store) {
				for (auto& style : *store)
					styles.push_back(style.get());
			}
			else {
				for (auto& style : c->ass->Styles)
					styles.push_back(&style);
			}
			return styles;
		}
	};

	/// Check if there are any uses of a style name in the file, or replace them
	class StyleRenamer {
		agi::Context *c;
		bool found_any = false;
		bool do_replace = false;
		std::string source_name;
		std::string new_name;

		/// Process a single override parameter to check if it's \r with this style name
		static void ProcessTag(std::string const& tag, AssOverrideParameter* param, void *userData) {
			StyleRenamer *self = static_cast<StyleRenamer*>(userData);
			if (tag == "\\r" && param->GetType() == VariableDataType::TEXT && param->Get<std::string>() == self->source_name) {
				if (self->do_replace)
					param->Set(self->new_name);
				else
					self->found_any = true;
			}
		}

		void Walk(bool replace) {
			found_any = false;
			do_replace = replace;

			for (auto& diag : c->ass->Events) {
				if (diag.Style == source_name) {
					if (replace)
						diag.Style = new_name;
					else
						found_any = true;
				}

				auto blocks = diag.ParseTags();
				for (auto block : blocks | agi::of_type<AssDialogueBlockOverride>())
					block->ProcessParameters(&StyleRenamer::ProcessTag, this);
				if (replace)
					diag.UpdateText(blocks);

				if (found_any) return;
			}
		}

	public:
		StyleRenamer(agi::Context *c, std::string source_name, std::string new_name)
		: c(c)
		, source_name(std::move(source_name))
		, new_name(std::move(new_name))
		{
		}

		bool NeedsReplace() {
			Walk(false);
			return found_any;
		}

		void Replace() {
			Walk(true);
		}
	};

	template<class Func>
	std::string unique_name(Func name_checker, std::string const& source_name) {
		if (name_checker(source_name)) {
			std::string name = agi::format(_("%s - Copy"), source_name);
			for (int i = 2; name_checker(name); ++i)
				name = agi::format(_("%s - Copy (%d)"), source_name, i);
			return name;
		}
		return source_name;
	}

	int confirm_delete(int n, std::string const& title) {
		return ui::ShowMessage(
			fmt_plural(n, "Are you sure you want to delete this style?", "Are you sure you want to delete these %d styles?", n),
			title, ui::YES_NO | ui::ICON_WARNING, nullptr);
	}

	/// Set the fields of a style which the editor's values have
	void style_from_json(AssStyle &style, json::Object const& values) {
		auto color = [&](const char *key, agi::Color &field) {
			auto value = json_io::GetString(values, key);
			if (!value.empty())
				field = agi::Color(value);
		};

		style.font = json_io::GetString(values, "font", style.font);
		style.fontsize = json_io::GetDouble(values, "size", style.fontsize);
		color("primary", style.primary);
		color("secondary", style.secondary);
		color("outline", style.outline);
		color("shadow", style.shadow);
		style.bold = json_io::GetBool(values, "bold", style.bold);
		style.italic = json_io::GetBool(values, "italic", style.italic);
		style.underline = json_io::GetBool(values, "underline", style.underline);
		style.strikeout = json_io::GetBool(values, "strikeout", style.strikeout);
		style.scalex = json_io::GetDouble(values, "scale_x", style.scalex);
		style.scaley = json_io::GetDouble(values, "scale_y", style.scaley);
		style.spacing = json_io::GetDouble(values, "spacing", style.spacing);
		style.angle = json_io::GetDouble(values, "angle", style.angle);
		style.borderstyle = static_cast<int>(json_io::GetInt(values, "border_style", style.borderstyle));
		style.outline_w = json_io::GetDouble(values, "outline_width", style.outline_w);
		style.shadow_w = json_io::GetDouble(values, "shadow_width", style.shadow_w);
		style.alignment = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(values, "alignment", style.alignment), 1, 9));
		const char *margins[] = {"margin_l", "margin_r", "margin_v"};
		for (int i = 0; i < 3; ++i)
			style.Margin[i] = static_cast<int>(json_io::GetInt(values, margins[i], style.Margin[i]));
		style.encoding = static_cast<int>(json_io::GetInt(values, "encoding", style.encoding));
	}

	json::Object style_list(StyleList &list) {
		json::Array styles;
		for (auto style : list.All())
			styles.push_back(android::dialogs::StyleToJson(*style));
		return json_io::Obj("styles", std::move(styles));
	}

	struct cmp_name {
		template<typename T>
		bool operator()(T const& lft, T const& rgt) const { return lft->name < rgt->name; }
	};

	template<class Cont>
	void do_move(Cont& styls, int type, int& first, int& last, bool storage) {
		auto begin = styls.begin();

		// Move up
		if (type == 0) {
			if (first == 0) return;
			rotate(begin + first - 1, begin + first, begin + last + 1);
			first--;
			last--;
		}
		// Move to top
		else if (type == 1) {
			rotate(begin, begin + first, begin + last + 1);
			last = last - first;
			first = 0;
		}
		// Move down
		else if (type == 2) {
			if (last + 1 == (int)styls.size()) return;
			rotate(begin + first, begin + last + 1, begin + last + 2);
			first++;
			last++;
		}
		// Move to bottom
		else if (type == 3) {
			rotate(begin + first, begin + last + 1, styls.end());
			first = styls.size() - (last - first + 1);
			last = styls.size() - 1;
		}
		// Sort
		else if (type == 4) {
			// Get confirmation
			if (storage) {
				int res = ui::ShowMessage(_("Are you sure? This cannot be undone!"), _("Sort styles"), ui::YES_NO | ui::CENTRE, nullptr);
				if (res == ui::NO) return;
			}

			sort(styls.begin(), styls.end(), cmp_name());

			first = 0;
			last = 0;
		}
	}
}

namespace android::dialogs {
	json::Object ShiftTimesState(agi::Context *c, Args) {
		json::Array history;
		for (auto& entry : load_shift_history()) {
			if (json_io::TypeOf(entry) != json_io::Type::Object) continue;
			auto const& obj = static_cast<json::Object const&>(entry);
			history.push_back(json_io::Obj(
				"display", history_string(obj),
				"by_time", json_io::GetBool(obj, "is by time"),
				"backward", json_io::GetBool(obj, "is backward"),
				"amount", json_io::GetString(obj, "amount"),
				"fields", json_io::GetInt(obj, "fields"),
				"mode", json_io::GetInt(obj, "mode")
			));
		}

		bool can_use_frames = c->project->Timecodes().IsLoaded();
		bool has_selection = !c->selectionController->GetSelectedSet().empty();
		int64_t affect = OPT_GET("Tool/Shift Times/Affect")->GetInt();
		if (!has_selection)
			affect = 0;
		return json_io::Obj(
			"time", agi::Time(static_cast<int>(OPT_GET("Tool/Shift Times/Time")->GetInt())).GetAssFormatted(),
			"frames", OPT_GET("Tool/Shift Times/Frames")->GetInt(),
			"by_time", OPT_GET("Tool/Shift Times/ByTime")->GetBool() || !can_use_frames,
			"fields", OPT_GET("Tool/Shift Times/Type")->GetInt(),
			"mode", affect,
			"backward", OPT_GET("Tool/Shift Times/Direction")->GetBool(),
			"can_use_frames", can_use_frames,
			"has_selection", has_selection,
			"history", std::move(history)
		);
	}

	void ShiftTimes(agi::Context *c, Args args) {
		auto fps = c->project->Timecodes();
		auto time_text = json_io::GetString(args, "time", "0:00:00.00");
		int time = agi::Time(time_text);
		int64_t frames = json_io::GetInt(args, "frames");
		bool by_time = json_io::GetBool(args, "by_time", true) || !fps.IsLoaded();
		bool reverse = json_io::GetBool(args, "backward");
		int type = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "fields"), 0, 2));
		auto const& sel = c->selectionController->GetSelectedSet();
		int mode = sel.empty() ? 0 : static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "mode"), 0, 2));

		// Kept when the desktop dialog closes
		OPT_SET("Tool/Shift Times/Time")->SetInt(time);
		OPT_SET("Tool/Shift Times/Frames")->SetInt(frames);
		OPT_SET("Tool/Shift Times/ByTime")->SetBool(by_time);
		OPT_SET("Tool/Shift Times/Type")->SetInt(type);
		OPT_SET("Tool/Shift Times/Affect")->SetInt(mode);
		OPT_SET("Tool/Shift Times/Direction")->SetBool(reverse);

		if (!json_io::GetBool(args, "apply", true))
			return;

		bool start = type != 2;
		bool end = type != 1;

		long shift;
		if (by_time) {
			shift = time;
			if (shift == 0)
				return;
		}
		else
			shift = static_cast<long>(frames);

		if (reverse)
			shift = -shift;

		// Track which rows were shifted for the log
		int block_start = 0;
		json::Array shifted_blocks;

		for (auto& line : c->ass->Events) {
			if (!sel.count(&line)) {
				if (block_start) {
					json::Object block;
					block["start"] = block_start;
					block["end"] = line.Row;
					shifted_blocks.push_back(std::move(block));
					block_start = 0;
				}
				if (mode == 1) continue;
				if (mode == 2 && shifted_blocks.empty()) continue;
			}
			else if (!block_start)
				block_start = line.Row + 1;

			if (start)
				line.Start = shift_time(fps, line.Start, shift, by_time, agi::vfr::START);
			if (end)
				line.End = shift_time(fps, line.End, shift, by_time, agi::vfr::END);
		}

		c->ass->Commit(_("shifting"), AssFile::COMMIT_DIAG_TIME);

		if (block_start) {
			json::Object block;
			block["start"] = block_start;
			block["end"] = c->ass->Events.back().Row + 1;
			shifted_blocks.push_back(std::move(block));
		}

		json::Object new_entry;
		new_entry["filename"] = c->subsController->Filename().filename().string();
		new_entry["is by time"] = by_time;
		new_entry["is backward"] = reverse;
		new_entry["amount"] = by_time ? agi::Time(time).GetAssFormatted() : std::to_string(frames);
		new_entry["fields"] = type;
		new_entry["mode"] = mode;
		new_entry["selection"] = std::move(shifted_blocks);

		auto history = load_shift_history();
		history.insert(history.begin(), std::move(new_entry));
		if (history.size() > 50)
			history.resize(50);

		try {
			agi::JsonWriter::Write(history, agi::io::Save(shift_history_file()).Get());
		}
		catch (agi::fs::FileSystemError const& e) {
			LOG_E("dialog_shift_times/save_history") << "Cannot save shift times history: " << e.GetMessage();
		}
	}

	void ClearShiftHistory(agi::Context *, Args) {
		agi::fs::Remove(shift_history_file());
	}

	json::Object SelectLinesState(agi::Context *, Args) {
		return json_io::Obj(
			"text", OPT_GET("Tool/Select Lines/Text")->GetString(),
			"field", OPT_GET("Tool/Select Lines/Field")->GetInt(),
			"action", OPT_GET("Tool/Select Lines/Action")->GetInt(),
			"match_case", OPT_GET("Tool/Select Lines/Match/Case")->GetBool(),
			"dialogue", OPT_GET("Tool/Select Lines/Match/Dialogue")->GetBool(),
			"comments", OPT_GET("Tool/Select Lines/Match/Comment")->GetBool(),
			"invert", !!OPT_GET("Tool/Select Lines/Condition")->GetInt(),
			"mode", OPT_GET("Tool/Select Lines/Mode")->GetInt()
		);
	}

	json::Object SelectLines(agi::Context *c, Args args) {
		auto text = json_io::GetString(args, "text");
		bool match_case = json_io::GetBool(args, "match_case");
		auto mode = static_cast<Mode>(std::clamp<int64_t>(json_io::GetInt(args, "mode", 1), 0, 2));
		bool invert = json_io::GetBool(args, "invert");
		bool comments = json_io::GetBool(args, "comments");
		bool dialogue = json_io::GetBool(args, "dialogue", true);
		// The dialog always keeps one of these checked
		if (!comments && !dialogue)
			dialogue = true;
		int field = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "field"), 0, 3));
		auto action = static_cast<Action>(std::clamp<int64_t>(json_io::GetInt(args, "action"), 0, 3));

		// Kept when the desktop dialog closes
		OPT_SET("Tool/Select Lines/Text")->SetString(text);
		OPT_SET("Tool/Select Lines/Condition")->SetInt(invert);
		OPT_SET("Tool/Select Lines/Field")->SetInt(field);
		OPT_SET("Tool/Select Lines/Action")->SetInt(static_cast<int>(action));
		OPT_SET("Tool/Select Lines/Mode")->SetInt(mode);
		OPT_SET("Tool/Select Lines/Match/Case")->SetBool(match_case);
		OPT_SET("Tool/Select Lines/Match/Dialogue")->SetBool(dialogue);
		OPT_SET("Tool/Select Lines/Match/Comment")->SetBool(comments);

		std::set<AssDialogue*> matches;
		try {
			matches = process(text, match_case, mode, invert, comments, dialogue, field, c->ass.get());
		}
		catch (agi::Exception const&) {
			// The desktop dialog does nothing when the pattern is invalid
			return json_io::Obj("changed", false);
		}

		Selection old_sel, new_sel;
		if (action != Action::SET)
			old_sel = c->selectionController->GetSelectedSet();

		std::string message;
		size_t count = 0;
		switch (action) {
			case Action::SET:
				new_sel = std::move(matches);
				message = (count = new_sel.size())
					? fmt_plural(count, "Selection was set to one line", "Selection was set to %u lines", count)
					: _("Selection was set to no lines");
				break;

			case Action::ADD:
				boost::set_union(old_sel, matches, inserter(new_sel, new_sel.begin()));
				message = (count = new_sel.size() - old_sel.size())
					? fmt_plural(count, "One line was added to selection", "%u lines were added to selection", count)
					: _("No lines were added to selection");
				break;

			case Action::SUB:
				boost::set_difference(old_sel, matches, inserter(new_sel, new_sel.begin()));
				goto sub_message;

			case Action::INTERSECT:
				boost::set_intersection(old_sel, matches, inserter(new_sel, new_sel.begin()));
				sub_message:
				message = (count = old_sel.size() - new_sel.size())
					? fmt_plural(count, "One line was removed from selection", "%u lines were removed from selection", count)
					: _("No lines were removed from selection");
				break;
		}

		if (count == 0)
			ui::ShowMessage(message, _("Selection"), ui::OK | ui::CENTRE, nullptr);
		else
			ui::StatusTimeout(c, message);

		AssDialogue *new_active = c->selectionController->GetActiveLine();
		if (new_sel.size() && !new_sel.count(new_active))
			new_active = *new_sel.begin();
		c->selectionController->SetSelectionAndActive(std::move(new_sel), new_active);

		return json_io::Obj(
			"changed", true,
			"message", message,
			"count", static_cast<int64_t>(count)
		);
	}

	json::Object SearchState(agi::Context *, Args) {
		return json_io::Obj(
			"find_history", mru_list("Find"),
			"replace_history", mru_list("Replace"),
			"field", OPT_GET("Tool/Search Replace/Field")->GetInt(),
			"limit", OPT_GET("Tool/Search Replace/Affect")->GetInt(),
			"match_case", OPT_GET("Tool/Search Replace/Match Case")->GetBool(),
			"regex", OPT_GET("Tool/Search Replace/RegExp")->GetBool(),
			"skip_comments", OPT_GET("Tool/Search Replace/Skip Comments")->GetBool(),
			"skip_tags", OPT_GET("Tool/Search Replace/Skip Tags")->GetBool()
		);
	}

	json::Object Search(agi::Context *c, Args args) {
		SearchReplaceSettings settings;
		settings.find = json_io::GetString(args, "find");
		settings.replace_with = json_io::GetString(args, "replace");
		settings.field = static_cast<SearchReplaceSettings::Field>(std::clamp<int64_t>(json_io::GetInt(args, "field"), 0, 3));
		settings.limit_to = static_cast<SearchReplaceSettings::Limit>(std::clamp<int64_t>(json_io::GetInt(args, "limit"), 0, 1));
		settings.match_case = json_io::GetBool(args, "match_case");
		settings.use_regex = json_io::GetBool(args, "regex");
		settings.ignore_comments = json_io::GetBool(args, "skip_comments");
		settings.skip_tags = json_io::GetBool(args, "skip_tags");
		settings.exact_match = false;

		auto action = json_io::GetString(args, "action", "find_next");
		bool has_replace = action != "find_next" || has(args, "replace");

		if (settings.find.empty())
			return json_io::Obj("found", false);

		c->search->Configure(settings);
		bool found = false;
		try {
			if (action == "replace_next")
				found = c->search->ReplaceNext();
			else if (action == "replace_all")
				found = c->search->ReplaceAll();
			else
				found = c->search->FindNext();
		}
		catch (std::exception const& e) {
			ui::ShowMessage(e.what(), _("Error"), ui::OK | ui::ICON_ERROR | ui::CENTRE, nullptr);
			return json_io::Obj("found", false);
		}

		config::mru->Add("Find", settings.find);
		if (has_replace)
			config::mru->Add("Replace", settings.replace_with);

		OPT_SET("Tool/Search Replace/Match Case")->SetBool(settings.match_case);
		OPT_SET("Tool/Search Replace/RegExp")->SetBool(settings.use_regex);
		OPT_SET("Tool/Search Replace/Skip Comments")->SetBool(settings.ignore_comments);
		OPT_SET("Tool/Search Replace/Skip Tags")->SetBool(settings.skip_tags);
		OPT_SET("Tool/Search Replace/Field")->SetInt(static_cast<int>(settings.field));
		OPT_SET("Tool/Search Replace/Affect")->SetInt(static_cast<int>(settings.limit_to));

		return json_io::Obj("found", found);
	}

	json::Object Properties(agi::Context *c, Args) {
		json::Object text;
		for (auto key : text_properties)
			text[key] = std::string(c->ass->GetScriptInfo(key));

		json::Array matrices;
		for (auto matrix : agi::ycbcr::valid_header_strings)
			matrices.emplace_back(std::string(matrix));

		auto provider = c->project->VideoProvider();
		json::Object video;
		if (provider) {
			video = json_io::Obj(
				"width", static_cast<int64_t>(provider->GetWidth()),
				"height", static_cast<int64_t>(provider->GetHeight()),
				"matrix", agi::ycbcr::Header(provider->GetRealColorSpace()).to_existing_string()
			);
		}

		return json_io::Obj(
			"text", std::move(text),
			"res_x", std::to_string(c->ass->GetScriptInfoAsInt("PlayResX")),
			"res_y", std::to_string(c->ass->GetScriptInfoAsInt("PlayResY")),
			"layout_res_x", std::to_string(c->ass->GetScriptInfoAsInt("LayoutResX")),
			"layout_res_y", std::to_string(c->ass->GetScriptInfoAsInt("LayoutResY")),
			"wrap_style", static_cast<int64_t>(c->ass->GetScriptInfoAsInt("WrapStyle")),
			"scaled_border", boost::iequals(c->ass->GetScriptInfo("ScaledBorderAndShadow"), "yes"),
			"matrix", std::string(c->ass->GetScriptInfo("YCbCr Matrix")),
			"matrices", std::move(matrices),
			"video", std::move(video)
		);
	}

	json::Object SetProperties(agi::Context *c, Args args) {
		int count = 0;
		if (auto text = json_io::GetObject(args, "text")) {
			for (auto key : text_properties) {
				if (text->find(key) != text->end())
					count += set_info_if_different(c, key, json_io::GetString(*text, key));
			}
		}

		// The desktop dialog's text boxes only take digits
		auto digits = [&](const char *arg, const char *key) {
			auto value = json_io::GetString(args, arg, std::to_string(c->ass->GetScriptInfoAsInt(key)));
			value.erase(std::remove_if(value.begin(), value.end(), [](char ch) { return ch < '0' || ch > '9'; }), value.end());
			return value;
		};

		count += set_info_if_different(c, "PlayResX", digits("res_x", "PlayResX"));
		count += set_info_if_different(c, "PlayResY", digits("res_y", "PlayResY"));
		count += set_info_if_different(c, "LayoutResX", digits("layout_res_x", "LayoutResX"));
		count += set_info_if_different(c, "LayoutResY", digits("layout_res_y", "LayoutResY"));
		int64_t wrap_style = std::clamp<int64_t>(json_io::GetInt(args, "wrap_style", c->ass->GetScriptInfoAsInt("WrapStyle")), 0, 3);
		count += set_info_if_different(c, "WrapStyle", std::to_string(wrap_style));
		count += set_info_if_different(c, "ScaledBorderAndShadow", json_io::GetBool(args, "scaled_border") ? "yes" : "no");

		auto matrix = json_io::GetString(args, "matrix", c->ass->GetScriptInfo("YCbCr Matrix"));
		// The desktop dialog's list only has the valid values
		bool valid_matrix = std::ranges::find(agi::ycbcr::valid_header_strings, matrix) != std::end(agi::ycbcr::valid_header_strings);
		if (valid_matrix || matrix == c->ass->GetScriptInfo("YCbCr Matrix"))
			count += set_info_if_different(c, "YCbCr Matrix", matrix);

		if (count) c->ass->Commit(_("property changes"), AssFile::COMMIT_SCRIPTINFO);
		return json_io::Obj("changed", count > 0);
	}

	json::Object Catalogs(agi::Context *c, Args) {
		auto catalogs = AssStyleStorage::GetCatalogs();
		// Create a default storage if there are none
		if (catalogs.empty()) {
			AssStyleStorage store;
			store.LoadCatalog("Default");
			store.push_back(std::make_unique<AssStyle>());
			store.Save();
			catalogs.push_back("Default");
		}

		std::string current = c->ass->Properties.style_storage;
		if (current.empty())
			current = "Default";
		auto it = std::find_if(catalogs.begin(), catalogs.end(), [&](std::string const& name) {
			return boost::iequals(name, current);
		});
		current = it == catalogs.end() ? catalogs.front() : *it;

		return json_io::Obj(
			"catalogs", to_array(catalogs),
			"current", current
		);
	}

	json::Object SetCatalog(agi::Context *c, Args args) {
		c->ass->Properties.style_storage = json_io::GetString(args, "catalog");
		return {};
	}

	json::Object NewCatalog(agi::Context *c, Args args) {
		auto name = json_io::GetString(args, "name");
		if (name.empty())
			return json_io::Obj("created", false);

		// Remove bad characters from the name
		const std::string_view badchars = "\\/:*?\"<>|";
		int badchars_removed = 0;
		for (auto& chr : name) {
			if (badchars.find(chr) != badchars.npos) {
				chr = '_';
				++badchars_removed;
			}
		}

		// Make sure that there is no storage with the same name (case insensitive search since Windows filenames are case insensitive)
		for (auto const& catalog : AssStyleStorage::GetCatalogs()) {
			if (boost::iequals(catalog, name)) {
				ui::ShowMessage(_("A catalog with that name already exists."), _("Catalog name conflict"), ui::OK | ui::ICON_ERROR | ui::CENTRE, nullptr);
				return json_io::Obj("created", false);
			}
		}

		// Warn about bad characters
		if (badchars_removed) {
			ui::ShowMessage(
				fmt_tl("The specified catalog name contains one or more illegal characters. They have been replaced with underscores instead.\nThe catalog has been renamed to \"%s\".", name),
				_("Invalid characters"), ui::OK, nullptr);
		}

		// The desktop dialog makes the file when a style is first saved in it
		AssStyleStorage store;
		store.LoadCatalog(name);
		store.Save();
		c->ass->Properties.style_storage = name;
		return json_io::Obj("created", true, "name", name);
	}

	json::Object DeleteCatalog(agi::Context *c, Args args) {
		auto name = json_io::GetString(args, "catalog");
		if (AssStyleStorage::GetCatalogs().size() <= 1 || name.empty())
			return json_io::Obj("deleted", false);

		auto message = fmt_tl("Are you sure you want to delete the storage \"%s\" from the catalog?", name);
		if (ui::ShowMessage(message, _("Confirm delete"), ui::YES_NO | ui::ICON_WARNING, nullptr) != ui::YES)
			return json_io::Obj("deleted", false);

		agi::fs::Remove(config::path->Decode("?user/catalog/" + name + ".sty"));
		if (boost::iequals(c->ass->Properties.style_storage, name))
			c->ass->Properties.style_storage.clear();
		return json_io::Obj("deleted", true);
	}

	json::Object CatalogStyles(agi::Context *c, Args args) {
		StyleList list(c, args);
		return style_list(list);
	}

	json::Object CopyStyleName(agi::Context *c, Args args) {
		StyleList list(c, args);
		auto name = json_io::GetString(args, "name");
		return json_io::Obj("name", unique_name([&](std::string const& str) { return list.Get(str) != nullptr; }, name));
	}

	json::Object SaveStyle(agi::Context *c, Args args) {
		StyleList list(c, args);
		auto old_name = json_io::GetString(args, "name");
		bool is_new = old_name.empty() || json_io::GetBool(args, "new");
		auto values = json_io::GetObject(args, "style");
		if (!values)
			throw agi::InvalidInputException("No style to save");

		AssStyle *style = is_new ? nullptr : list.Get(old_name);
		if (!is_new && !style)
			throw agi::InvalidInputException("No style named " + old_name);

		// The editor works on a copy, which starts as a default style for a
		// new one
		AssStyle work = style ? *style : AssStyle();
		if (is_new && json_io::GetString(*values, "name").empty())
			throw agi::InvalidInputException("The style has no name");
		auto new_name = json_io::GetString(*values, "name", work.name);

		// Check if style name is unique
		AssStyle *existing = list.Get(new_name);
		if (existing && existing != style) {
			ui::ShowMessage(_("There is already a style with this name. Please choose another name."), _("Style name conflict"), ui::OK | ui::ICON_ERROR | ui::CENTRE, nullptr);
			return json_io::Obj("saved", false);
		}

		// Style name change
		bool did_rename = false;
		if (work.name != new_name) {
			if (!list.IsCatalog() && !is_new) {
				StyleRenamer renamer(c, work.name, new_name);
				if (renamer.NeedsReplace()) {
					// See if user wants to update style name through script
					int answer = ui::ShowMessage(
						_("Do you want to change all instances of this style in the script to this new name?"),
						_("Update script?"),
						ui::YES_NO | ui::CANCEL, nullptr);

					if (answer == ui::CANCEL)
						return json_io::Obj("saved", false);

					if (answer == ui::YES) {
						did_rename = true;
						renamer.Replace();
					}
				}
			}

			work.name = new_name;
		}

		style_from_json(work, *values);

		if (is_new) {
			auto added = new AssStyle(work);
			added->UpdateData();
			if (auto store = list.Store())
				store->push_back(std::unique_ptr<AssStyle>(added));
			else
				c->ass->Styles.push_back(*added);
		}
		else {
			*style = work;
			style->UpdateData();
		}

		if (auto store = list.Store())
			store->Save();
		else
			c->ass->Commit(_("style change"), AssFile::COMMIT_STYLES | (did_rename ? AssFile::COMMIT_DIAG_FULL : 0));

		return json_io::Obj("saved", true, "name", work.name);
	}

	json::Object DeleteStyles(agi::Context *c, Args args) {
		StyleList list(c, args);
		std::vector<AssStyle *> styles;
		for (auto const& name : strings(args, "names")) {
			if (auto style = list.Get(name))
				styles.push_back(style);
		}
		if (styles.empty())
			return json_io::Obj("deleted", false);

		auto title = list.IsCatalog() ? _("Confirm delete from storage") : _("Confirm delete from current");
		if (confirm_delete(static_cast<int>(styles.size()), title) != ui::YES)
			return json_io::Obj("deleted", false);

		if (auto store = list.Store()) {
			for (auto style : styles) {
				for (size_t i = 0; i < store->size(); ++i) {
					if ((*store)[i] == style) {
						store->Delete(static_cast<int>(i));
						break;
					}
				}
			}
			store->Save();
		}
		else {
			for (auto style : styles)
				delete style;
			c->ass->Commit(_("style delete"), AssFile::COMMIT_STYLES);
		}
		return json_io::Obj("deleted", true);
	}

	json::Object MoveStyles(agi::Context *c, Args args) {
		StyleList list(c, args);
		// 0: up, 1: to the top, 2: down, 3: to the bottom, 4: sort by name
		int type = static_cast<int>(std::clamp<int64_t>(json_io::GetInt(args, "type"), 0, 4));
		auto all = list.All();

		// The moved styles are the ones between the first and the last
		// selected, like the desktop dialog moves them
		int first = -1, last = -1;
		auto names = strings(args, "names");
		for (int i = 0; i < static_cast<int>(all.size()); ++i) {
			if (std::find(names.begin(), names.end(), all[i]->name) != names.end()) {
				if (first == -1) first = i;
				last = i;
			}
		}
		if (first == -1 && type != 4)
			return json_io::Obj("first", static_cast<int64_t>(-1), "last", static_cast<int64_t>(-1));
		if (first == -1)
			first = last = 0;

		if (auto store = list.Store()) {
			std::vector<std::unique_ptr<AssStyle>> owned;
			for (auto& style : *store)
				owned.push_back(std::make_unique<AssStyle>(*style));
			do_move(owned, type, first, last, true);
			store->clear();
			for (auto& style : owned)
				store->push_back(std::move(style));
			store->Save();
		}
		else {
			std::vector<AssStyle *> styleMap = all;
			do_move(styleMap, type, first, last, false);

			// Replace styles
			size_t curn = 0;
			for (auto it = c->ass->Styles.begin(); it != c->ass->Styles.end(); ++it) {
				auto new_style_at_pos = c->ass->Styles.iterator_to(*styleMap[curn]);
				EntryList<AssStyle>::node_algorithms::swap_nodes(it.pointed_node(), new_style_at_pos.pointed_node());
				if (++curn == styleMap.size()) break;
				it = new_style_at_pos;
			}

			c->ass->Commit(_("style move"), AssFile::COMMIT_STYLES);
		}

		return json_io::Obj("first", static_cast<int64_t>(first), "last", static_cast<int64_t>(last));
	}

	json::Object CopyStylesToCatalog(agi::Context *c, Args args) {
		auto catalog = json_io::GetString(args, "catalog");
		if (catalog.empty())
			throw agi::InvalidInputException("No catalog to copy to");
		AssStyleStorage store;
		store.LoadCatalog(catalog);

		json::Array copied;
		for (auto const& name : strings(args, "names")) {
			auto source = c->ass->GetStyle(name);
			if (!source) continue;

			if (AssStyle *style = store.GetStyle(name)) {
				if (ui::YES == ui::ShowMessage(fmt_tl("There is already a style with the name \"%s\" in the current storage. Overwrite?", name), _("Style name collision"), ui::YES_NO, nullptr)) {
					*style = *source;
					copied.emplace_back(name);
				}
			}
			else {
				store.push_back(std::make_unique<AssStyle>(*source));
				copied.emplace_back(name);
			}
		}
		store.Save();
		return json_io::Obj("copied", std::move(copied));
	}

	json::Object CopyStylesToScript(agi::Context *c, Args args) {
		auto catalog = json_io::GetString(args, "catalog");
		if (catalog.empty())
			throw agi::InvalidInputException("No catalog to copy from");
		AssStyleStorage store;
		store.LoadCatalog(catalog);

		json::Array copied;
		for (auto const& name : strings(args, "names")) {
			auto source = store.GetStyle(name);
			if (!source) continue;

			if (AssStyle *style = c->ass->GetStyle(name)) {
				if (ui::YES == ui::ShowMessage(fmt_tl("There is already a style with the name \"%s\" in the current script. Overwrite?", name), _("Style name collision"), ui::YES_NO, nullptr)) {
					*style = *source;
					copied.emplace_back(name);
				}
			}
			else {
				c->ass->Styles.push_back(*new AssStyle(*source));
				copied.emplace_back(name);
			}
		}

		c->ass->Commit(_("style copy"), AssFile::COMMIT_STYLES);
		return json_io::Obj("copied", std::move(copied));
	}

	json::Object StyleEncodings(agi::Context *, Args) {
		return json_io::Obj("encodings", to_array(AssStyle::GetEncodings()));
	}

	json::Object StyleToJson(AssStyle const& style) {
		return json_io::Obj(
			"name", style.name,
			"font", style.font,
			"size", style.fontsize,
			"primary", style.primary.GetAssStyleFormatted(),
			"secondary", style.secondary.GetAssStyleFormatted(),
			"outline", style.outline.GetAssStyleFormatted(),
			"shadow", style.shadow.GetAssStyleFormatted(),
			"bold", style.bold,
			"italic", style.italic,
			"underline", style.underline,
			"strikeout", style.strikeout,
			"scale_x", style.scalex,
			"scale_y", style.scaley,
			"spacing", style.spacing,
			"angle", style.angle,
			"border_style", static_cast<int64_t>(style.borderstyle),
			"outline_width", style.outline_w,
			"shadow_width", style.shadow_w,
			"alignment", static_cast<int64_t>(style.alignment),
			"margin_l", static_cast<int64_t>(style.Margin[0]),
			"margin_r", static_cast<int64_t>(style.Margin[1]),
			"margin_v", static_cast<int64_t>(style.Margin[2]),
			"encoding", static_cast<int64_t>(style.encoding)
		);
	}
}
