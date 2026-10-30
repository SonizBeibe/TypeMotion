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

/// @file font_index.cpp
/// @brief Finding font files by name, for frontends without a system font API

#include "font_index.h"

#include <libaegisub/log.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <set>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H

namespace {
struct IndexedFace {
	fonts::Face face;
	/// Family name to show, in English if the font has it
	std::string display_family;
	/// Lowercased family names, in every language the font has
	std::vector<std::string> families;
	/// Lowercased full and PostScript names
	std::vector<std::string> full_names;
};

std::mutex index_mutex;
std::vector<agi::fs::path> directories;
size_t scanned_directories = 0;
std::vector<IndexedFace> faces;

std::string lowercase(std::string str) {
	for (auto& c : str) {
		if (c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
	}
	return str;
}

void append_utf8(std::string& out, uint32_t cp) {
	if (cp < 0x80)
		out += static_cast<char>(cp);
	else if (cp < 0x800) {
		out += static_cast<char>(0xC0 | (cp >> 6));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
	else if (cp < 0x10000) {
		out += static_cast<char>(0xE0 | (cp >> 12));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
	else {
		out += static_cast<char>(0xF0 | (cp >> 18));
		out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (cp & 0x3F));
	}
}

/// Decode a name from the Windows platform, which is UTF-16BE
std::string decode_utf16be(FT_Byte const *data, FT_UInt len) {
	std::string out;
	for (FT_UInt i = 0; i + 1 < len; i += 2) {
		uint32_t cp = (data[i] << 8) | data[i + 1];
		if (cp >= 0xD800 && cp < 0xDC00 && i + 3 < len) {
			uint32_t low = (data[i + 2] << 8) | data[i + 3];
			if (low >= 0xDC00 && low < 0xE000) {
				cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
				i += 2;
			}
		}
		append_utf8(out, cp);
	}
	return out;
}

void add_unique(std::vector<std::string>& names, std::string name) {
	if (name.empty()) return;
	name = lowercase(std::move(name));
	if (std::find(names.begin(), names.end(), name) == names.end())
		names.push_back(std::move(name));
}

void index_face(FT_Face ft_face, agi::fs::path const& path, int index) {
	IndexedFace face;
	face.face.path = path;
	face.face.index = index;

	// Windows only reads the names for its own platform
	for (FT_UInt i = 0, count = FT_Get_Sfnt_Name_Count(ft_face); i < count; ++i) {
		FT_SfntName name;
		if (FT_Get_Sfnt_Name(ft_face, i, &name) || name.platform_id != TT_PLATFORM_MICROSOFT)
			continue;
		if (name.encoding_id != TT_MS_ID_UNICODE_CS && name.encoding_id != TT_MS_ID_SYMBOL_CS && name.encoding_id != TT_MS_ID_UCS_4)
			continue;
		switch (name.name_id) {
			case TT_NAME_ID_FONT_FAMILY: {
				auto family = decode_utf16be(name.string, name.string_len);
				if (face.display_family.empty() || name.language_id == TT_MS_LANGID_ENGLISH_UNITED_STATES)
					face.display_family = family;
				add_unique(face.families, std::move(family));
				break;
			}
			case TT_NAME_ID_FULL_NAME:
			case TT_NAME_ID_PS_NAME:
				add_unique(face.full_names, decode_utf16be(name.string, name.string_len));
				break;
		}
	}

	// Fonts that aren't TrueType or OpenType have no name table
	if (face.families.empty() && ft_face->family_name) {
		face.display_family = ft_face->family_name;
		add_unique(face.families, ft_face->family_name);
	}
	if (face.families.empty())
		return;
	if (auto ps_name = FT_Get_Postscript_Name(ft_face))
		add_unique(face.full_names, ps_name);

	auto os2 = static_cast<TT_OS2 *>(FT_Get_Sfnt_Table(ft_face, FT_SFNT_OS2));
	if (os2 && os2->version != 0xFFFF && os2->usWeightClass >= 1 && os2->usWeightClass <= 1000) {
		face.face.weight = os2->usWeightClass;
		// Some old fonts use 1 to 9 instead of 100 to 900
		if (face.face.weight < 10)
			face.face.weight *= 100;
		face.face.italic = (os2->fsSelection & 0x201) != 0; // italic or oblique
	}
	else {
		face.face.weight = ft_face->style_flags & FT_STYLE_FLAG_BOLD ? 700 : 400;
		face.face.italic = ft_face->style_flags & FT_STYLE_FLAG_ITALIC;
	}

	faces.push_back(std::move(face));
}

bool is_font_file(agi::fs::path const& path) {
	auto ext = lowercase(path.extension().string());
	return ext == ".ttf" || ext == ".otf" || ext == ".ttc" || ext == ".otc";
}

void scan_directory(FT_Library library, agi::fs::path const& dir) {
	std::error_code ec;
	auto options = std::filesystem::directory_options::skip_permission_denied
	             | std::filesystem::directory_options::follow_directory_symlink;
	std::filesystem::recursive_directory_iterator it(dir, options, ec), end;
	for (; !ec && it != end; it.increment(ec)) {
		agi::fs::path path(it->path());
		if (!it->is_regular_file(ec) || !is_font_file(path))
			continue;

		auto path_str = path.string();
		FT_Long num_faces = 1;
		for (FT_Long i = 0; i < num_faces; ++i) {
			FT_Face ft_face;
			if (FT_New_Face(library, path_str.c_str(), i, &ft_face))
				break;
			num_faces = ft_face->num_faces;
			index_face(ft_face, path, static_cast<int>(i));
			FT_Done_Face(ft_face);
		}
	}
}

/// Scan the folders added since the last scan. Must be called with the lock held.
void update_index() {
	if (scanned_directories == directories.size())
		return;

	FT_Library library;
	if (FT_Init_FreeType(&library)) {
		LOG_E("font_index") << "Could not initialize FreeType";
		return;
	}
	for (; scanned_directories < directories.size(); ++scanned_directories)
		scan_directory(library, directories[scanned_directories]);
	FT_Done_FreeType(library);

	LOG_D("font_index") << "Found " << faces.size() << " font faces";
}

/// Pick the face of a family closest to the style asked for, the way the
/// Windows font mapper does: matching italic matters most, then the weight
IndexedFace const *best_face(std::vector<IndexedFace const *> const& candidates, bool bold, bool italic) {
	int target_weight = bold ? 700 : 400;
	IndexedFace const *best = nullptr;
	int best_score = 0;
	for (auto face : candidates) {
		int score = std::abs(face->face.weight - target_weight);
		// A heavier face is a better substitute for bold than a lighter one
		if (face->face.weight < target_weight)
			score += 1;
		if (face->face.italic != italic)
			score += 10000;
		if (!best || score < best_score) {
			best = face;
			best_score = score;
		}
	}
	return best;
}

std::vector<IndexedFace const *> find_by_name(std::string const& name) {
	std::vector<IndexedFace const *> found;
	for (auto const& face : faces) {
		if (std::find(face.families.begin(), face.families.end(), name) != face.families.end())
			found.push_back(&face);
	}
	if (!found.empty())
		return found;

	for (auto const& face : faces) {
		if (std::find(face.full_names.begin(), face.full_names.end(), name) != face.full_names.end())
			found.push_back(&face);
	}
	return found;
}
}

namespace fonts {
void AddDirectory(agi::fs::path const& dir) {
	std::lock_guard<std::mutex> lock(index_mutex);
	if (std::find(directories.begin(), directories.end(), dir) == directories.end())
		directories.push_back(dir);
}

std::vector<agi::fs::path> Directories() {
	std::lock_guard<std::mutex> lock(index_mutex);
	return directories;
}

std::optional<Match> Find(std::string const& requested_name, bool bold, bool italic) {
	std::lock_guard<std::mutex> lock(index_mutex);
	update_index();
	if (faces.empty())
		return std::nullopt;

	auto name = lowercase(requested_name);
	// Fonts for vertical text use the same faces
	if (!name.empty() && name[0] == '@')
		name.erase(0, 1);

	auto candidates = find_by_name(name);
	for (auto fallback : {"arial", "liberation sans", "roboto", "dejavu sans", "noto sans"}) {
		if (!candidates.empty()) break;
		candidates = find_by_name(fallback);
	}
	if (candidates.empty())
		candidates.push_back(&faces.front());

	auto face = best_face(candidates, bold, italic);
	Match match;
	match.face = face->face;
	match.fake_bold = bold && face->face.weight < 600;
	match.fake_italic = italic && !face->face.italic;
	return match;
}

std::vector<std::string> Families() {
	std::lock_guard<std::mutex> lock(index_mutex);
	update_index();
	std::set<std::string> families;
	for (auto const& face : faces)
		families.insert(face.display_family);
	return {families.begin(), families.end()};
}
}
