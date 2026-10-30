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

/// @file dialog_methods.h
/// @brief API methods for the app's screens which do what desktop dialogs do
/// @ingroup android
///
/// Each pair of methods gives the app what the desktop dialog shows when it
/// opens, and does what the dialog does with the values the user entered,
/// asking the same questions through the ui hooks.

#pragma once

#include <libaegisub/cajun/elements.h>

class AssStyle;
namespace agi { struct Context; }

namespace android::dialogs {
	using Args = json::Object const&;

	// Shift Times
	json::Object ShiftTimesState(agi::Context *c, Args);
	/// Shift, or with "apply" false only keep the values for next time, as
	/// closing the desktop dialog does
	void ShiftTimes(agi::Context *c, Args args);
	void ClearShiftHistory(agi::Context *c, Args);

	// Select Lines
	json::Object SelectLinesState(agi::Context *c, Args);
	json::Object SelectLines(agi::Context *c, Args args);

	// Find and Replace
	json::Object SearchState(agi::Context *c, Args);
	json::Object Search(agi::Context *c, Args args);

	// Script Properties
	json::Object Properties(agi::Context *c, Args);
	json::Object SetProperties(agi::Context *c, Args args);

	// Styles Manager and Style Editor. The styles are the script's, or a
	// catalog's when "catalog" is given.
	json::Object Catalogs(agi::Context *c, Args);
	json::Object SetCatalog(agi::Context *c, Args args);
	json::Object NewCatalog(agi::Context *c, Args args);
	json::Object DeleteCatalog(agi::Context *c, Args args);
	json::Object CatalogStyles(agi::Context *c, Args args);
	json::Object CopyStyleName(agi::Context *c, Args args);
	json::Object SaveStyle(agi::Context *c, Args args);
	json::Object DeleteStyles(agi::Context *c, Args args);
	json::Object MoveStyles(agi::Context *c, Args args);
	json::Object CopyStylesToCatalog(agi::Context *c, Args args);
	json::Object CopyStylesToScript(agi::Context *c, Args args);
	json::Object StyleEncodings(agi::Context *c, Args);

	/// A style, as styles() and the style methods describe it
	json::Object StyleToJson(AssStyle const& style);
}
