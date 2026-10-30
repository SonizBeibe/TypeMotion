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

/// @file command_core.cpp
/// @brief The commands which builds of the core without wxWidgets have
/// @ingroup command
///
/// command_wx.cpp is the desktop program's version. The commands for the
/// desktop window, audio, video, help and visual tools are not here because
/// they work with its widgets.

#include "command.h"

#include <libaegisub/log.h>

namespace cmd {
std::string Command::GetTooltip(std::string) const {
	// There are no hotkeys to show
	return StrHelp();
}

void init_automation();
void init_edit();
void init_grid();
void init_keyframe();
void init_recent();
void init_subtitle();
void init_time();
void init_timecode();
void init_tool();

void init_builtin_commands() {
	LOG_D("command/init") << "Populating command map";
	init_automation();
	init_edit();
	init_grid();
	init_keyframe();
	init_recent();
	init_subtitle();
	init_time();
	init_timecode();
	init_tool();
}
}
