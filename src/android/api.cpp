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

/// @file api.cpp
/// @brief The methods the app calls with NativeEngine.call()
/// @ingroup android
///
/// Each method takes a JSON object of arguments and returns a JSON object.
/// Lines are identified by their row, their position among the dialogue
/// lines counting from 0. Changing the file through these methods works the
/// same way as the desktop program's subtitle grid and edit box, including
/// undo.

#include "android.h"
#include "bridge.h"
#include "dialog_methods.h"
#include "edit_box.h"
#include "json_io.h"
#include "media.h"

#include "../ass_dialogue.h"
#include "../ass_file.h"
#include "../ass_info.h"
#include "../ass_style.h"
#include "../auto4_base.h"
#include "../command/command.h"
#include "../fold_controller.h"
#include "../font_index.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../ui_hooks.h"
#include "../ytt_export.h"

#include <libaegisub/ass/time.h>
#include <libaegisub/character_count.h>
#include <libaegisub/exception.h>
#include <libaegisub/format.h>
#include <libaegisub/fs.h>
#include <libaegisub/signal.h>

#include <map>

namespace {
	using Args = json::Object const&;
	using Method = json::Object (*)(agi::Context *, Args);

	std::vector<agi::signal::Connection> connections;
	/// The commit the last set_lines call made, which the next one with
	/// "amend" adds its changes to, like typing in the edit box
	int last_edit_commit = -1;

	std::vector<AssDialogue *> all_lines(agi::Context *c) {
		std::vector<AssDialogue *> lines;
		for (auto& line : c->ass->Events)
			lines.push_back(&line);
		return lines;
	}

	AssDialogue *line_at(std::vector<AssDialogue *> const& lines, int64_t row) {
		if (row < 0 || row >= static_cast<int64_t>(lines.size()))
			throw agi::InvalidInputException("No line at row " + std::to_string(row));
		return lines[row];
	}

	/// Characters per second, as the desktop grid's CPS column shows them,
	/// or -1 when it shows nothing
	int64_t cps(AssDialogue const& line) {
		int duration = line.End - line.Start;
		auto const& text = line.Text.get();
		if (duration <= 100 || text.size() > static_cast<size_t>(duration))
			return -1;

		int ignore = agi::IGNORE_BLOCKS;
		if (OPT_GET("Subtitle/Character Counter/Ignore Whitespace")->GetBool())
			ignore |= agi::IGNORE_WHITESPACE;
		if (OPT_GET("Subtitle/Character Counter/Ignore Punctuation")->GetBool())
			ignore |= agi::IGNORE_PUNCTUATION;
		return static_cast<int64_t>(agi::CharacterCount(text, ignore) * 1000 / duration);
	}

	/// What the desktop grid's fold column shows: v for an open fold, > for
	/// a closed one and - for the end of an open one, indented by depth
	std::string fold_marker(const AssDialogue *line) {
		std::string value;
		if (line->Fold.hasFold()) {
			if (!line->Fold.isEnd())
				value = line->Fold.isFolded() ? ">" : "v";
			else if (!line->Fold.isFolded())
				value = "-";
			while (line->Fold.getFoldOpener()) {
				line = line->Fold.getFoldOpener();
				value = " " + value;
			}
		}
		return value;
	}

	json::Object line_to_json(AssDialogue const& line) {
		return json_io::Obj(
			"row", static_cast<int64_t>(line.Row),
			"id", static_cast<int64_t>(line.Id),
			"comment", line.Comment,
			"layer", static_cast<int64_t>(line.Layer),
			"start", static_cast<int64_t>(static_cast<int>(line.Start)),
			"end", static_cast<int64_t>(static_cast<int>(line.End)),
			"style", line.Style.get(),
			"actor", line.Actor.get(),
			"effect", line.Effect.get(),
			"margin_l", static_cast<int64_t>(line.Margin[0]),
			"margin_r", static_cast<int64_t>(line.Margin[1]),
			"margin_v", static_cast<int64_t>(line.Margin[2]),
			"text", line.Text.get(),
			"cps", cps(line),
			"fold", fold_marker(&line),
			"fold_opener", line.Fold.hasFold() && !line.Fold.isEnd(),
			// Lines in a closed fold aren't shown in the grid
			"visible", line.Fold.isVisible()
		);
	}

	json::Object file_info(agi::Context *c, Args = {}) {
		int res_x = 0, res_y = 0;
		c->ass->GetResolution(res_x, res_y);
		auto active = c->selectionController->GetActiveLine();
		return json_io::Obj(
			"path", c->subsController->Filename().string(),
			"modified", c->subsController->IsModified(),
			"can_undo", !c->subsController->IsUndoStackEmpty(),
			"can_redo", !c->subsController->IsRedoStackEmpty(),
			"undo", c->subsController->GetUndoDescription(),
			"redo", c->subsController->GetRedoDescription(),
			"lines", static_cast<int64_t>(c->ass->Events.size()),
			"styles", static_cast<int64_t>(c->ass->Styles.size()),
			"res_x", static_cast<int64_t>(res_x),
			"res_y", static_cast<int64_t>(res_y),
			"title", std::string(c->ass->GetScriptInfo("Title")),
			"active_row", static_cast<int64_t>(active ? active->Row : -1)
		);
	}

	/// Ask whether to save the changes before closing the file, like the
	/// desktop program does
	/// @return false if the user cancelled
	bool try_to_close(agi::Context *c) {
		return c->subsController->TryToClose() != ui::CANCEL;
	}

	json::Object open(agi::Context *c, Args args) {
		auto path = json_io::GetString(args, "path");
		if (path.empty())
			throw agi::InvalidInputException("No file to open");
		if (!try_to_close(c))
			return json_io::Obj("cancelled", true);
		c->project->LoadSubtitles(agi::fs::path(path), json_io::GetString(args, "charset"));
		auto info = file_info(c);
		info["opened"] = c->subsController->Filename() == agi::fs::path(path);
		return info;
	}

	json::Object new_file(agi::Context *c, Args) {
		if (!try_to_close(c))
			return json_io::Obj("cancelled", true);
		c->project->CloseSubtitles();
		return file_info(c);
	}

	json::Object save(agi::Context *c, Args args) {
		auto path = json_io::GetString(args, "path");
		agi::fs::path file = path.empty() ? c->subsController->Filename() : agi::fs::path(path);
		if (file.empty())
			throw agi::InvalidInputException("The file has no name yet");
		c->subsController->Save(file, json_io::GetString(args, "charset").c_str());
		return file_info(c);
	}

	json::Object export_ytt(agi::Context *c, Args args) {
		auto path = json_io::GetString(args, "path");
		if (path.empty())
			throw agi::InvalidInputException("No file to export to");
		ExportYtt(c->ass.get(), c->project->Timecodes(), agi::fs::path(path));
		return {};
	}

	json::Object lines(agi::Context *c, Args args) {
		int64_t start = json_io::GetInt(args, "start", 0);
		int64_t count = json_io::GetInt(args, "count", -1);
		json::Array result;
		for (auto const& line : c->ass->Events) {
			if (line.Row < start) continue;
			if (count >= 0 && line.Row >= start + count) break;
			result.push_back(line_to_json(line));
		}
		return json_io::Obj(
			"total", static_cast<int64_t>(c->ass->Events.size()),
			"lines", std::move(result)
		);
	}

	/// Change fields of lines. Arguments:
	/// - changes: [{row, and the fields to change, named as lines() names them}]
	/// - description: what the undo menu calls the change
	/// - amend: add the changes to the previous set_lines call's undo step
	json::Object set_lines(agi::Context *c, Args args) {
		auto changes = json_io::GetArray(args, "changes");
		if (!changes || changes->empty()) return file_info(c);

		auto rows = all_lines(c);
		int type = 0;
		AssDialogue *changed_line = nullptr;
		for (auto const& change_value : *changes) {
			if (json_io::TypeOf(change_value) != json_io::Type::Object) continue;
			auto const& change = static_cast<json::Object const&>(change_value);
			auto line = line_at(rows, json_io::GetInt(change, "row", -1));
			changed_line = line;

			auto has = [&](const char *key) { return change.find(key) != change.end(); };
			if (has("text")) {
				line->Text = json_io::GetString(change, "text");
				type |= AssFile::COMMIT_DIAG_TEXT;
			}
			if (has("start")) {
				line->Start = static_cast<int>(json_io::GetInt(change, "start"));
				type |= AssFile::COMMIT_DIAG_TIME;
			}
			if (has("end")) {
				line->End = static_cast<int>(json_io::GetInt(change, "end"));
				type |= AssFile::COMMIT_DIAG_TIME;
			}
			if (has("comment")) {
				line->Comment = json_io::GetBool(change, "comment");
				type |= AssFile::COMMIT_DIAG_META;
			}
			if (has("layer")) {
				line->Layer = static_cast<int>(json_io::GetInt(change, "layer"));
				type |= AssFile::COMMIT_DIAG_META;
			}
			if (has("style")) {
				line->Style = json_io::GetString(change, "style");
				type |= AssFile::COMMIT_DIAG_META;
			}
			if (has("actor")) {
				line->Actor = json_io::GetString(change, "actor");
				type |= AssFile::COMMIT_DIAG_META;
			}
			if (has("effect")) {
				line->Effect = json_io::GetString(change, "effect");
				type |= AssFile::COMMIT_DIAG_META;
			}
			const char *margins[] = {"margin_l", "margin_r", "margin_v"};
			for (int i = 0; i < 3; ++i) {
				if (has(margins[i])) {
					line->Margin[i] = static_cast<int>(json_io::GetInt(change, margins[i]));
					type |= AssFile::COMMIT_DIAG_META;
				}
			}
		}
		if (!type) return file_info(c);

		auto description = json_io::GetString(args, "description", "modify line");
		bool amend = json_io::GetBool(args, "amend");
		last_edit_commit = c->ass->Commit(description, type, amend ? last_edit_commit : -1,
			changes->size() == 1 ? changed_line : nullptr);
		return file_info(c);
	}

	/// Open or close the fold which starts at a line, as clicking the
	/// desktop grid's fold column does
	json::Object toggle_fold(agi::Context *c, Args args) {
		auto line = line_at(all_lines(c), json_io::GetInt(args, "row", -1));
		if (line->Fold.hasFold() && !line->Fold.isEnd())
			c->foldController->ToggleFoldsAt({line});
		return {};
	}

	/// The recent files of a list: Subtitle, Video, Audio, Timecodes or
	/// Keyframes. The recent/subtitle/N commands open them.
	json::Object recent(agi::Context *, Args args) {
		json::Array files;
		for (auto const& file : *config::mru->Get(json_io::GetString(args, "list", "Subtitle")))
			files.emplace_back(file.string());
		return json_io::Obj("files", std::move(files));
	}

	/// The desktop program's menus, as default_menu.json describes them
	json::Object menus(agi::Context *, Args) {
		return json_io::Obj("menus", json_io::ReadObject(GET_DEFAULT_CONFIG(default_menu)));
	}

	json::Object styles(agi::Context *c, Args) {
		json::Array result;
		for (auto const& style : c->ass->Styles)
			result.push_back(android::dialogs::StyleToJson(style));
		return json_io::Obj("styles", std::move(result));
	}

	json::Object script_info(agi::Context *c, Args) {
		json::Object info;
		for (auto const& entry : c->ass->Info)
			info[std::string(entry.Key())] = std::string(entry.Value());
		return json_io::Obj("info", std::move(info));
	}

	json::Object selection(agi::Context *c, Args = {}) {
		json::Array rows;
		for (auto line : c->selectionController->GetSortedSelection())
			rows.push_back(static_cast<int64_t>(line->Row));
		auto active = c->selectionController->GetActiveLine();
		return json_io::Obj(
			"rows", std::move(rows),
			"active", static_cast<int64_t>(active ? active->Row : -1)
		);
	}

	json::Object select(agi::Context *c, Args args) {
		auto lines = all_lines(c);
		Selection sel;
		if (auto rows = json_io::GetArray(args, "rows")) {
			for (auto const& row : *rows) {
				if (json_io::TypeOf(row) == json_io::Type::Int)
					sel.insert(line_at(lines, static_cast<json::Integer const&>(row)));
			}
		}
		int64_t active_row = json_io::GetInt(args, "active", -1);
		AssDialogue *active = active_row >= 0 ? line_at(lines, active_row) : nullptr;
		if (active)
			sel.insert(active);
		else if (!sel.empty())
			active = *sel.begin();
		c->selectionController->SetSelectionAndActive(std::move(sel), active);
		return selection(c);
	}

	json::Object undo(agi::Context *c, Args) {
		c->subsController->Undo();
		return file_info(c);
	}

	json::Object redo(agi::Context *c, Args) {
		c->subsController->Redo();
		return file_info(c);
	}

	json::Object script_to_json(agi::Context *c, Automation4::Script const& script, bool custom) {
		json::Array macros;
		for (auto macro : script.GetMacros()) {
			macros.push_back(json_io::Obj(
				"command", macro->name(),
				"name", macro->StrDisplay(c),
				"help", macro->StrHelp()
			));
		}

		json::Array warnings;
		for (auto const& warning : script.GetWarnings())
			warnings.emplace_back(warning);

		return json_io::Obj(
			"path", script.GetFilename().string(),
			"file", script.GetPrettyFilename().string(),
			"name", script.GetName(),
			"description", script.GetDescription(),
			"author", script.GetAuthor(),
			"version", script.GetVersion(),
			"loaded", script.GetLoadedState(),
			"warnings", std::move(warnings),
			"custom", custom,
			"macros", std::move(macros)
		);
	}

	/// The automation scripts which are loaded, and their macros
	json::Object macros(agi::Context *c, Args) {
		// The user's own scripts are the ones in the TypeMotion folder
		auto custom_dir = (android::StorageFolder() / "automation").string();
		json::Array global;
		for (auto const& script : config::global_scripts->GetScripts()) {
			auto path = script->GetFilename().string();
			bool custom = path.compare(0, custom_dir.size(), custom_dir) == 0;
			global.push_back(script_to_json(c, *script, custom));
		}

		json::Array local;
		for (auto const& script : c->local_scripts->GetScripts())
			local.push_back(script_to_json(c, *script, true));

		return json_io::Obj(
			"global", std::move(global),
			"local", std::move(local)
		);
	}

	/// Which macros can run on the current selection, as the desktop
	/// program's automation menu shows them
	json::Object validate_macros(agi::Context *c, Args) {
		json::Object valid;
		auto add = [&](std::vector<cmd::Command *> const& commands) {
			for (auto command : commands)
				valid[command->name()] = command->Validate(c);
		};
		add(config::global_scripts->GetMacros());
		add(c->local_scripts->GetMacros());
		return json_io::Obj("valid", std::move(valid));
	}

	json::Object reload_scripts(agi::Context *c, Args) {
		config::global_scripts->Reload();
		return macros(c, {});
	}

	Automation4::Script *find_script(std::vector<std::unique_ptr<Automation4::Script>> const& scripts, std::string const& path) {
		for (auto const& script : scripts) {
			if (script->GetFilename().string() == path)
				return script.get();
		}
		return nullptr;
	}

	/// The Automation Manager's Add: load scripts for the open file only
	json::Object add_scripts(agi::Context *c, Args args) {
		auto paths = json_io::GetArray(args, "paths");
		if (paths) {
			for (auto const& value : *paths) {
				if (json_io::TypeOf(value) != json_io::Type::String) continue;
				agi::fs::path path(static_cast<json::String const&>(value));
				OPT_SET("Path/Last/Automation")->SetString(path.parent_path().string());

				if (find_script(c->local_scripts->GetScripts(), path.string()) || find_script(config::global_scripts->GetScripts(), path.string())) {
					ui::LogError(agi::format("Script '%s' is already loaded", path.string()));
					continue;
				}
				c->local_scripts->Add(Automation4::ScriptFactory::CreateFromFile(path, true));
			}
		}
		return macros(c, {});
	}

	/// The Automation Manager's Remove, which only takes out the open file's scripts
	json::Object remove_script(agi::Context *c, Args args) {
		if (auto script = find_script(c->local_scripts->GetScripts(), json_io::GetString(args, "path")))
			c->local_scripts->Remove(script);
		return macros(c, {});
	}

	/// The Automation Manager's Reload, for one script
	json::Object reload_script(agi::Context *c, Args args) {
		auto path = json_io::GetString(args, "path");
		if (auto script = find_script(config::global_scripts->GetScripts(), path))
			static_cast<Automation4::ScriptManager *>(config::global_scripts)->Reload(script);
		else if (auto script = find_script(c->local_scripts->GetScripts(), path))
			c->local_scripts->Reload(script);
		return macros(c, {});
	}

	/// The scripting engines, for the Automation Manager's info and its file dialog
	json::Object automation_info(agi::Context *, Args) {
		json::Array engines;
		for (auto const& factory : Automation4::ScriptFactory::GetFactories()) {
			if (factory)
				engines.push_back(json_io::Obj("name", factory->GetEngineName(), "pattern", factory->GetFilenamePattern()));
		}
		return json_io::Obj(
			"engines", std::move(engines),
			"wildcard", Automation4::ScriptFactory::GetWildcardStr(),
			"last_dir", OPT_GET("Path/Last/Automation")->GetString()
		);
	}

	/// Run a command by name, such as a macro's command from macros()
	json::Object run_command(agi::Context *c, Args args) {
		auto name = json_io::GetString(args, "name");
		auto command = cmd::get(name);
		if ((command->Type() & cmd::COMMAND_VALIDATE) && !command->Validate(c))
			throw agi::InvalidInputException(name + " can't run on the selected lines");
		(*command)(c);
		return file_info(c);
	}

	/// The program's commands, for the app's menus and buttons
	json::Object commands(agi::Context *c, Args) {
		json::Array list;
		for (auto name : cmd::get_registered_commands()) {
			auto command = cmd::get(name);
			int type = command->Type();
			list.push_back(json_io::Obj(
				"name", std::string(name),
				"menu", command->StrMenu(c),
				"display", command->StrDisplay(c),
				"help", command->StrHelp(),
				"validate", (type & cmd::COMMAND_VALIDATE) != 0,
				"toggle", (type & cmd::COMMAND_TOGGLE) != 0,
				"radio", (type & cmd::COMMAND_RADIO) != 0,
				"dynamic_name", (type & cmd::COMMAND_DYNAMIC_NAME) != 0
			));
		}
		return json_io::Obj("commands", std::move(list));
	}

	/// How the desktop program's menus would show the given commands now:
	/// whether they can run, whether they're checked and what they're called
	json::Object command_state(agi::Context *c, Args args) {
		json::Object valid, active, menu;
		if (auto names = json_io::GetArray(args, "names")) {
			for (auto const& value : *names) {
				if (json_io::TypeOf(value) != json_io::Type::String) continue;
				std::string const& name = static_cast<json::String const&>(value);
				cmd::Command *command;
				try {
					command = cmd::get(name);
				}
				catch (cmd::CommandNotFound const&) {
					continue;
				}

				int type = command->Type();
				valid[name] = !(type & cmd::COMMAND_VALIDATE) || command->Validate(c);
				if (type & (cmd::COMMAND_TOGGLE | cmd::COMMAND_RADIO))
					active[name] = command->IsActive(c);
				if (type & cmd::COMMAND_DYNAMIC_NAME)
					menu[name] = command->StrMenu(c);
			}
		}
		return json_io::Obj(
			"valid", std::move(valid),
			"active", std::move(active),
			"menu", std::move(menu)
		);
	}

	json::Object list_fonts(agi::Context *, Args) {
		json::Array families;
		for (auto const& family : fonts::Families())
			families.emplace_back(family);
		return json_io::Obj("families", std::move(families));
	}

	json::Object get_option(agi::Context *, Args args) {
		auto opt = config::opt->Get(json_io::GetString(args, "name"));
		switch (opt->GetType()) {
			case agi::OptionType::String: return json_io::Obj("value", opt->GetString());
			case agi::OptionType::Int: return json_io::Obj("value", opt->GetInt());
			case agi::OptionType::Double: return json_io::Obj("value", opt->GetDouble());
			case agi::OptionType::Bool: return json_io::Obj("value", opt->GetBool());
			case agi::OptionType::Color: return json_io::Obj("value", opt->GetColor().GetRgbFormatted());
			default: throw agi::InvalidInputException("Lists can't be read yet");
		}
	}

	json::Object set_option(agi::Context *, Args args) {
		auto opt = config::opt->Get(json_io::GetString(args, "name"));
		switch (opt->GetType()) {
			case agi::OptionType::String: opt->SetString(json_io::GetString(args, "value")); break;
			case agi::OptionType::Int: opt->SetInt(json_io::GetInt(args, "value")); break;
			case agi::OptionType::Double: opt->SetDouble(json_io::GetDouble(args, "value")); break;
			case agi::OptionType::Bool: opt->SetBool(json_io::GetBool(args, "value")); break;
			case agi::OptionType::Color: opt->SetColor(agi::Color(json_io::GetString(args, "value"))); break;
			default: throw agi::InvalidInputException("Lists can't be changed yet");
		}
		return {};
	}

	/// Write the options and recent files to disk, for when the app is
	/// about to be stopped
	json::Object flush(agi::Context *, Args) {
		config::opt->Flush();
		return {};
	}

	const std::map<std::string, Method, std::less<>> methods = {
		{"file_info", [](agi::Context *c, Args args) { return file_info(c, args); }},
		{"open", open},
		{"new", new_file},
		{"save", save},
		{"export_ytt", export_ytt},
		{"lines", lines},
		{"set_lines", set_lines},
		{"toggle_fold", toggle_fold},
		{"styles", styles},
		{"script_info", script_info},
		{"selection", [](agi::Context *c, Args args) { return selection(c, args); }},
		{"select", select},
		{"undo", undo},
		{"redo", redo},
		{"macros", macros},
		{"validate_macros", validate_macros},
		{"reload_scripts", reload_scripts},
		{"add_scripts", add_scripts},
		{"remove_script", remove_script},
		{"reload_script", reload_script},
		{"automation_info", automation_info},
		{"run_command", run_command},
		{"commands", commands},
		{"command_state", command_state},
		{"fonts", list_fonts},
		{"recent", recent},
		{"menus", menus},
		{"get_option", get_option},
		{"set_option", set_option},
		{"flush", flush},

		// The edit box (edit_box.h)
		{"edit_box", android::edit_box::State},
		{"edit_box_set", android::edit_box::Set},
		{"set_text_selection", android::edit_box::SetTextSelection},

		// The screens which do what desktop dialogs do (dialog_methods.h)
		{"shift_times_state", android::dialogs::ShiftTimesState},
		{"shift_times", [](agi::Context *c, Args args) { android::dialogs::ShiftTimes(c, args); return file_info(c); }},
		{"clear_shift_history", [](agi::Context *c, Args args) { android::dialogs::ClearShiftHistory(c, args); return json::Object(); }},
		{"select_lines_state", android::dialogs::SelectLinesState},
		{"select_lines", android::dialogs::SelectLines},
		{"search_state", android::dialogs::SearchState},
		{"search", android::dialogs::Search},
		{"properties", android::dialogs::Properties},
		{"set_properties", android::dialogs::SetProperties},
		{"catalogs", android::dialogs::Catalogs},
		{"set_catalog", android::dialogs::SetCatalog},
		{"new_catalog", android::dialogs::NewCatalog},
		{"delete_catalog", android::dialogs::DeleteCatalog},
		{"catalog_styles", android::dialogs::CatalogStyles},
		{"copy_style_name", android::dialogs::CopyStyleName},
		{"save_style", android::dialogs::SaveStyle},
		{"delete_styles", android::dialogs::DeleteStyles},
		{"move_styles", android::dialogs::MoveStyles},
		{"copy_styles_to_catalog", android::dialogs::CopyStylesToCatalog},
		{"copy_styles_to_script", android::dialogs::CopyStylesToScript},
		{"style_encodings", android::dialogs::StyleEncodings},

		// Video and audio (media.h)
		{"video_state", android::media::State},
		{"open_video", android::media::OpenVideo},
		{"close_video", android::media::CloseVideo},
		{"open_audio", android::media::OpenAudio},
		{"close_audio", android::media::CloseAudio},
		{"video_seek", android::media::Seek},
		{"video_play", android::media::Play},
		{"video_stop", android::media::Stop},
		{"video_position", android::media::Position},
	};
}

namespace android {
	json::Object CallMethod(agi::Context *c, std::string const& method, json::Object const& args) {
		auto it = methods.find(method);
		if (it == methods.end())
			return json_io::Obj("error", "Unknown method: " + method);
		return it->second(c, args);
	}

	void ConnectEvents(agi::Context *c) {
		edit_box::Connect(c);
		media::Connect(c);
		connections.push_back(c->ass->AddCommitListener([](int type, const AssDialogue *line) {
			auto data = json_io::Obj("type", static_cast<int64_t>(type));
			if (line)
				data["row"] = static_cast<int64_t>(line->Row);
			bridge::Event("commit", std::move(data));
		}));
		connections.push_back(c->selectionController->AddSelectionListener([c] {
			bridge::Event("selection", selection(c));
		}));
		connections.push_back(c->selectionController->AddActiveLineListener([c](AssDialogue *) {
			bridge::Event("selection", selection(c));
		}));
		connections.push_back(c->subsController->AddFileOpenListener([](agi::fs::path const& path) {
			bridge::Event("file_open", json_io::Obj("path", path.string()));
		}));
		connections.push_back(c->subsController->AddFileSaveListener([c] {
			bridge::Event("file_save", json_io::Obj("path", c->subsController->Filename().string()));
		}));
		connections.push_back(config::global_scripts->AddScriptChangeListener([] {
			bridge::Event("scripts_changed");
		}));
		connections.push_back(c->local_scripts->AddScriptChangeListener([] {
			bridge::Event("scripts_changed");
		}));
	}

	void DisconnectEvents() {
		connections.clear();
		edit_box::Disconnect();
		media::Disconnect();
	}
}
