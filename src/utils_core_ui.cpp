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

/// @file utils_core_ui.cpp
/// @brief The functions of utils.h which the desktop program implements with
/// wxWidgets in utils.cpp, for builds of the core, which use ui_hooks.h

#include "utils.h"

#include "options.h"
#include "ui_hooks.h"

#include <optional>

std::string GetClipboard() {
	return ui::GetClipboardText();
}

void SetClipboard(std::string const& new_value) {
	ui::SetClipboardText(new_value);
}

static agi::fs::path FileSelector(bool save, UiString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent) {
	std::string dir;
	if (!option_name.empty())
		dir = OPT_GET(option_name)->GetString();

	// Like wxFileSelector, show files with the default extension when there
	// is no wildcard
	std::string filter = wildcard;
	if (filter.empty() && !default_extension.empty())
		filter = "*." + default_extension;

	std::optional<std::string> chosen;
	if (save)
		chosen = ui::ShowSaveFileDialog(parent, message, dir, default_filename, filter, true);
	else if (auto files = ui::ShowOpenFileDialog(message, dir, default_filename, filter, false, true); files && !files->empty())
		chosen = files->front();
	if (!chosen)
		return {};

	agi::fs::path filename(*chosen);
	if (!filename.empty() && !option_name.empty())
		OPT_SET(option_name)->SetString(filename.parent_path().string());
	return filename;
}

agi::fs::path OpenFileSelector(UiString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent) {
	return FileSelector(false, message, option_name, default_filename, default_extension, wildcard, parent);
}

agi::fs::path SaveFileSelector(UiString const& message, std::string const& option_name, std::string const& default_filename, std::string const& default_extension, std::string const& wildcard, wxWindow *parent) {
	return FileSelector(true, message, option_name, default_filename, default_extension, wildcard, parent);
}
