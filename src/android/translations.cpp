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

/// @file translations.cpp
/// @brief The core's translations, read from the desktop program's .po file
/// @ingroup android
///
/// The desktop program translates with wxWidgets and the compiled catalogs
/// in po/. The app reads the same translations from the .po file, which
/// comes with it, so that the core's text (the commands, the scripts' tr()
/// and the messages) is in the same language as on the desktop.

#include "android.h"

#include "../i18n.h"

#include <libaegisub/exception.h>
#include <libaegisub/io.h>
#include <libaegisub/log.h>

#include <cstdlib>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {
	/// The translations of each message, by its msgid, or by its msgctxt
	/// and msgid joined with \x04 as gettext does
	std::unordered_map<std::string, std::vector<std::string>> catalog;

	/// One entry of the .po file as it is read
	struct Entry {
		std::string context;
		std::string id;
		std::vector<std::string> strs;
		bool has_context = false;
		bool fuzzy = false;
	};

	/// The text of a quoted .po string
	std::string unquote(std::string_view line) {
		auto start = line.find('"');
		auto end = line.rfind('"');
		if (start == line.npos || end <= start) return "";

		std::string text;
		text.reserve(end - start);
		for (size_t i = start + 1; i < end; ++i) {
			char c = line[i];
			if (c != '\\' || i + 1 >= end) {
				text += c;
				continue;
			}
			switch (line[++i]) {
				case 'n': text += '\n'; break;
				case 't': text += '\t'; break;
				case 'r': text += '\r'; break;
				default: text += line[i]; break;
			}
		}
		return text;
	}

	void add(Entry& entry) {
		// The header has an empty msgid, and fuzzy translations aren't used
		// by the desktop program either
		bool translated = false;
		for (auto const& str : entry.strs)
			translated = translated || !str.empty();
		if (!entry.id.empty() && !entry.fuzzy && translated) {
			auto key = entry.has_context ? entry.context + '\x04' + entry.id : entry.id;
			catalog[key] = std::move(entry.strs);
		}
		entry = Entry();
	}

	std::string const *find(std::string const& key, size_t form) {
		auto it = catalog.find(key);
		if (it == catalog.end() || form >= it->second.size() || it->second[form].empty())
			return nullptr;
		return &it->second[form];
	}
}

namespace android {
	void LoadTranslations(agi::fs::path const& file) {
		catalog.clear();
		try {
			auto stream = agi::io::Open(file);
			Entry entry;
			// Where a continued string goes: the msgctxt, the msgid or a msgstr
			std::string *field = nullptr;
			std::string line;
			while (std::getline(*stream, line)) {
				if (!line.empty() && line.back() == '\r') line.pop_back();
				std::string_view view(line);
				while (!view.empty() && (view.front() == ' ' || view.front() == '\t'))
					view.remove_prefix(1);

				bool has_str = !entry.strs.empty();
				if (view.empty()) {
					if (has_str) add(entry);
					field = nullptr;
				}
				else if (view.front() == '#') {
					if (has_str) add(entry);
					field = nullptr;
					// Obsolete entries start with #~ and are skipped as comments
					if (view.substr(0, 2) == "#," && view.find("fuzzy") != view.npos)
						entry.fuzzy = true;
				}
				else if (view.substr(0, 8) == "msgctxt ") {
					if (has_str) add(entry);
					entry.has_context = true;
					entry.context = unquote(view);
					field = &entry.context;
				}
				else if (view.substr(0, 13) == "msgid_plural ") {
					// The plural forms are in the msgstr[n] lines
					field = nullptr;
				}
				else if (view.substr(0, 6) == "msgid ") {
					if (has_str) add(entry);
					entry.id = unquote(view);
					field = &entry.id;
				}
				else if (view.substr(0, 6) == "msgstr") {
					size_t form = 0;
					if (view.size() > 7 && view[6] == '[')
						form = static_cast<size_t>(std::atoi(std::string(view.substr(7)).c_str()));
					if (entry.strs.size() <= form) entry.strs.resize(form + 1);
					entry.strs[form] = unquote(view);
					field = &entry.strs[form];
				}
				else if (view.front() == '"' && field) {
					*field += unquote(view);
				}
			}
			add(entry);
			LOG_I("android/translations") << "Loaded " << catalog.size() << " translations from " << file;
		}
		catch (agi::Exception const& e) {
			LOG_E("android/translations") << "Could not read " << file << ": " << e.GetMessage();
		}
	}
}

namespace agi::i18n {
	std::string Translate(const char *message) {
		auto str = find(message, 0);
		return str ? *str : message;
	}

	std::string TranslateContext(const char *context, const char *message) {
		auto str = find(std::string(context) + '\x04' + message, 0);
		return str ? *str : message;
	}

	std::string TranslatePlural(const char *singular, const char *plural, int n) {
		// The plural rule of Spanish, and of English for the untranslated text
		size_t form = n == 1 ? 0 : 1;
		auto str = find(singular, form);
		if (str) return *str;
		return n == 1 ? singular : plural;
	}
}
