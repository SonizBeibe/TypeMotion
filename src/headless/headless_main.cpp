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

/// @file headless_main.cpp
/// @brief Command line tool which runs the subtitle core without an interface

#include "../ass_dialogue.h"
#include "../ass_file.h"
#include "../ass_style.h"
#include "../auto4_base.h"
#include "../auto4_lua_factory.h"
#include "../command/command.h"
#include "../font_index.h"
#include "../include/aegisub/context.h"
#include "../libresrc/libresrc.h"
#include "../options.h"
#include "../project.h"
#include "../selection_controller.h"
#include "../subs_controller.h"
#include "../task_queue.h"
#include "../ytt/ytt.h"
#include "../ytt_export.h"

#include <libaegisub/dispatch.h>
#include <libaegisub/exception.h>
#include <libaegisub/fs.h>
#include <libaegisub/log.h>
#include <libaegisub/path.h>
#include <libaegisub/util.h>

#include <boost/algorithm/string/predicate.hpp>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace config {
	agi::Options *opt = nullptr;
	agi::MRUManager *mru = nullptr;
	agi::Path *path = nullptr;
	Automation4::AutoloadScriptManager *global_scripts = nullptr;
}

namespace {
	const char usage[] =
		"Usage: typemotion-headless [--data DIR] [--fonts DIR]... COMMAND ARGS...\n"
		"\n"
		"Commands:\n"
		"  info FILE                       Show what a subtitle file contains\n"
		"  convert IN OUT                  Load IN and save it as OUT, in the format of OUT's\n"
		"                                  extension (.ass, .srt, .ssa, .sub, .txt, .ytt, ...)\n"
		"  macros SCRIPT                   List the macros of an automation script\n"
		"  run SCRIPT MACRO IN OUT         Run a macro on all dialogue lines of IN, then save OUT\n"
		"  command NAME IN OUT             Run one of the program's commands (such as\n"
		"                                  edit/line/duplicate) on all dialogue lines of IN,\n"
		"                                  then save OUT\n"
		"  extents FILE STYLE TEXT         Measure TEXT with a style of FILE, the way scripts do\n"
		"  fonts                           List the fonts found\n"
		"\n"
		"--data DIR is where the automation/include folder is (the ?data path).\n"
		"--fonts DIR adds a folder of fonts to the system ones.\n";

	/// Run the command on the main thread, running the tasks other threads
	/// queue for it at the same time
	int run_on_main(std::function<int()> const& command) {
		int ret = 1;
		bool done = false;
		agi::dispatch::Main().Async([&] {
			try {
				ret = command();
			}
			catch (agi::Exception const& e) {
				std::cerr << "Error: " << e.GetMessage() << '\n';
			}
			catch (ytt::ConversionError const& e) {
				std::cerr << "Error: " << e.what() << '\n';
			}
			catch (std::exception const& e) {
				std::cerr << "Error: " << e.what() << '\n';
			}
			done = true;
		});
		task_queue::Run([&] { return done; });
		return ret;
	}

	bool load(agi::Context *c, agi::fs::path const& file) {
		c->project->LoadSubtitles(file, "", false);
		return c->subsController->Filename() == file;
	}

	void save(agi::Context *c, agi::fs::path const& file) {
		if (agi::fs::HasExtension(file, "ytt"))
			ExportYtt(c->ass.get(), c->project->Timecodes(), file);
		else
			c->subsController->Save(file);
	}

	int info(agi::Context *c, agi::fs::path const& file) {
		if (!load(c, file)) return 1;
		std::cout << "Script resolution: " << c->ass->GetScriptInfoAsInt("PlayResX") << 'x' << c->ass->GetScriptInfoAsInt("PlayResY") << '\n';
		std::cout << "Styles: " << c->ass->Styles.size() << '\n';
		for (auto const& style : c->ass->Styles)
			std::cout << "  " << style.name << " (" << style.font << ", " << style.fontsize << ")\n";
		size_t comments = 0;
		for (auto const& line : c->ass->Events)
			comments += line.Comment;
		std::cout << "Lines: " << c->ass->Events.size() << " (" << comments << " comments)\n";
		return 0;
	}

	std::unique_ptr<Automation4::Script> load_script(agi::fs::path const& file) {
		auto script = Automation4::ScriptFactory::CreateFromFile(file, true, false);
		if (!script)
			throw agi::InvalidInputException("Not an automation script: " + file.string());
		if (!script->GetLoadedState())
			throw agi::InvalidInputException(script->GetDescription());
		for (auto const& warning : script->GetWarnings())
			std::cerr << "Warning: " << warning << '\n';
		return script;
	}

	int macros(agi::fs::path const& file) {
		auto script = load_script(file);
		std::cout << script->GetName() << ' ' << script->GetVersion() << '\n';
		for (auto macro : script->GetMacros())
			std::cout << "  " << macro->StrDisplay(nullptr) << "\n    " << macro->StrHelp() << '\n';
		return 0;
	}

	/// Select every dialogue line, with the first one active
	bool select_all(agi::Context *c) {
		Selection sel;
		for (auto& line : c->ass->Events)
			sel.insert(&line);
		if (sel.empty()) {
			std::cerr << "Error: the file has no dialogue lines\n";
			return false;
		}
		c->selectionController->SetSelectionAndActive(std::move(sel), &c->ass->Events.front());
		return true;
	}

	int run(agi::Context *c, agi::fs::path const& script_file, std::string_view name, agi::fs::path const& in, agi::fs::path const& out) {
		auto script = load_script(script_file);
		cmd::Command *macro = nullptr;
		for (auto m : script->GetMacros()) {
			if (m->StrDisplay(nullptr) == name || m->name() == name)
				macro = m;
		}
		if (!macro) {
			std::cerr << "Error: the script has no macro named " << name << '\n';
			return 1;
		}

		if (!load(c, in) || !select_all(c)) return 1;

		if (!macro->Validate(c)) {
			std::cerr << "Error: the macro can't run on these lines\n";
			return 1;
		}
		(*macro)(c);
		save(c, out);
		return 0;
	}

	int run_command(agi::Context *c, std::string const& name, agi::fs::path const& in, agi::fs::path const& out) {
		auto command = cmd::get(name);
		if (!load(c, in) || !select_all(c)) return 1;

		if ((command->Type() & cmd::COMMAND_VALIDATE) && !command->Validate(c)) {
			std::cerr << "Error: the command can't run on these lines\n";
			return 1;
		}
		(*command)(c);
		save(c, out);
		return 0;
	}

	int convert(agi::Context *c, agi::fs::path const& in, agi::fs::path const& out) {
		if (!load(c, in)) return 1;
		save(c, out);
		return 0;
	}

	int extents(agi::Context *c, agi::fs::path const& file, std::string const& style_name, std::string const& text) {
		if (!load(c, file)) return 1;
		auto style = c->ass->GetStyle(style_name);
		if (!style) {
			std::cerr << "Error: the file has no style named " << style_name << '\n';
			return 1;
		}

		if (auto match = fonts::Find(style->font, style->bold, style->italic)) {
			std::cout << "Font: " << match->face.path.string() << " #" << match->face.index;
			if (match->fake_bold) std::cout << " (fake bold)";
			if (match->fake_italic) std::cout << " (fake italic)";
			std::cout << '\n';
		}

		double width, height, descent, extlead;
		if (!Automation4::CalculateTextExtents(style, text, width, height, descent, extlead)) {
			std::cerr << "Error: could not measure the text\n";
			return 1;
		}
		std::cout << "width " << width << " height " << height << " descent " << descent << " extlead " << extlead << '\n';
		return 0;
	}

	int list_fonts() {
		for (auto const& family : fonts::Families())
			std::cout << family << '\n';
		return 0;
	}
}

int main(int argc, char **argv) {
	std::vector<std::string> args(argv + 1, argv + argc);
	agi::fs::path data_dir = P_DATA;
	while (args.size() >= 2 && (args[0] == "--data" || args[0] == "--fonts")) {
		if (args[0] == "--data")
			data_dir = args[1];
		else
			fonts::AddDirectory(agi::fs::path(std::filesystem::absolute(agi::fs::path(args[1]))));
		args.erase(args.begin(), args.begin() + 2);
	}
	fonts::AddDirectory("/usr/share/fonts");
	fonts::AddDirectory("/usr/local/share/fonts");
	if (auto home = getenv("HOME")) {
		fonts::AddDirectory(agi::fs::path(home) / ".local/share/fonts");
		fonts::AddDirectory(agi::fs::path(home) / ".fonts");
	}

	auto expect = [&](size_t count) {
		if (args.size() != count + 1) {
			std::cerr << usage;
			std::exit(2);
		}
	};
	if (args.empty()) {
		std::cerr << usage;
		return 2;
	}

	agi::util::InitLocale();
	task_queue::InitDispatch();
	// The core's debug log isn't shown anywhere
	agi::log::log = new agi::log::LogSink;

	// Keep everything the core writes (options, recent files, temporary
	// files) out of the desktop program's folders
	auto user_dir = agi::fs::path(std::filesystem::temp_directory_path()) / "typemotion-headless";
	agi::fs::CreateDirectory(user_dir);

	config::path = new agi::Path;
	config::path->SetToken("?data", agi::fs::path(std::filesystem::absolute(data_dir)));
	config::path->SetToken("?user", user_dir);
	config::path->SetToken("?local", user_dir);
	config::opt = new agi::Options("", GET_DEFAULT_CONFIG(default_config), agi::Options::FLUSH_SKIP);
	config::mru = new agi::MRUManager(user_dir / "mru.json", GET_DEFAULT_CONFIG(default_mru), config::opt);

	cmd::init_builtin_commands();
	Automation4::ScriptFactory::Register(std::make_unique<Automation4::LuaScriptFactory>());

	int ret = 2;
	{
		auto c = std::make_unique<agi::Context>();
		std::string const& command = args[0];
		if (command == "info") {
			expect(1);
			ret = run_on_main([&] { return info(c.get(), args[1]); });
		}
		else if (command == "convert") {
			expect(2);
			ret = run_on_main([&] { return convert(c.get(), args[1], args[2]); });
		}
		else if (command == "macros") {
			expect(1);
			ret = run_on_main([&] { return macros(args[1]); });
		}
		else if (command == "run") {
			expect(4);
			ret = run_on_main([&] { return run(c.get(), args[1], args[2], args[3], args[4]); });
		}
		else if (command == "command") {
			expect(3);
			ret = run_on_main([&] { return run_command(c.get(), args[1], args[2], args[3]); });
		}
		else if (command == "extents") {
			expect(3);
			ret = run_on_main([&] { return extents(c.get(), args[1], args[2], args[3]); });
		}
		else if (command == "fonts") {
			expect(0);
			ret = run_on_main([&] { return list_fonts(); });
		}
		else
			std::cerr << usage;
	}

	cmd::clear();
	delete config::mru;
	delete config::opt;
	delete config::path;
	delete agi::log::log;
	return ret;
}
