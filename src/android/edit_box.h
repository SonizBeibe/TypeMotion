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

/// @file edit_box.h
/// @brief The engine side of the app's edit box
/// @ingroup android
///
/// Does what the desktop SubsEditBox does: it shows the active line, and
/// changes all the selected lines when a field is edited, grouping the
/// changes into undo steps the same way. The app's text field reports its
/// cursor here, and follows the "text_selection" events when commands and
/// scripts move it. Positions in the text are in bytes of UTF-8.
///
/// It sends "edit_box" events with State() when the line changes other than
/// through Set(), with "text_changed" true when the app has to replace the
/// text it shows.

#pragma once

#include <libaegisub/cajun/elements.h>

namespace agi { struct Context; }

namespace android::edit_box {
	using Args = json::Object const&;

	/// Start following the active line, when the context is created
	void Connect(agi::Context *c);
	/// Stop, before the context is destroyed
	void Disconnect();

	/// What the edit box shows
	json::Object State(agi::Context *c, Args);
	/// Change a field of the selected lines, as editing it in the edit box
	/// does. Arguments: field (text, start, end, duration, layer, comment,
	/// style, actor, effect, margin_l, margin_r or margin_v) and value; for
	/// text, new_action when the edit doesn't continue the previous typing;
	/// for actor and effect, typed when it was typed rather than picked
	json::Object Set(agi::Context *c, Args args);
	/// Where the text field's cursor and selection are: start, end, insertion
	json::Object SetTextSelection(agi::Context *c, Args args);
}
