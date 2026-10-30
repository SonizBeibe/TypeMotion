// Copyright (c) 2006, 2007, Niels Martin Hansen
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

/// @file auto4_lua_dialog_wx.cpp
/// @brief wxWidgets controls for Lua configuration dialogs
/// @ingroup scripting
///

#include "auto4_lua_dialog.h"

#include "colour_button.h"
#include "compat.h"
#include "validators.h"

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm.hpp>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/dialog.h>
#include <wx/gbsizer.h>
#include <wx/panel.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/valgen.h>

namespace {
	using namespace Automation4;
	using Kind = LuaDialogControl::Kind;

	/// Create the wxControl for a dialog control
	wxControl *create_control(LuaDialogControl &c, wxWindow *parent) {
		switch (c.GetKind()) {
		case Kind::Label: {
			auto& ctl = static_cast<LuaControl::Label&>(c);
			return new wxStaticText(parent, -1, to_wx(ctl.label));
		}
		case Kind::Edit: {
			auto& ctl = static_cast<LuaControl::Edit&>(c);
			auto cw = new wxTextCtrl(parent, -1, to_wx(ctl.text));
			cw->SetMaxLength(0);
			cw->SetValidator(StringBinder(&ctl.text));
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::Color:
		case Kind::ColorAlpha: {
			auto& ctl = static_cast<LuaControl::Color&>(c);
			wxControl *cw = new ColourButton(parent, wxSize(50*ctl.width,10*ctl.height), ctl.alpha, ctl.color, ColorValidator(&ctl.color));
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::Textbox: {
			auto& ctl = static_cast<LuaControl::Textbox&>(c);
			auto cw = new wxTextCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE, StringBinder(&ctl.text));
			cw->SetMinSize(wxSize(0, 30));
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::IntEdit: {
			auto& ctl = static_cast<LuaControl::IntEdit&>(c);
			auto cw = new wxSpinCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, ctl.min, ctl.max, ctl.value);
			cw->SetValidator(wxGenericValidator(&ctl.value));
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::FloatEdit: {
			auto& ctl = static_cast<LuaControl::FloatEdit&>(c);
			if (ctl.step > 0) {
				auto scd = new wxSpinCtrlDouble(parent, -1, "", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, ctl.min, ctl.max, ctl.value, ctl.step);
				scd->SetValidator(DoubleSpinValidator(&ctl.value));
				scd->SetToolTip(to_wx(ctl.hint));
				return scd;
			}

			DoubleValidator val(&ctl.value, ctl.min, ctl.max);
			auto cw = new wxTextCtrl(parent, -1, "", wxDefaultPosition, wxDefaultSize, 0, val);
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::Dropdown: {
			auto& ctl = static_cast<LuaControl::Dropdown&>(c);
			auto cw = new wxComboBox(parent, -1, to_wx(ctl.value), wxDefaultPosition, wxDefaultSize, to_wx(ctl.items), wxCB_READONLY, StringBinder(&ctl.value));
			cw->SetToolTip(to_wx(ctl.hint));
			return cw;
		}
		case Kind::Checkbox: {
			auto& ctl = static_cast<LuaControl::Checkbox&>(c);
			auto cw = new wxCheckBox(parent, -1, to_wx(ctl.label));
			cw->SetValidator(wxGenericValidator(&ctl.value));
			cw->SetToolTip(to_wx(ctl.hint));
			cw->SetValue(ctl.value);
			return cw;
		}
		}
		return nullptr;
	}

	/// Get the flags to use when inserting a control into a sizer
	int sizer_flags(LuaDialogControl const& c) {
		return c.GetKind() == Kind::Label ? wxALIGN_CENTRE_VERTICAL | wxALIGN_LEFT : wxEXPAND;
	}

	wxWindowID button_id_to_wx(int id) {
		switch (id) {
		case LuaDialog::BUTTON_OK:           return wxID_OK;
		case LuaDialog::BUTTON_YES:          return wxID_YES;
		case LuaDialog::BUTTON_SAVE:         return wxID_SAVE;
		case LuaDialog::BUTTON_APPLY:        return wxID_APPLY;
		case LuaDialog::BUTTON_CLOSE:        return wxID_CLOSE;
		case LuaDialog::BUTTON_NO:           return wxID_NO;
		case LuaDialog::BUTTON_CANCEL:       return wxID_CANCEL;
		case LuaDialog::BUTTON_HELP:         return wxID_HELP;
		case LuaDialog::BUTTON_CONTEXT_HELP: return wxID_CONTEXT_HELP;
		default:                             return -1;
		}
	}
}

namespace Automation4 {
	wxWindow* LuaDialog::CreateWindow(wxWindow *parent) {
		auto window = new wxPanel(parent);

		auto s = new wxGridBagSizer(4, 4);
		for (auto& c : controls)
			s->Add(create_control(*c, window), wxGBPosition(c->y, c->x),
				wxGBSpan(c->height, c->width), sizer_flags(*c));

		if (!use_buttons) {
			window->SetSizerAndFit(s);
			return window;
		}

		auto dialog = static_cast<wxDialog *>(parent);
		auto bs = new wxStdDialogButtonSizer;

		auto make_button = [&](wxWindowID id, int button_pushed, std::string const& text) -> wxButton *{
			auto button = new wxButton(window, id, to_wx(text));
			button->Bind(wxEVT_BUTTON, [=, this](wxCommandEvent &) {
				this->button_pushed = button_pushed;
				dialog->TransferDataFromWindow();
				dialog->EndModal(0);
			});

			if (id == wxID_OK || id == wxID_YES || id == wxID_SAVE) {
				button->SetDefault();
				dialog->SetAffirmativeId(id);
			}

			if (id == wxID_CLOSE || id == wxID_NO)
				dialog->SetEscapeId(id);

			return button;
		};

		if (boost::count(buttons | boost::adaptors::map_keys, BUTTON_CUSTOM) == 0) {
			for (size_t i = 0; i < buttons.size(); ++i)
				bs->AddButton(make_button(button_id_to_wx(buttons[i].first), i, buttons[i].second));
			bs->Realize();
		}
		else {
			for (size_t i = 0; i < buttons.size(); ++i)
				bs->Add(make_button(button_id_to_wx(buttons[i].first), i, buttons[i].second));
		}

		auto ms = new wxBoxSizer(wxVERTICAL);
		ms->Add(s, 0, wxBOTTOM, 5);
		ms->Add(bs);
		window->SetSizerAndFit(ms);

		return window;
	}
}
