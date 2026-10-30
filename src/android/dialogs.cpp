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

/// @file dialogs.cpp
/// @brief The dialogs of dialogs.h which the core's commands open
/// @ingroup android
///
/// The dialogs which only need the subtitles become an open_dialog event,
/// and the app shows its own screen for them, working through the engine's
/// methods. The ones which give an answer to the command which opened them
/// are requests, which work out what to show the way the desktop dialogs do.

#include "bridge.h"
#include "json_io.h"

#include "../ass_file.h"
#include "../async_video_provider.h"
#include "../dialogs.h"
#include "../i18n.h"
#include "../include/aegisub/context.h"
#include "../options.h"
#include "../project.h"
#include "../resolution_resampler.h"

#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/ycbcr.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <iterator>
#include <map>
#include <string_view>
#include <vector>

namespace {
	void open_dialog(std::string_view name, json::Object data = {}) {
		data["name"] = std::string(name);
		bridge::Event("open_dialog", std::move(data));
	}

	struct Version {
		std::string filename;
		std::time_t date;
		std::string display;
	};

	struct AutosaveFile {
		std::string name;
		std::vector<Version> versions;
	};

	/// Read a date of the form the autosaves are named with, as
	/// wxDateTime::ParseFormat(str, "%Y-%m-%d-%H-%M-%S") does
	bool parse_date(std::string const& str, std::time_t &date) {
		std::tm tm{};
		int length = 0;
		if (std::sscanf(str.c_str(), "%4d-%2d-%2d-%2d-%2d-%2d%n", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &length) != 6
			|| length != static_cast<int>(str.size()))
			return false;
		tm.tm_year -= 1900;
		tm.tm_mon -= 1;
		tm.tm_isdst = -1;
		date = std::mktime(&tm);
		return date != -1;
	}

	std::string format_date(std::time_t date) {
		char buf[64];
		std::tm tm{};
		localtime_r(&date, &tm);
		return std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tm) ? buf : "";
	}

	std::time_t modified_time(agi::fs::path const& file) {
		try {
			auto time = agi::fs::ModifiedTime(file);
			// The NDK's libc++ has no clock_cast, so convert through now()
			using file_clock = decltype(time)::clock;
			auto sys_time = std::chrono::system_clock::now() +
				std::chrono::duration_cast<std::chrono::system_clock::duration>(time - file_clock::now());
			return std::chrono::system_clock::to_time_t(sys_time);
		}
		catch (agi::fs::FileSystemError const&) {
			return 0;
		}
	}

	/// The same as DialogAutosave::Populate
	void populate(std::map<std::string, AutosaveFile> &files_map, std::string const& path, std::string const& filter, std::string const& name_fmt) {
		auto directory = config::path->Decode(path);
		for (auto const& fn : agi::fs::DirectoryIterator(directory, "*" + filter)) {
			std::time_t date = 0;
			bool has_date = false;

			std::string stem = fn.substr(0, fn.size() - filter.size());
			std::string name, date_str;
			auto dot = stem.rfind('.');
			if (dot == std::string::npos)
				date_str = stem;
			else {
				name = stem.substr(0, dot);
				date_str = stem.substr(dot + 1);
			}
			if (name.empty())
				name = date_str;
			else {
				has_date = parse_date(date_str, date);
				if (!has_date)
					name += "." + date_str;
			}
			if (!has_date)
				date = modified_time(directory / fn);

			auto it = files_map.find(name);
			if (it == files_map.end())
				it = files_map.insert({name, AutosaveFile{name, {}}}).first;
			it->second.versions.push_back(Version{(directory / fn).string(), date, agi::format(name_fmt, format_date(date))});
		}
	}

	/// The resampling dialog's choices of colour matrix
	constexpr std::string_view MatrixOptions[] = {
		"",
		"TV.601", "PC.601",
		"TV.709", "PC.709",
		"TV.FCC", "PC.FCC",
		"TV.240M", "PC.240M",
	};

	std::string matrix_option(agi::ycbcr::Header header) {
		std::string header_string = header.to_existing_string();
		if (std::ranges::find(MatrixOptions, header_string) != std::end(MatrixOptions))
			return header_string;
		return "";
	}

	/// The value of a number field, kept in the range the desktop dialog's
	/// spin control allows
	int spin_value(json::Object const& answer, std::string_view key, int def, int min, int max) {
		return static_cast<int>(std::clamp<int64_t>(json_io::GetInt(answer, key, def), min, max));
	}
}

bool GetColorFromUser(wxWindow *, agi::Color original, bool alpha, std::function<void (agi::Color)> callback) {
	// The alpha is the one of ASS, where 0 is opaque and 255 is transparent
	auto answer = bridge::Request("color", json_io::Obj(
		"r", static_cast<int64_t>(original.r),
		"g", static_cast<int64_t>(original.g),
		"b", static_cast<int64_t>(original.b),
		"a", static_cast<int64_t>(original.a),
		"show_alpha", alpha
	));
	if (!json_io::GetBool(answer, "ok"))
		return false;

	auto component = [&](std::string_view key, unsigned char def) {
		return static_cast<unsigned char>(std::clamp<int64_t>(json_io::GetInt(answer, key, def), 0, 255));
	};
	callback(agi::Color(
		component("r", original.r),
		component("g", original.g),
		component("b", original.b),
		alpha ? component("a", original.a) : original.a));
	return true;
}

std::string PickAutosaveFile(wxWindow *) {
	std::map<std::string, AutosaveFile> files_map;
	populate(files_map, OPT_GET("Path/Auto/Save")->GetString(), ".AUTOSAVE.ass", "%s");
	populate(files_map, OPT_GET("Path/Auto/Backup")->GetString(), ".ORIGINAL.ass", _("%s [ORIGINAL BACKUP]"));
	populate(files_map, "?user/recovered", ".ass", _("%s [RECOVERED]"));

	std::vector<AutosaveFile> files;
	for (auto& file : files_map)
		files.emplace_back(std::move(file.second));

	for (auto& file : files) {
		sort(begin(file.versions), end(file.versions),
			[](Version const& a, Version const& b) { return a.date > b.date; });
	}

	sort(begin(files), end(files),
		[](AutosaveFile const& a, AutosaveFile const& b) { return a.versions[0].date > b.versions[0].date; });

	json::Array file_list;
	for (auto const& file : files) {
		json::Array versions;
		for (auto const& version : file.versions) {
			versions.push_back(json_io::Obj(
				"path", version.filename,
				"display", version.display,
				"time", static_cast<int64_t>(version.date)
			));
		}
		file_list.push_back(json_io::Obj(
			"name", file.name,
			"versions", std::move(versions)
		));
	}

	auto answer = bridge::Request("pick_autosave", json_io::Obj("files", std::move(file_list)));
	auto path = json_io::GetString(answer, "path");
	// Only open one of the files offered
	for (auto const& file : files) {
		for (auto const& version : file.versions) {
			if (version.filename == path)
				return path;
		}
	}
	return "";
}

bool ShowPasteOverDialog(wxWindow *) {
	const char *choices[] = {
		"Comment", "Layer", "Start Time", "End Time", "Style", "Actor",
		"Margin Left", "Margin Right", "Margin Vertical", "Effect", "Text"
	};

	std::vector<bool> options = OPT_GET("Tool/Paste Lines Over/Fields")->GetListBool();
	if (options.size() != std::size(choices))
		options.resize(std::size(choices), false);

	json::Array fields;
	for (size_t i = 0; i < std::size(choices); ++i) {
		fields.push_back(json_io::Obj(
			"name", _(choices[i]),
			"checked", static_cast<bool>(options[i])
		));
	}

	auto answer = bridge::Request("paste_over", json_io::Obj("fields", std::move(fields)));
	if (!json_io::GetBool(answer, "ok"))
		return false;

	auto checked = json_io::GetArray(answer, "checked");
	if (!checked)
		return false;
	for (size_t i = 0; i < options.size(); ++i) {
		options[i] = i < checked->size()
			&& json_io::TypeOf((*checked)[i]) == json_io::Type::Bool
			&& static_cast<json::Boolean const&>((*checked)[i]);
	}
	OPT_SET("Tool/Paste Lines Over/Fields")->SetListBool(std::move(options));
	return true;
}

bool PromptForResampleSettings(agi::Context *c, ResampleSettings &settings) {
	auto script_mat = agi::ycbcr::Header(std::string(c->ass->GetScriptInfo("YCbCr Matrix"))).to_effective();
	int script_w, script_h;
	c->ass->GetResolution(script_w, script_h);
	int video_w = 0, video_h = 0;
	agi::ycbcr::header_colorspace video_mat;

	settings.source_x = script_w;
	settings.source_y = script_h;
	auto provider = c->project->VideoProvider();
	if (provider) {
		settings.dest_x = video_w = provider->GetWidth();
		settings.dest_y = video_h = provider->GetHeight();
		video_mat = provider->GetRealColorSpace();
	}
	else {
		settings.dest_x = script_w;
		settings.dest_y = script_h;
	}

	// Suggest resampling the colours when the script was made for another
	// colour matrix than the video's, as the desktop dialog does
	std::string source_matrix, dest_matrix;
	agi::ycbcr::Header video_mat_header(video_mat);
	if (video_mat_header != script_mat
			&& video_mat_header.valid()
			&& script_mat.valid()
			&& std::holds_alternative<agi::ycbcr::header_colorspace>(video_mat_header)
			&& std::holds_alternative<agi::ycbcr::header_colorspace>(script_mat)
	) {
		source_matrix = matrix_option(script_mat);
		dest_matrix = matrix_option(video_mat_header);
	}

	json::Array matrices;
	for (auto matrix : MatrixOptions)
		matrices.emplace_back(std::string(matrix));

	// In the order of ResampleARMode
	json::Array ar_modes;
	for (auto mode : {"Stretch", "Add borders", "Remove borders", "Manual"})
		ar_modes.emplace_back(_(mode));

	auto answer = bridge::Request("resample", json_io::Obj(
		"script_x", static_cast<int64_t>(script_w),
		"script_y", static_cast<int64_t>(script_h),
		"script_matrix", matrix_option(script_mat),
		"has_video", provider != nullptr,
		"video_x", static_cast<int64_t>(video_w),
		"video_y", static_cast<int64_t>(video_h),
		"video_matrix", matrix_option(video_mat_header),
		"source_x", static_cast<int64_t>(settings.source_x),
		"source_y", static_cast<int64_t>(settings.source_y),
		"dest_x", static_cast<int64_t>(settings.dest_x),
		"dest_y", static_cast<int64_t>(settings.dest_y),
		"source_matrix", source_matrix,
		"dest_matrix", dest_matrix,
		"matrices", std::move(matrices),
		"ar_modes", std::move(ar_modes),
		"ar_mode", static_cast<int64_t>(settings.ar_mode)
	));
	if (!json_io::GetBool(answer, "ok"))
		return false;

	settings.source_x = spin_value(answer, "source_x", settings.source_x, 1, 999999);
	settings.source_y = spin_value(answer, "source_y", settings.source_y, 1, 999999);
	settings.dest_x = spin_value(answer, "dest_x", settings.dest_x, 1, 999999);
	settings.dest_y = spin_value(answer, "dest_y", settings.dest_y, 1, 999999);

	auto ar_mode = json_io::GetInt(answer, "ar_mode", 0);
	if (ar_mode >= static_cast<int64_t>(ResampleARMode::Stretch) && ar_mode <= static_cast<int64_t>(ResampleARMode::Manual))
		settings.ar_mode = static_cast<ResampleARMode>(ar_mode);

	// In the order of ResampleSettings::margin
	const char *margins[] = {"margin_left", "margin_right", "margin_top", "margin_bottom"};
	for (size_t i = 0; i < std::size(margins); ++i)
		settings.margin[i] = spin_value(answer, margins[i], 0, -9999, 9999);

	agi::ycbcr::Header src_mat(json_io::GetString(answer, "source_matrix"));
	agi::ycbcr::Header dst_mat(json_io::GetString(answer, "dest_matrix"));
	if (std::holds_alternative<agi::ycbcr::header_colorspace>(src_mat) && std::holds_alternative<agi::ycbcr::header_colorspace>(dst_mat))
		settings.matrix_conversion = std::make_pair(std::get<agi::ycbcr::header_colorspace>(src_mat), std::get<agi::ycbcr::header_colorspace>(dst_mat));
	else
		settings.matrix_conversion = std::nullopt;
	return true;
}

void ShowAttachmentsDialog(wxWindow *, AssFile *) { open_dialog("attachments"); }
void ShowAutomationDialog(agi::Context *) { open_dialog("automation"); }
void ShowExportDialog(agi::Context *) { open_dialog("export"); }
void ShowFontsCollectorDialog(agi::Context *) { open_dialog("fonts_collector"); }
void ShowKanjiTimerDialog(agi::Context *) { open_dialog("kanji_timer"); }
void ShowPropertiesDialog(agi::Context *) { open_dialog("properties"); }
void ShowSearchReplaceDialog(agi::Context *, bool replace) { open_dialog("search_replace", json_io::Obj("replace", replace)); }
void ShowSelectLinesDialog(agi::Context *) { open_dialog("select_lines"); }
void ShowShiftTimesDialog(agi::Context *) { open_dialog("shift_times"); }
void ShowSpellcheckerDialog(agi::Context *) { open_dialog("spellchecker"); }
void ShowStyleManagerDialog(agi::Context *) { open_dialog("style_manager"); }
void ShowTimingProcessorDialog(agi::Context *) { open_dialog("timing_processor"); }
