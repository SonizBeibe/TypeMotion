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

/// @file text_extents.cpp
/// @brief Measuring text for automation scripts without Windows
///
/// The desktop version measures text with GDI on Windows, and karaoke
/// templates are written against those numbers. This reads the fonts with
/// FreeType and computes the metrics the same way GDI does, so that scripts
/// place text where they would on Windows.

#include "auto4_base.h"

#include "ass_style.h"
#include "font_index.h"

#include <libaegisub/log.h>

#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H
#include FT_TRUETYPE_TABLES_H

namespace {
/// The parts of a font GDI uses to measure text, in font units
struct GdiFont {
	FT_Face face = nullptr;
	long units_per_em = 0;
	long win_ascent = 0;
	long win_descent = 0;
	long hhea_ascent = 0;
	long hhea_descent = 0;
	long line_gap = 0;
	/// Symbol fonts put their characters at U+F020 to U+F0FF
	bool symbol = false;
};

std::mutex extents_mutex;
FT_Library library = nullptr;
std::map<std::pair<std::string, int>, GdiFont> open_fonts;

/// Windows' MulDiv, which rounds to the nearest integer
long mul_div(long a, long b, long c) {
	if (c == 0) return -1;
	long long num = static_cast<long long>(a) * b;
	bool negative = (num < 0) != (c < 0);
	long long abs_num = num < 0 ? -num : num;
	long long abs_c = c < 0 ? -static_cast<long long>(c) : c;
	long long result = (abs_num + abs_c / 2) / abs_c;
	return static_cast<long>(negative ? -result : result);
}

GdiFont *open_font(fonts::Face const& face) {
	auto key = std::make_pair(face.path.string(), face.index);
	auto it = open_fonts.find(key);
	if (it != open_fonts.end())
		return &it->second;

	if (!library && FT_Init_FreeType(&library)) {
		library = nullptr;
		return nullptr;
	}

	// Scripts use few fonts, so this only matters when something goes wrong
	if (open_fonts.size() >= 64) {
		for (auto& font : open_fonts)
			FT_Done_Face(font.second.face);
		open_fonts.clear();
	}

	GdiFont font;
	if (FT_New_Face(library, key.first.c_str(), face.index, &font.face)) {
		LOG_E("text_extents") << "Could not open font " << key.first;
		return nullptr;
	}

	if (FT_Select_Charmap(font.face, FT_ENCODING_UNICODE) && !FT_Select_Charmap(font.face, FT_ENCODING_MS_SYMBOL))
		font.symbol = true;

	font.units_per_em = font.face->units_per_EM ? font.face->units_per_EM : 2048;
	if (auto hhea = static_cast<TT_HoriHeader *>(FT_Get_Sfnt_Table(font.face, FT_SFNT_HHEA))) {
		font.hhea_ascent = hhea->Ascender;
		font.hhea_descent = hhea->Descender;
		font.line_gap = hhea->Line_Gap;
	}
	else {
		font.hhea_ascent = font.face->ascender;
		font.hhea_descent = font.face->descender;
		font.line_gap = font.face->height - font.face->ascender + font.face->descender;
	}

	auto os2 = static_cast<TT_OS2 *>(FT_Get_Sfnt_Table(font.face, FT_SFNT_OS2));
	if (os2 && os2->version != 0xFFFF && os2->usWinAscent + os2->usWinDescent != 0) {
		font.win_ascent = os2->usWinAscent;
		// Some fonts have a negative descent here, which Windows accepts
		font.win_descent = std::abs(static_cast<FT_Short>(os2->usWinDescent));
	}
	else {
		font.win_ascent = font.hhea_ascent;
		font.win_descent = -font.hhea_descent;
	}

	return &(open_fonts[key] = font);
}

/// The size of the em square GDI picks for a font height, as in
/// LOGFONT::lfHeight. A positive height is the height of the whole cell.
long ppem_for_height(GdiFont const& font, long height) {
	const long max_ppem = (1 << 16) - 1;
	if (height == 0)
		height = 16;
	if (height < 0)
		return height >= -max_ppem ? -height : 1;

	long units = font.win_ascent + font.win_descent;
	if (units == 0)
		units = font.hhea_ascent - font.hhea_descent;
	long ppem = mul_div(font.units_per_em, height, units);
	// Rounding up can make the cell taller than asked for
	if (ppem > 1 && mul_div(units, ppem, font.units_per_em) > height)
		--ppem;
	return ppem > max_ppem ? 1 : ppem;
}

/// Advance width of a character in pixels, the way GetTextExtentPoint32 measures it
long char_width(GdiFont const& font, long ppem, uint32_t ch, bool fake_bold) {
	FT_UInt glyph = FT_Get_Char_Index(font.face, ch);
	if (!glyph && font.symbol && ch < 0x100)
		glyph = FT_Get_Char_Index(font.face, 0xF000 + ch);

	FT_Fixed advance = 0;
	if (FT_Get_Advance(font.face, glyph, FT_LOAD_NO_SCALE, &advance))
		return 0;
	long width = mul_div(static_cast<long>(advance), ppem, font.units_per_em);
	// GDI makes faked bold one pixel wider
	return fake_bold ? width + 1 : width;
}

/// Decode the next character of a UTF-8 string
uint32_t next_char(std::string const& str, size_t& pos) {
	auto byte = [&](size_t i) { return static_cast<unsigned char>(str[i]); };
	uint32_t c = byte(pos++);
	int extra = 0;
	if (c >= 0xF0) { c &= 0x07; extra = 3; }
	else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
	else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
	else if (c >= 0x80) return 0xFFFD;
	for (; extra > 0; --extra) {
		if (pos >= str.size() || (byte(pos) & 0xC0) != 0x80)
			return 0xFFFD;
		c = (c << 6) | (byte(pos++) & 0x3F);
	}
	return c;
}
}

namespace Automation4 {
	bool CalculateTextExtents(AssStyle *style, std::string const& text, double &width, double &height, double &descent, double &extlead)
	{
		width = height = descent = extlead = 0;

		double fontsize = style->fontsize * 64;
		double spacing = style->spacing * 64;

		auto match = fonts::Find(style->font, style->bold, style->italic);
		if (!match) return false;

		std::lock_guard<std::mutex> lock(extents_mutex);
		auto font = open_font(match->face);
		if (!font) return false;

		long ppem = ppem_for_height(*font, static_cast<long>(fontsize));
		long ascent = mul_div(font->win_ascent, ppem, font->units_per_em);
		long cell_descent = mul_div(font->win_descent, ppem, font->units_per_em);
		long cell_height = ascent + cell_descent;

		if (spacing != 0) {
			for (size_t pos = 0; pos < text.size(); ) {
				uint32_t ch = next_char(text, pos);
				// The desktop version measures UTF-16 code units one by one
				int units = ch >= 0x10000 ? 2 : 1;
				width += char_width(*font, ppem, ch, match->fake_bold) + spacing * units;
				height = cell_height;
			}
		}
		else {
			long total = 0;
			for (size_t pos = 0; pos < text.size(); )
				total += char_width(*font, ppem, next_char(text, pos), match->fake_bold);
			width = total;
			height = cell_height;
		}

		descent = cell_descent;
		// el = MAX(0, LineGap - ((WinAscent + WinDescent) - (Ascender - Descender)))
		long gap = font->line_gap - ((font->win_ascent + font->win_descent) - (font->hhea_ascent - font->hhea_descent));
		extlead = std::max(0L, mul_div(gap, ppem, font->units_per_em));

		// Compensate for scaling
		width = style->scalex / 100 * width / 64;
		height = style->scaley / 100 * height / 64;
		descent = style->scaley / 100 * descent / 64;
		extlead = style->scaley / 100 * extlead / 64;

		return true;
	}
}
