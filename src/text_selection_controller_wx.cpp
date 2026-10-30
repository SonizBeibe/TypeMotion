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

#include <wx/stc/stc.h>

namespace {
/// Tracks the selection of a wxStyledTextCtrl
class StcControl final : public TextSelectionController::Control {
	TextSelectionController *controller;
	wxStyledTextCtrl *ctrl;

	void UpdateUI(wxStyledTextEvent &evt) {
		bool selection_changed = evt.GetUpdated() & wxSTC_UPDATE_SELECTION;
		controller->ControlUpdated(ctrl->GetInsertionPoint(), selection_changed,
			selection_changed ? ctrl->GetSelectionStart() : 0,
			selection_changed ? ctrl->GetSelectionEnd() : 0);
	}

public:
	StcControl(TextSelectionController *controller, wxStyledTextCtrl *ctrl)
	: controller(controller), ctrl(ctrl)
	{
		ctrl->Bind(wxEVT_STC_UPDATEUI, &StcControl::UpdateUI, this);
	}

	~StcControl() {
		ctrl->Unbind(wxEVT_STC_UPDATEUI, &StcControl::UpdateUI, this);
	}

	void SetInsertionPoint(int point) override { ctrl->SetInsertionPoint(point); }
	void SetSelectionStart(int start) override { ctrl->SetSelectionStart(start); }
	void SetSelectionEnd(int end) override { ctrl->SetSelectionEnd(end); }
};
}

void TextSelectionController::SetControl(wxStyledTextCtrl *ctrl) {
	if (ctrl)
		SetControl(std::make_unique<StcControl>(this, ctrl));
	else
		SetControl(std::unique_ptr<Control>());
}
