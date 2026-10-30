// Copyright (c) 2006, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "auto4_base.h"
#include <libaegisub/io.h>
#include <memory>
#include <libaegisub/format.h>


#include "ass_file.h"
#include "ass_style.h"
#include "i18n.h"
#include "include/aegisub/context.h"
#include "options.h"
#include "subs_controller.h"
#include "ui_hooks.h"

#include <libaegisub/ass/string_codec.h>
#include <libaegisub/dispatch.h>
#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/split.h>
#include <libaegisub/string.h>

#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <future>
#include <ranges>

namespace Automation4 {
	ExportFilter::ExportFilter(std::string const& name, std::string const& description, int priority)
	: AssExportFilter(name, description, priority)
	{
	}

	std::string ExportFilter::GetScriptSettingsIdentifier()
	{
		return agi::ass::inline_string_encode(GetName());
	}

	void ExportFilter::LoadSettings(bool, agi::Context *c) {
		if (config_dialog)
			c->ass->Properties.automation_settings[GetScriptSettingsIdentifier()] = config_dialog->Serialise();
	}

	// ProgressSink
	ProgressSink::ProgressSink(agi::ProgressSink *impl, BackgroundScriptRunner *bsr)
	: impl(impl)
	, bsr(bsr)
	, trace_level(OPT_GET("Automation/Trace Level")->GetInt())
	{
	}

	BackgroundScriptRunner::BackgroundScriptRunner(wxWindow *parent, std::string const& title)
	: impl(ui::CreateProgress(parent, to_ui(title), UiString()))
	{
	}

	BackgroundScriptRunner::~BackgroundScriptRunner()
	{
	}

	void BackgroundScriptRunner::Run(std::function<void (ProgressSink*)> task)
	{
		impl->Run([&](agi::ProgressSink *ps) {
			ProgressSink aps(ps, this);
			task(&aps);
		});
	}

	wxWindow *BackgroundScriptRunner::GetParentWindow() const
	{
		return ui::GetProgressWindow(impl.get());
	}

	std::string BackgroundScriptRunner::GetTitle() const
	{
		return ui::GetProgressTitle(impl.get());
	}

	// Script
	Script::Script(agi::fs::path const& filename)
	: filename(filename)
	{
		include_path.emplace_back(filename.parent_path());

		std::string include_paths = OPT_GET("Path/Automation/Include")->GetString();
		for (auto tok : agi::Split(include_paths, '|')) {
			auto path = config::path->Decode(std::string(tok));
			if (path.is_absolute() && agi::fs::DirectoryExists(path))
				include_path.emplace_back(std::move(path));
		}
	}

	// ScriptManager
	void ScriptManager::Add(std::unique_ptr<Script> script)
	{
		if (find(scripts.begin(), scripts.end(), script) == scripts.end())
			scripts.emplace_back(std::move(script));

		ScriptsChanged();
	}

	void ScriptManager::Remove(Script *script)
	{
		auto i = find_if(scripts.begin(), scripts.end(), [&](std::unique_ptr<Script> const& s) { return s.get() == script; });
		if (i != scripts.end())
			scripts.erase(i);

		ScriptsChanged();
	}

	void ScriptManager::RemoveAll()
	{
		scripts.clear();
		ScriptsChanged();
	}

	void ScriptManager::Reload(Script *script)
	{
		script->Reload();
		ScriptsChanged();
	}

	const std::vector<cmd::Command*>& ScriptManager::GetMacros()
	{
		macros.clear();
		for (auto& script : scripts) {
			std::vector<cmd::Command*> sfs = script->GetMacros();
			copy(sfs.begin(), sfs.end(), back_inserter(macros));
		}
		return macros;
	}

	// AutoloadScriptManager
	AutoloadScriptManager::AutoloadScriptManager(std::string path, std::string unrestricted_path)
	: path(std::move(path))
	, unrestricted_path(std::move(unrestricted_path))
	{
		Reload();
	}


// Official automation scripts shipped with TypeMotion. This allowlist is
// compiled into the binary on purpose: it is hidden and tamper-proof, so a
// user cannot edit it to sneak custom scripts past the check. The build is
// sensitive and user-added scripts in the autoload folder can break it, so
// on every autoload scan any .lua that is not on this list is removed,
// leaving only the official tools. To add or remove an official tool, edit
// this list and rebuild.
static const std::set<std::string> official_autoload_scripts = {
	"CleanTags.lua",
	"FadeworksFX.lua",
	"GlitchFX.lua",
	"KaraokeFX.lua",
	"LightFX.lua",
	"OndasRGB_fx.lua",
	"TimeShifter.lua",
	"lyger.GradientByChar.lua",
};

// Delete any .lua in the autoload directories that is not an official script.
static void EnforceOfficialScripts(std::set<agi::fs::path> const& dirnames) {
	for (auto const& dirname : dirnames) {
		if (!agi::fs::DirectoryExists(dirname)) continue;

		// Collect first, then remove, so we don't mutate the directory while
		// iterating over it.
		std::vector<std::string> to_remove;
		for (auto filename : agi::fs::DirectoryIterator(dirname, "*.lua")) {
			if (!official_autoload_scripts.count(filename))
				to_remove.push_back(filename);
		}
		for (auto const& filename : to_remove) {
			// Ignore failures (e.g. a read-only install dir such as Program
			// Files when not elevated) so a non-removable file never aborts
			// the autoload scan.
			try { agi::fs::Remove(dirname / filename); }
			catch (...) { }
		}
	}
}

	void AutoloadScriptManager::Reload()
	{
		scripts.clear();

		std::set<agi::fs::path> dirnames;
		int error_count = 0;
		int warning_count = 0;

		for (auto tok : agi::Split(path, '|')) {
			auto dirname = config::path->Decode(std::string(tok));
			if (!agi::fs::DirectoryExists(dirname)) continue;

			if (dirnames.count(dirname)) continue;
			dirnames.insert(dirname);
		}

		EnforceOfficialScripts(dirnames);

		// Folders the user puts their own scripts in, which are loaded as they are
		std::vector<agi::fs::path> all_dirnames(dirnames.begin(), dirnames.end());
		for (auto tok : agi::Split(unrestricted_path, '|')) {
			auto dirname = config::path->Decode(std::string(tok));
			if (!agi::fs::DirectoryExists(dirname)) continue;

			if (std::find(all_dirnames.begin(), all_dirnames.end(), dirname) != all_dirnames.end()) continue;
			all_dirnames.push_back(dirname);
		}

		for (auto const& dirname : all_dirnames) {
			for (auto filename : agi::fs::DirectoryIterator(dirname, "*.*")) {
				auto s = ScriptFactory::CreateFromFile(dirname/filename, false, false);
				if (s) {
					if (!s->GetLoadedState()) ++error_count;
					if (!s->GetWarnings().empty()) ++warning_count;
					scripts.emplace_back(std::move(s));
				}
			}
		}

		if (error_count == 1) {
			ui::LogWarning(_("A script in the Automation autoload directory failed to load.\nPlease review the errors, fix them and use the Rescan Autoload Dir button in Automation Manager to load the scripts again."));
		}
		else if (error_count > 1) {
			ui::LogWarning(_("Multiple scripts in the Automation autoload directory failed to load.\nPlease review the errors, fix them and use the Rescan Autoload Dir button in Automation Manager to load the scripts again."));
		}
		else if (warning_count == 1) {
			ui::LogWarning(_("A script in the Automation autoload directory loaded with warnings.\nPlease review the warnings, fix them and use the Rescan Autoload Dir button in Automation Manager to load the scripts again."));
		}
		else if (warning_count > 1) {
			ui::LogWarning(_("Multiple scripts in the Automation autoload directory loaded with warnings.\nPlease review the warnings, fix them and use the Rescan Autoload Dir button in Automation Manager to load the scripts again."));
		}

		ScriptsChanged();
	}

	LocalScriptManager::LocalScriptManager(agi::Context *c)
	: context(c)
	{
		BindConnection(c->subsController->AddFileOpenListener(&LocalScriptManager::Reload, this));
		BindConnection(AddScriptChangeListener(&LocalScriptManager::SaveLoadedList, this));
	}

	void LocalScriptManager::Reload()
	{
		bool was_empty = scripts.empty();
		scripts.clear();

		auto const& local_scripts = context->ass->Properties.automation_scripts;

		// Tuples of (raw script path, base path, resolved path)
		using PathTuple = std::tuple<std::string, agi::fs::path, agi::fs::path>;
		std::vector<PathTuple> resolvedpaths;

		auto autobasefn(OPT_GET("Path/Automation/Base")->GetString());

		for (auto tok : agi::Split(local_scripts, '|')) {
			tok = agi::Trim(tok);
			if (tok.size() == 0) continue;
			char first_char = tok[0];
			std::string trimmed(begin(tok) + 1, end(tok));

			agi::fs::path basepath;
			if (first_char == '~') {
				basepath = context->subsController->Filename().parent_path();
			} else if (first_char == '$') {
				basepath = autobasefn;
			} else if (first_char == '/') {
			} else {
				ui::LogWarning(fmt_tl("Automation Script referenced with unknown location specifier character.\nLocation specifier found: %c\nFilename specified: %s",
					first_char, to_ui(trimmed)));
				continue;
			}
			auto sfname = basepath/trimmed;

			resolvedpaths.emplace_back(tok, basepath, sfname);
		}

		if (resolvedpaths.empty()) {
			if (!was_empty)
				ScriptsChanged();
			return;
		}

		bool load = ui::ConfirmLoadingScripts(
				fmt_tl(
					"The subtitle file you have opened contains local automation scripts. "
					"Do you want to load the associated automation scripts?\n"
					"\n"
					"List of scripts:\n\n%s\n\n"
					"You should only load associated automation scripts if you trust their authors! "
					"They are not required to view or edit the subtitle file."
					, agi::Join("\n", std::views::transform(resolvedpaths, [](PathTuple const& paths) {return from_ui(fmt_tl("    %s (%s)", to_ui(std::get<0>(paths)), std::get<2>(paths).string()));})))
				, _("Load associated automation scripts?")
			);

		if (!load) {
			if (!was_empty)
				ScriptsChanged();
			else
				SaveLoadedList();
			return;
		}

		for (auto const& [tok, basepath, sfname] : resolvedpaths) {
			if (agi::fs::FileExists(sfname))
				scripts.emplace_back(Automation4::ScriptFactory::CreateFromFile(sfname, true));
			else {
				ui::LogWarning(fmt_tl("Automation Script referenced could not be found.\nFilename specified: %s\nSearched relative to: %s\nResolved filename: %s",
					to_ui(tok), basepath.string(), sfname.string()));
			}
		}

		ScriptsChanged();
	}

	void LocalScriptManager::SaveLoadedList()
	{
		// Store Automation script data
		// Algorithm:
		// 1. If script filename has Automation Base Path as a prefix, the path is relative to that (ie. "$")
		// 2. Otherwise try making it relative to the ass filename
		// 3. If step 2 failed, or absolute path is shorter than path relative to ass, use absolute path ("/")
		// 4. Otherwise, use path relative to ass ("~")
		std::string scripts_string;
		agi::fs::path autobasefn(OPT_GET("Path/Automation/Base")->GetString());

		for (auto& script : GetScripts()) {
			if (!scripts_string.empty())
				scripts_string += "|";

			auto scriptfn(script->GetFilename().string());
			auto autobase_rel = context->path->MakeRelative(scriptfn, autobasefn);
			auto assfile_rel = context->path->MakeRelative(scriptfn, std::string_view("?script"));

			if (autobase_rel.string().size() <= scriptfn.size() && autobase_rel.string().size() <= assfile_rel.string().size()) {
				scriptfn = "$" + autobase_rel.generic_string();
			} else if (assfile_rel.string().size() <= scriptfn.size() && assfile_rel.string().size() <= autobase_rel.string().size()) {
				scriptfn = "~" + assfile_rel.generic_string();
			} else {
				scriptfn = "/" + script->GetFilename().generic_string();
			}

			scripts_string += scriptfn;
		}
		context->ass->Properties.automation_scripts = std::move(scripts_string);
	}

	// ScriptFactory
	ScriptFactory::ScriptFactory(std::string engine_name, std::string filename_pattern)
	: engine_name(std::move(engine_name))
	, filename_pattern(std::move(filename_pattern))
	{
	}

	void ScriptFactory::Register(std::unique_ptr<ScriptFactory> factory)
	{
		if (find(Factories().begin(), Factories().end(), factory) != Factories().end())
			throw agi::InternalError("Automation 4: Attempt to register the same script factory multiple times. This should never happen.");

		Factories().emplace_back(std::move(factory));
	}

	std::unique_ptr<Script> ScriptFactory::CreateFromFile(agi::fs::path const& filename, bool complain_about_unrecognised, bool create_unknown)
	{
		for (auto& factory : Factories()) {
			auto s = factory->Produce(filename);
			if (s) {
				if (!s->GetLoadedState()) {
					ui::LogError(fmt_tl("Failed to load Automation script '%s':\n%s", filename.string(), s->GetDescription()));
				}
				for (auto const& warning : s->GetWarnings()) {
					ui::LogWarning(fmt_tl("Warning when loading Automation script '%s':\n%s", filename.string(), warning));
				}
				return s;
			}
		}

		if (complain_about_unrecognised) {
			ui::LogError(fmt_tl("The file was not recognised as an Automation script: %s", filename.string()));
		}

		return create_unknown ? std::make_unique<UnknownScript>(filename) : nullptr;
	}

	std::vector<std::unique_ptr<ScriptFactory>>& ScriptFactory::Factories()
	{
		static std::vector<std::unique_ptr<ScriptFactory>> factories;
		return factories;
	}

	const std::vector<std::unique_ptr<ScriptFactory>>& ScriptFactory::GetFactories()
	{
		return Factories();
	}

	std::string ScriptFactory::GetWildcardStr()
	{
		std::string fnfilter, catchall;
		for (auto& fact : Factories()) {
			if (fact->GetEngineName().empty() || fact->GetFilenamePattern().empty())
				continue;

			std::string filter(fact->GetFilenamePattern());
			boost::replace_all(filter, ",", ";");
			fnfilter += agi::format("%s scripts (%s)|%s|", fact->GetEngineName(), fact->GetFilenamePattern(), filter);
			catchall += filter + ";";
		}
		fnfilter += from_ui(_("All Files")) + " (*.*)|*.*";

		if (!catchall.empty())
			catchall.pop_back();

		if (Factories().size() > 1)
			fnfilter = agi::Str(from_ui(_("All Supported Formats")), "|", catchall, "|", fnfilter);

		return fnfilter;
	}

	std::string UnknownScript::GetDescription() const {
		return from_ui(_("File was not recognized as a script"));
	}
}
