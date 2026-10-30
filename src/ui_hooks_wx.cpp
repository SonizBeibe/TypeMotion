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

/// @file ui_hooks_wx.cpp
/// @brief The desktop implementation of ui_hooks.h

#include "ui_hooks.h"

#include "audio_controller.h"
#include "audio_timing.h"
#include "base_grid.h"
#include "command/command.h"
#include "compat.h"
#include "dialog_progress.h"
#include "dialogs.h"
#include "frame_main.h"
#include "include/aegisub/context.h"
#include "utils.h"
#include "video_controller.h"
#include "video_display.h"
#include "video_events.h"

#include <wx/arrstr.h>
#include <wx/choicdlg.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/log.h>
#include <wx/msgdlg.h>
#include <wx/textentry.h>
#include <wx/thread.h>
#include <wx/timer.h>
#include <wx/utils.h>

static_assert(long(ui::CENTRE) == long(wxCENTRE));
static_assert(long(ui::YES) == long(wxYES));
static_assert(long(ui::OK) == long(wxOK));
static_assert(long(ui::NO) == long(wxNO));
static_assert(long(ui::YES_NO) == long(wxYES_NO));
static_assert(long(ui::CANCEL) == long(wxCANCEL));
static_assert(long(ui::ICON_WARNING) == long(wxICON_WARNING));
static_assert(long(ui::ICON_ERROR) == long(wxICON_ERROR));
static_assert(long(ui::ICON_QUESTION) == long(wxICON_QUESTION));
static_assert(long(ui::ICON_INFORMATION) == long(wxICON_INFORMATION));

namespace ui {
int ShowMessage(wxString const& message, wxString const& caption, long style, wxWindow *parent) {
	return wxMessageBox(message, caption, style, parent);
}

void ShowMessage(wxString const& message) {
	wxMessageBox(message);
}

void LogWarning(wxString const& message) {
	wxLogWarning("%s", message);
}

void LogError(wxString const& message) {
	wxLogError("%s", message);
}

bool ConfirmLoadingScripts(wxString const& message, wxString const& caption) {
	wxMessageDialog dlg(
			nullptr,
			message
			, caption
			, wxICON_WARNING | wxOK | wxCANCEL | wxCANCEL_DEFAULT | wxCENTRE
		);
	dlg.SetOKCancelLabels(_("Trust the authors && load scripts"), _("No"));
	return dlg.ShowModal() == wxID_OK;
}

int GetSingleChoiceIndex(wxString const& message, wxString const& caption, std::vector<wxString> const& choices) {
	wxArrayString choice_array;
	for (auto const& choice : choices)
		choice_array.push_back(choice);
	return wxGetSingleChoiceIndex(message, caption, choice_array);
}

std::unique_ptr<agi::BackgroundRunner> CreateProgress(wxWindow *parent, wxString const& title, wxString const& message) {
	return std::make_unique<DialogProgress>(parent, title, message);
}

wxWindow *GetProgressWindow(agi::BackgroundRunner *progress) {
	return static_cast<DialogProgress *>(progress);
}

std::string GetProgressTitle(agi::BackgroundRunner *progress) {
	return from_wx(static_cast<DialogProgress *>(progress)->GetTitle());
}

std::optional<std::vector<std::string>> ShowOpenFileDialog(wxString const& message, wxString const& dir, wxString const& file, wxString const& wildcard, bool multiple, bool must_exist) {
	int flags = wxFD_OPEN;
	if (multiple)
		flags |= wxFD_MULTIPLE;
	if (must_exist)
		flags |= wxFD_FILE_MUST_EXIST;

	wxFileDialog diag(nullptr, message, dir, file, wildcard, flags);
	if (diag.ShowModal() == wxID_CANCEL)
		return std::nullopt;

	std::vector<std::string> paths;
	if (multiple) {
		wxArrayString files;
		diag.GetPaths(files);
		for (auto const& path : files)
			paths.emplace_back(path.utf8_str());
	}
	else
		paths.emplace_back(diag.GetPath().utf8_str());
	return paths;
}

std::optional<std::string> ShowSaveFileDialog(wxWindow *parent, wxString const& message, wxString const& dir, wxString const& file, wxString const& wildcard, bool prompt_overwrite) {
	int flags = wxFD_SAVE;
	if (prompt_overwrite)
		flags |= wxFD_OVERWRITE_PROMPT;

	wxFileDialog diag(parent, message, dir, file, wildcard, flags);
	if (diag.ShowModal() == wxID_CANCEL)
		return std::nullopt;
	return std::string(diag.GetPath().utf8_str());
}

std::string GetClipboardText() {
	return GetClipboard();
}

bool SetClipboardText(std::string const& text) {
	bool succeeded = false;
	wxClipboard &cb = *wxTheClipboard;
	if (cb.Open()) {
		succeeded = cb.SetData(new wxTextDataObject(wxString::FromUTF8(text.c_str())));
		cb.Close();
		cb.Flush();
	}
	return succeeded;
}

std::optional<bool> ClipboardHasText() {
	if (!wxTheClipboard->Open())
		return std::nullopt;
	bool has_text = wxTheClipboard->IsSupported(wxDF_TEXT) || wxTheClipboard->IsSupported(wxDF_UNICODETEXT);
	wxTheClipboard->Close();
	return has_text;
}

wxString GetSingleChoice(wxString const& message, wxString const& caption, std::vector<wxString> const& choices, wxWindow *parent, int width, int height) {
	wxArrayString choice_array;
	for (auto const& choice : choices)
		choice_array.push_back(choice);
	return wxGetSingleChoice(message, caption, choice_array, parent, -1, -1, true, width, height);
}

bool HasFocusedTextEntry(agi::Context *c) {
	return dynamic_cast<wxTextEntryBase*>(c->parent->FindFocus()) != nullptr;
}

void EditFocusedTextEntry(agi::Context *c, TextEntryAction action) {
	if (wxTextEntryBase *ctrl = dynamic_cast<wxTextEntryBase*>(c->parent->FindFocus())) {
		switch (action) {
			case TextEntryAction::Copy: ctrl->Copy(); break;
			case TextEntryAction::Cut: ctrl->Cut(); break;
			case TextEntryAction::Paste: ctrl->Paste(); break;
		}
	}
}

bool IsCmdDown() {
	return wxGetMouseState().CmdDown();
}

bool IsShiftDown() {
	return wxGetMouseState().ShiftDown();
}

void CloseWindow(agi::Context *c) {
	c->frame->Close();
}

bool ShowPlainTextImportDialog() {
	return ::ShowPlainTextImportDialog();
}

bool IsBusy() {
	return wxIsBusy();
}

void BeginBusyCursor() {
	wxBeginBusyCursor();
}

void EndBusyCursor() {
	wxEndBusyCursor();
}

bool IsMainThread() {
	return wxThread::IsMain();
}

bool HasStatusBar(const agi::Context *c) {
	return c->frame != nullptr;
}

void StatusTimeout(agi::Context *c, wxString const& text) {
	c->frame->StatusTimeout(text);
}

agi::BackgroundRunner *CreateWindowProgress(wxWindow *parent) {
	return new DialogProgress(parent);
}

std::unique_ptr<AsyncVideoProviderListener> CreateVideoListener(agi::Context *c) {
	return MakeVideoEventSender(c->videoController.get());
}

void UpdateVideoProperties(AssFile *file, const AsyncVideoProvider *new_provider, wxWindow *parent) {
	::UpdateVideoProperties(file, new_provider, parent);
}

void SetVideoZoom(agi::Context *c, double zoom) {
	c->videoDisplay->SetWindowZoom(zoom);
}

void ScrollGridTo(agi::Context *c, int row) {
	c->subsGrid->ScrollTo(row);
}

void RunCommand(const char *name, agi::Context *c) {
	cmd::call(name, c);
}

namespace {
class WxTimer final : public Timer {
	wxTimer timer;

public:
	WxTimer(std::function<void()> on_timer) {
		timer.Bind(wxEVT_TIMER, [on_timer = std::move(on_timer)](wxTimerEvent&) { on_timer(); });
	}

	void Start(int interval) override { timer.Start(interval); }
	void Stop() override { timer.Stop(); }
};
}

std::unique_ptr<Timer> CreateTimer(std::function<void()> on_timer) {
	return std::make_unique<WxTimer>(std::move(on_timer));
}
}
