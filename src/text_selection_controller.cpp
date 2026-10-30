// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
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
//
// Aegisub Project http://www.aegisub.org/

#include "text_selection_controller.h"

void TextSelectionController::SetControl(std::unique_ptr<Control> ctrl) {
	this->ctrl = std::move(ctrl);
}

TextSelectionController::~TextSelectionController() = default;

#define GET(var, new_value) do { \
	int tmp = new_value;      \
	if (tmp != var) {         \
		var = tmp;            \
		changed = true;       \
	}                         \
} while(false)

#define SET(var, new_value, Setter) do { \
	if (var != new_value) {              \
		var = new_value;                 \
		if (ctrl) ctrl->Setter(var);     \
	}                                    \
} while (false)

void TextSelectionController::ControlUpdated(int ctrl_insertion_point, bool selection_changed, int ctrl_selection_start, int ctrl_selection_end) {
	if (changing) return;

	bool changed = false;
	GET(insertion_point, ctrl_insertion_point);
	if (selection_changed) {
		GET(selection_start, ctrl_selection_start);
		GET(selection_end, ctrl_selection_end);
	}
	else {
		GET(selection_start, insertion_point);
		GET(selection_end, insertion_point);
	}
	if (changed) AnnounceSelectionChanged();
}

void TextSelectionController::SetInsertionPoint(int position) {
	changing = true;
	SET(insertion_point, position, SetInsertionPoint);
	changing = false;
	AnnounceSelectionChanged();
}

void TextSelectionController::SetSelection(int start, int end) {
	changing = true;
	SET(selection_start, start, SetSelectionStart);
	SET(selection_end, end, SetSelectionEnd);
	changing = false;
	AnnounceSelectionChanged();
}


void TextSelectionController::CommitStagedChanges() {
	if (has_staged_selection) {
		if (staged_selection_start <= staged_selection_end) {
			SetSelection(staged_selection_start, staged_selection_end);
		} else {
			// commit some crimes to get this to work in all cases
			SetInsertionPoint(staged_selection_end == 0 ? staged_selection_start : 0);
			SetSelection(staged_selection_start, staged_selection_start);
			SetInsertionPoint(staged_selection_end);
		}
		has_staged_selection = false;
	}
}