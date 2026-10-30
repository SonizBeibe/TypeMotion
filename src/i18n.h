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

// Translated, user-visible text for the files of the subtitle core.
//
// In the desktop program this is the same as including compat.h, format.h
// and wx/intl.h. Without wxWidgets (TYPEMOTION_NO_WX), _(), fmt_tl and
// fmt_plural produce UTF-8 std::strings translated by agi::i18n, which the
// program using the core implements.

#include "ui_string.h"

#ifdef TYPEMOTION_NO_WX
#include <libaegisub/format.h>

#include <string>
#include <string_view>
#include <vector>

namespace agi::i18n {
	/// Translate a message
	std::string Translate(const char *message);
	/// Translate a message which is translated differently in some contexts
	std::string TranslateContext(const char *context, const char *message);
	/// Translate a message which has a different form for plurals
	std::string TranslatePlural(const char *singular, const char *plural, int n);
}

namespace agi {
	/// Format with a translated format string, as format.h allows for wxString
	template<typename... Args>
	std::string format(std::string const& fmt, Args&&... args) {
		return format(fmt.c_str(), std::forward<Args>(args)...);
	}
}

#define _(str) agi::i18n::Translate(str)
// Named like wxWidgets' macro, so that the same code works in both builds
#define wxGETTEXT_IN_CONTEXT(context, str) agi::i18n::TranslateContext(context, str)
#define fmt_tl(str, ...) agi::format(agi::i18n::Translate(str).c_str(), __VA_ARGS__)
#define fmt_plural(n, sing, plural, ...) \
	agi::format(agi::i18n::TranslatePlural(sing, plural, (n)).c_str(), __VA_ARGS__)

inline std::string to_ui(std::string const& str) { return str; }
inline std::string to_ui(std::string_view str) { return std::string(str); }
inline std::string to_ui(const char *str) { return str; }
inline std::string from_ui(std::string const& str) { return str; }
inline std::vector<std::string> to_ui(std::vector<std::string> const& strs) { return strs; }
/// Convert a UTF-8 string which may be null
inline std::string ui_from_utf8(const char *str) { return str ? str : ""; }
/// Translate a message which is not a string literal
inline std::string translate_ui(std::string const& str) { return agi::i18n::Translate(str.c_str()); }

/// Format with an already translated format string
template<typename... Args>
std::string ui_format(std::string const& fmt, Args&&... args) {
	return agi::format(fmt.c_str(), std::forward<Args>(args)...);
}
#else
#include "compat.h"
#include "format.h"

#include <wx/intl.h>

#include <vector>

inline wxString to_ui(std::string const& str) { return to_wx(str); }
inline wxString to_ui(std::string_view str) { return to_wx(str); }
inline wxString to_ui(const char *str) { return to_wx(str); }
inline std::string from_ui(wxString const& str) { return from_wx(str); }
inline std::vector<wxString> to_ui(std::vector<std::string> const& strs) {
	std::vector<wxString> ret;
	ret.reserve(strs.size());
	for (auto const& str : strs)
		ret.push_back(to_wx(str));
	return ret;
}
/// Convert a UTF-8 string which may be null
inline wxString ui_from_utf8(const char *str) { return wxString::FromUTF8(str); }
/// Translate a message which is not a string literal
inline wxString translate_ui(wxString const& str) { return wxGetTranslation(str); }

/// Format with an already translated format string
template<typename... Args>
wxString ui_format(wxString const& fmt, Args&&... args) {
	return agi::wxformat(fmt, std::forward<Args>(args)...);
}
#endif
