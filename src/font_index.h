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

/// @file font_index.h
/// @brief Finding font files by name, for frontends without a system font API

#pragma once

#include <libaegisub/fs.h>

#include <optional>
#include <string>
#include <vector>

namespace fonts {
	/// A face inside a font file
	struct Face {
		agi::fs::path path;
		int index = 0;
		/// Weight class, from 100 (thin) to 900 (black)
		int weight = 400;
		bool italic = false;
	};

	/// The face chosen for a font name, and the styles that have to be faked
	/// because the family doesn't have them
	struct Match {
		Face face;
		bool fake_bold = false;
		bool fake_italic = false;
	};

	/// Add a folder to look for fonts in, including its subfolders. Folders
	/// are scanned the first time a font is looked up.
	void AddDirectory(agi::fs::path const& dir);

	/// The folders fonts are looked for in
	std::vector<agi::fs::path> Directories();

	/// Find the face Windows would use for a font name. The name is matched
	/// against the family names first, then the full and PostScript names.
	/// Unknown names fall back to a common sans-serif font.
	/// @return Nothing if no fonts were found at all
	std::optional<Match> Find(std::string const& name, bool bold, bool italic);

	/// The family names of all the fonts found, sorted
	std::vector<std::string> Families();
}
