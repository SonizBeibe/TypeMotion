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

#pragma once

// What the subtitle core needs from the user interface of the program using
// it. The desktop program implements these with wxWidgets in ui_hooks_wx.cpp;
// other front ends (such as the Android app) provide their own implementation.

#include "aspect_ratio.h"
#include "ui_string.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class AssFile;
class AsyncVideoProvider;
class AsyncVideoProviderListener;
class wxWindow;
namespace agi {
	class BackgroundRunner;
	struct Context;
}

namespace ui {
	/// Message box styles and answers. The values are the same as wxWidgets'.
	enum : long {
		CENTRE = 0x00000001,
		YES = 0x00000002,
		OK = 0x00000004,
		NO = 0x00000008,
		YES_NO = YES | NO,
		CANCEL = 0x00000010,
		ICON_WARNING = 0x00000100,
		ICON_ERROR = 0x00000200,
		ICON_QUESTION = 0x00000400,
		ICON_INFORMATION = 0x00000800,
	};

	/// Show a message box and wait for it to be closed
	/// @param style Buttons and icon to show
	/// @param parent Window to show the message box over, if any
	/// @return The button which was pressed (YES, NO, OK or CANCEL)
	int ShowMessage(UiString const& message, UiString const& caption, long style, wxWindow *parent = nullptr);

	/// Show a message box with only an OK button and the default caption
	void ShowMessage(UiString const& message);

	/// Log a warning, which the user is shown later
	void LogWarning(UiString const& message);
	/// Log an error, which the user is shown later
	void LogError(UiString const& message);

	/// Ask whether to load the automation scripts which a subtitle file asks for
	/// @return true if they should be loaded
	bool ConfirmLoadingScripts(UiString const& message, UiString const& caption);

	/// Ask the user to pick one of several choices
	/// @return The index of the chosen item, or -1 if cancelled
	int GetSingleChoiceIndex(UiString const& message, UiString const& caption, std::vector<UiString> const& choices);

	/// Create a progress dialog for running a task in the background
	std::unique_ptr<agi::BackgroundRunner> CreateProgress(wxWindow *parent, UiString const& title, UiString const& message);

	/// Get the window of a progress dialog created with CreateProgress
	wxWindow *GetProgressWindow(agi::BackgroundRunner *progress);
	/// Get the current title of a progress dialog created with CreateProgress
	std::string GetProgressTitle(agi::BackgroundRunner *progress);

	/// Ask for one or more files to open
	/// @return The chosen files as UTF-8 paths, or nothing if cancelled
	std::optional<std::vector<std::string>> ShowOpenFileDialog(UiString const& message, UiString const& dir, UiString const& file, UiString const& wildcard, bool multiple, bool must_exist);
	/// Ask for a file to save to
	/// @return The chosen file as a UTF-8 path, or nothing if cancelled
	std::optional<std::string> ShowSaveFileDialog(wxWindow *parent, UiString const& message, UiString const& dir, UiString const& file, UiString const& wildcard, bool prompt_overwrite);

	/// Get the text on the clipboard, or an empty string if there is none
	std::string GetClipboardText();
	/// Put text on the clipboard
	/// @return Whether it succeeded
	bool SetClipboardText(std::string const& text);
	/// Is there text on the clipboard?
	/// @return Nothing if the clipboard couldn't be opened
	std::optional<bool> ClipboardHasText();

	/// Ask the user to pick one of several choices, in a list of the given size
	/// @return The chosen item, or an empty string if cancelled
	UiString GetSingleChoice(UiString const& message, UiString const& caption, std::vector<UiString> const& choices, wxWindow *parent, int width, int height);

	/// Does a text box in the context's window have the keyboard focus?
	bool HasFocusedTextEntry(agi::Context *c);
	enum class TextEntryAction { Copy, Cut, Paste };
	/// Copy, cut or paste in the text box which has the keyboard focus
	void EditFocusedTextEntry(agi::Context *c, TextEntryAction action);

	/// Is Ctrl (Cmd on macOS) being held down?
	bool IsCmdDown();
	/// Is Shift being held down?
	bool IsShiftDown();

	/// Close the context's window
	void CloseWindow(agi::Context *c);

#ifdef TYPEMOTION_NO_WX
	/// A font, as the font picker of GetFontFromUser shows it
	struct FontSelection {
		std::string face;
		int size = 0;
		bool bold = false;
		bool italic = false;
		bool underline = false;

		bool operator==(FontSelection const&) const = default;
	};

	/// Ask the user to pick a font, starting with the given one
	/// @return The font, or nothing if cancelled
	std::optional<FontSelection> GetFontFromUser(wxWindow *parent, FontSelection const& initial);
#endif

	/// Ask for the settings for importing a plain text file
	/// @return false if cancelled
	bool ShowPlainTextImportDialog();

	/// Is a busy cursor currently being shown?
	bool IsBusy();
	void BeginBusyCursor();
	void EndBusyCursor();

	/// Is this the thread which the user interface runs on?
	bool IsMainThread();

	/// Does the context's window have a status bar to show messages in?
	bool HasStatusBar(const agi::Context *c);
	/// Show a temporary message in the status bar of the context's window
	void StatusTimeout(agi::Context *c, UiString const& text);

	/// Create a progress dialog which belongs to the given window
	agi::BackgroundRunner *CreateWindowProgress(wxWindow *parent);

	/// Create the listener for the frames and errors of the context's video
	std::unique_ptr<AsyncVideoProviderListener> CreateVideoListener(agi::Context *c);

	/// Update the video properties of the file for a newly opened video,
	/// possibly asking the user what to do
	void UpdateVideoProperties(AssFile *file, const AsyncVideoProvider *new_provider, wxWindow *parent);

	/// Set the zoom level of the video display
	void SetVideoZoom(agi::Context *c, double zoom);

	/// Scroll the subtitles grid to the given row
	void ScrollGridTo(agi::Context *c, int row);

	/// Run the named user interface command (such as "subtitle/save")
	void RunCommand(const char *name, agi::Context *c);

	/// A timer which calls a function on the main thread
	class Timer {
	public:
		virtual ~Timer() = default;
		/// Call the function every interval milliseconds, restarting the timer
		/// if it is already running
		virtual void Start(int interval) = 0;
		virtual void Stop() = 0;
	};

	std::unique_ptr<Timer> CreateTimer(std::function<void()> on_timer);
}
