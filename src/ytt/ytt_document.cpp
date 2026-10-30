// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ytt_document.cpp
/// @brief Port of SubtitleDocument's line merging and of YttDocument.Save()
/// @ingroup ytt

#include "ytt_internal.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>
#include <tuple>

namespace ytt {
namespace {

constexpr char16_t ZeroWidthSpace[] = u"\u200B";
// Surrounded with zwsp's so the reverse conversion can recognize and remove it
constexpr char16_t PaddingSpace[] = u"\u200B \u200B";

[[noreturn]] void IndexOutOfRange() {
	Fail("Index was out of range. Must be non-negative and less than the size of the collection.\r\nParameter name: index");
}

template<typename T>
T& ItemAt(std::vector<T>& list, size_t index) {
	if (index >= list.size()) IndexOutOfRange();
	return list[index];
}

template<typename T>
void InsertAt(std::vector<T>& list, size_t index, T item) {
	if (index > list.size()) IndexOutOfRange();
	list.insert(list.begin() + static_cast<ptrdiff_t>(index), std::move(item));
}

float CsMax(float a, float b) {
	if (a > b) return a;
	if (std::isnan(a)) return a;
	return b;
}

float CsMin(float a, float b) {
	if (a < b) return a;
	if (std::isnan(a)) return a;
	return b;
}

/// (int)Math.Round(x)
int RoundToInt(double x) {
	return CsInt(std::nearbyint(x));
}

/// Characters that culture-sensitive string comparisons skip over
bool IsIgnorable(char16_t c) {
	return c == 0 || c == 0x00AD || (c >= 0x200B && c <= 0x200D) || c == 0x2060 || c == 0xFEFF;
}

/// string.StartsWith(" ") (culture-sensitive)
bool CultureStartsWithSpace(u16v s) {
	size_t i = 0;
	while (i < s.size() && IsIgnorable(s[i])) ++i;
	return i < s.size() && s[i] == u' ';
}

/// string.EndsWith(" ") (culture-sensitive)
bool CultureEndsWithSpace(u16v s) {
	size_t i = s.size();
	while (i > 0 && IsIgnorable(s[i - 1])) --i;
	return i > 0 && s[i - 1] == u' ';
}

bool SameFormat(Section const& x, Section const& y, bool normalized);

}

// ---------------------------------------------------------------------------
// SubtitleDocument

void SubtitleDocument::CloseGaps() {
	std::vector<Ticks> start_times;
	for (auto const& line : lines)
		start_times.push_back(line->start);
	std::sort(start_times.begin(), start_times.end());
	start_times.erase(std::unique(start_times.begin(), start_times.end()), start_times.end());

	for (auto const& line : lines) {
		size_t end_time_idx = std::lower_bound(start_times.begin(), start_times.end(), line->end) - start_times.begin();

		int time_gap_before = end_time_idx > 0 ? CsInt(TotalMs(line->end - start_times[end_time_idx - 1])) : INT_MAX;
		int time_gap_after = end_time_idx < start_times.size() ? CsInt(TotalMs(start_times[end_time_idx] - line->end)) : INT_MAX;

		if (time_gap_before < 50 && time_gap_before < time_gap_after)
			end_time_idx--;
		else if (time_gap_after < 50 && time_gap_after <= time_gap_before)
			;
		else
			continue;

		line->end = start_times[end_time_idx];
	}
}

static void InsertConcatenatedLine(std::vector<LinePtr>& target, size_t base_index, Ticks start, Ticks end, bool after_equal_start, std::initializer_list<Line const *> source_lines) {
	auto source = source_lines.begin();
	LinePtr line = (*source)->Clone();
	for (++source; source != source_lines.end(); ++source) {
		if ((*source)->sections.empty())
			continue;

		size_t first_new_section_idx = line->sections.size();
		for (auto const& section : (*source)->sections)
			line->sections.push_back(section->Clone());
		line->sections[first_new_section_idx]->text = u"\r\n" + line->sections[first_new_section_idx]->text;
	}

	line->start = start;
	line->end = end;

	size_t index = base_index;
	while (index < target.size() && (after_equal_start ? target[index]->start <= start : target[index]->start < start))
		index++;

	target.insert(target.begin() + static_cast<ptrdiff_t>(index), line);
}

void SubtitleDocument::MergeSimultaneousLines() {
	std::vector<LinePtr> sorted;
	for (auto const& line : lines)
		if (line->start < line->end) sorted.push_back(line);
	std::stable_sort(sorted.begin(), sorted.end(), [](LinePtr const& a, LinePtr const& b) { return a->start < b->start; });

	size_t i = 0;
	while (i < sorted.size()) {
		if (sorted[i]->position) {
			i++;
			continue;
		}

		LinePtr first_line = sorted[i];
		LinePtr second_line;

		size_t j = i + 1;
		while (j < sorted.size() && sorted[j]->start < sorted[i]->end) {
			if (!sorted[j]->position && sorted[j]->anchor_point == first_line->anchor_point) {
				second_line = sorted[j];
				break;
			}
			j++;
		}

		if (!second_line) {
			i++;
			continue;
		}

		sorted.erase(sorted.begin() + static_cast<ptrdiff_t>(j));
		sorted.erase(sorted.begin() + static_cast<ptrdiff_t>(i));

		Line const *first = first_line.get();
		Line const *second = second_line.get();
		if (first->start < second->start)
			InsertConcatenatedLine(sorted, i, first->start, second->start, false, {first});

		Ticks overlap_end = std::min(first->end, second->end);
		if (IsBottomAligned(first->anchor_point) ^ (line_merge_type == LineMergeType::MoveExisting))
			InsertConcatenatedLine(sorted, i, second->start, overlap_end, false, {second, first});
		else
			InsertConcatenatedLine(sorted, i, second->start, overlap_end, false, {first, second});

		if (first->end < second->end)
			InsertConcatenatedLine(sorted, i, first->end, second->end, true, {second});
		else if (second->end < first->end)
			InsertConcatenatedLine(sorted, i, second->end, first->end, false, {first});
	}

	lines = std::move(sorted);
}

void SubtitleDocument::MergeIdenticallyFormattedSections() {
	for (auto const& line : lines)
		MergeIdenticallyFormattedSections(*line);
}

void SubtitleDocument::MergeIdenticallyFormattedSections(Line& line) {
	size_t i = 0;
	while (i + 1 < line.sections.size()) {
		auto& a = *line.sections[i];
		auto& b = *line.sections[i + 1];
		if (SameFormat(a, b, false) && a.start_offset == b.start_offset) {
			a.text += b.text;
			line.sections.erase(line.sections.begin() + static_cast<ptrdiff_t>(i) + 1);
		}
		else
			i++;
	}
}

void SubtitleDocument::MoveLineBreaksToSeparateSections(Line& line) {
	for (ptrdiff_t section_idx = static_cast<ptrdiff_t>(line.sections.size()) - 1; section_idx >= 0; --section_idx) {
		SectionPtr section = line.sections[section_idx];
		u16 const& text = section->text;

		// ([\r\n]+|[^\r\n]+)+
		std::vector<u16> runs;
		for (size_t pos = 0; pos < text.size(); ) {
			bool newline = text[pos] == u'\r' || text[pos] == u'\n';
			size_t end = pos;
			while (end < text.size() && (text[end] == u'\r' || text[end] == u'\n') == newline) ++end;
			runs.emplace_back(text.substr(pos, end - pos));
			pos = end;
		}
		if (runs.size() <= 1)
			continue;

		line.sections.erase(line.sections.begin() + section_idx);
		for (size_t i = 0; i < runs.size(); ++i) {
			auto sub_section = section->Clone();
			sub_section->text = runs[i];
			line.sections.insert(line.sections.begin() + section_idx + static_cast<ptrdiff_t>(i), sub_section);
		}
	}
}

// ---------------------------------------------------------------------------
// YttDocument

namespace {

int GetAnchorPointId(AnchorPoint anchor_point) {
	return static_cast<int>(anchor_point);
}

int GetJustificationId(AnchorPoint anchor_point) {
	if (IsLeftAligned(anchor_point)) return 0;
	if (IsRightAligned(anchor_point)) return 1;
	return 2;
}

std::pair<int, int> GetPrintAndScrollDirectionIds(HorizontalTextDirection hor, VerticalTextType ver) {
	switch (ver) {
		case VerticalTextType::Positioned: return {2, hor == HorizontalTextDirection::RightToLeft ? 0 : 1};
		case VerticalTextType::Rotated:    return {3, hor == HorizontalTextDirection::LeftToRight ? 0 : 1};
		default:                           return {hor == HorizontalTextDirection::LeftToRight ? 0 : 1, 0};
	}
}

int GetEdgeTypeId(ShadowType type) {
	switch (type) {
		case ShadowType::HardShadow: return 1;
		case ShadowType::Bevel:      return 2;
		case ShadowType::Glow:       return 3;
		case ShadowType::SoftShadow: return 4;
	}
	Fail("Specified argument was out of the range of valid values.");
}

int GetOffsetTypeId(OffsetType type) {
	switch (type) {
		case OffsetType::Subscript:   return 0;
		case OffsetType::Regular:     return 1;
		case OffsetType::Superscript: return 2;
	}
	Fail("Specified argument was out of the range of valid values.");
}

int GetRubyPartId(RubyPart part) {
	switch (part) {
		case RubyPart::None:        return 0;
		case RubyPart::Base:        return 1;
		case RubyPart::Parenthesis: return 2;
		case RubyPart::TextBefore:  return 4;
		case RubyPart::TextAfter:   return 5;
	}
	Fail("Specified argument was out of the range of valid values.");
}

int GetFontStyleId(std::optional<u16> const& font) {
	if (!font)
		return 0;
	static const std::map<u16, int> ids = {
		{u"courier new", 1}, {u"courier", 1}, {u"nimbus mono l", 1}, {u"cutive mono", 1},
		{u"times new roman", 2}, {u"times", 2}, {u"georgia", 2}, {u"cambria", 2}, {u"pt serif caption", 2},
		// The incorrect font name from YouTube's captions.js, as well as the correct name
		{u"deja vu sans mono", 3}, {u"dejavu sans mono", 3},
		{u"lucida console", 3}, {u"monaco", 3}, {u"consolas", 3}, {u"pt mono", 3},
		{u"comic sans ms", 5}, {u"impact", 5}, {u"handlee", 5},
		{u"monotype corsiva", 6}, {u"urw chancery l", 6}, {u"apple chancery", 6}, {u"dancing script", 6},
		{u"carrois gothic sc", 7},
	};
	auto it = ids.find(ToLower(*font));
	return it == ids.end() ? 0 : it->second;
}

float GetRealFontScale(int ytt_scale) {
	return 1 + ((ytt_scale / 100.f) - 1) / 4;
}

int GetYouTubeFontScale(float real_scale) {
	float ytt_scale = CsMax(1 + (real_scale - 1) * 4, 0);
	return RoundToInt(ytt_scale * 100);
}

/// SectionFormatComparer and NormalizedSectionFormatComparer
bool SameFormat(Section const& x, Section const& y, bool normalized) {
	auto font = [&](Section const& s) -> std::optional<u16> {
		if (normalized) return u16(1, static_cast<char16_t>(GetFontStyleId(s.font)));
		return s.font;
	};
	auto scale = [&](Section const& s) {
		return normalized ? GetRealFontScale(GetYouTubeFontScale(s.scale)) : s.scale;
	};

	if (x.bold != y.bold ||
		x.italic != y.italic ||
		x.underline != y.underline ||
		font(x) != font(y) ||
		std::fabs(scale(x) - scale(y)) > 0.001 ||
		x.offset != y.offset ||
		x.fore_color.ToArgb() != y.fore_color.ToArgb() ||
		x.back_color.ToArgb() != y.back_color.ToArgb() ||
		x.ruby_part != y.ruby_part ||
		x.packed != y.packed ||
		x.shadow_colors.Count() != y.shadow_colors.Count())
		return false;

	for (auto const& [type, color] : x.shadow_colors.Items()) {
		auto other = y.shadow_colors.TryGet(type);
		if (!other || color.ToArgb() != other->ToArgb())
			return false;
	}
	return true;
}

/// Minimal XmlWriter producing the same bytes as .NET's for what YttDocument writes
class XmlWriter {
	std::string out;
	std::vector<std::string> open_elements;
	bool in_start_tag = false;
	size_t content_pos = 0;

	void CloseStartTag() {
		if (in_start_tag) {
			out += '>';
			content_pos = out.size();
			in_start_tag = false;
		}
	}

	[[noreturn]] static void InvalidChar(char16_t c) {
		char buf[96];
		snprintf(buf, sizeof buf, "'%s', hexadecimal value 0x%02X, is an invalid character.",
			c < 0x20 ? "" : Utf16ToUtf8(u16v(&c, 1)).c_str(), c);
		Fail(buf);
	}

public:
	XmlWriter() {
		out = "\xEF\xBB\xBF<?xml version=\"1.0\" encoding=\"utf-8\"?>";
	}

	void WriteStartElement(std::string name) {
		CloseStartTag();
		out += '<';
		out += name;
		open_elements.push_back(std::move(name));
		in_start_tag = true;
	}

	void WriteAttributeString(char const *name, std::string const& value) {
		out += ' ';
		out += name;
		out += "=\"";
		out += value;
		out += '"';
	}

	void WriteAttributeString(char const *name, int64_t value) {
		WriteAttributeString(name, IntToString(value));
	}

	void WriteString(u16v text) {
		CloseStartTag();
		// Even an empty string prevents the shortcut syntax for the end tag
		content_pos = std::string::npos;
		u16 escaped;
		for (size_t i = 0; i < text.size(); ++i) {
			char16_t c = text[i];
			switch (c) {
				case u'<': escaped += u"&lt;"; break;
				case u'>': escaped += u"&gt;"; break;
				case u'&': escaped += u"&amp;"; break;
				case u'\n': escaped += u'\n'; break;
				case u'\r':
					if (i + 1 < text.size() && text[i + 1] == u'\n') ++i;
					escaped += u'\n';
					break;
				case u'\t': escaped += c; break;
				default:
					if (c >= 0xD800 && c <= 0xDBFF) {
						if (i + 1 >= text.size() || text[i + 1] < 0xDC00 || text[i + 1] > 0xDFFF)
							Fail("The surrogate pair is invalid. Missing a low surrogate character.");
						escaped += c;
						escaped += text[++i];
					}
					else if (c >= 0xDC00 && c <= 0xDFFF)
						Fail("Invalid high surrogate character.");
					else if (c < 0x20 || c == 0xFFFE || c == 0xFFFF)
						InvalidChar(c);
					else
						escaped += c;
			}
		}
		out += Utf16ToUtf8(escaped);
	}

	void WriteEndElement() {
		if (in_start_tag) {
			CloseStartTag();
		}
		if (content_pos == out.size()) {
			// Nothing was written since the start tag: use the shortcut syntax
			out.pop_back();
			out += " />";
		}
		else {
			out += "</";
			out += open_elements.back();
			out += '>';
		}
		open_elements.pop_back();
	}

	std::string Finish() {
		while (!open_elements.empty())
			WriteEndElement();
		return std::move(out);
	}
};

/// Dictionary<T, int> filled by SubtitleDocument.ExtractAttributes
template<typename Key>
class AttributeIds {
	std::map<Key, int> ids;

public:
	/// Returns whether the key is new
	bool Add(Key key) {
		return ids.emplace(std::move(key), 1 + static_cast<int>(ids.size())).second;
	}

	int At(Key const& key) const {
		auto it = ids.find(key);
		if (it == ids.end())
			Fail("The given key was not present in the dictionary.");
		return it->second;
	}
};

class YttDocument final : public SubtitleDocument {
	using PositionKey = std::tuple<int, int, int>;
	using WindowStyleKey = std::tuple<int, int, int>;
	using PenKey = std::tuple<bool, bool, bool, int, int, int, int32_t, int32_t, int, bool, std::vector<std::pair<int, int32_t>>>;

	struct Point {
		int x, y;
	};

	int GetYouTubeCoord(float pixel_coord, float max_value) const {
		float percentage = pixel_coord / max_value * 100;
		percentage = (percentage - 2) / 0.96f;
		percentage = CsMax(percentage, 0);
		percentage = CsMin(percentage, 100);
		return RoundToInt(percentage);
	}

	Point GetYouTubePosition(PointF pixel_pos) const {
		return {
			GetYouTubeCoord(pixel_pos.x, static_cast<float>(video_dimensions.width)),
			GetYouTubeCoord(pixel_pos.y, static_cast<float>(video_dimensions.height)),
		};
	}

	Point GetYouTubePosition(Line const& line) const {
		return GetYouTubePosition(line.position ? *line.position : GetDefaultPosition(line.anchor_point));
	}

	PositionKey GetPositionKey(Line const& line) const {
		Point pos = GetYouTubePosition(line);
		return {static_cast<int>(line.anchor_point), pos.x, pos.y};
	}

	static WindowStyleKey GetWindowStyleKey(Line const& line) {
		return {GetJustificationId(line.anchor_point), static_cast<int>(line.horizontal_text_direction), static_cast<int>(line.vertical_text_type)};
	}

	/// Two sections get the same key exactly when NormalizedSectionFormatComparer considers them equal
	static PenKey GetPenKey(Section const& s) {
		std::vector<std::pair<int, int32_t>> shadows;
		for (auto const& [type, color] : s.shadow_colors.Items())
			shadows.emplace_back(static_cast<int>(type), color.ToArgb());
		std::sort(shadows.begin(), shadows.end());
		return {s.bold, s.italic, s.underline, GetFontStyleId(s.font), GetYouTubeFontScale(s.scale),
			static_cast<int>(s.offset), s.fore_color.ToArgb(), s.back_color.ToArgb(),
			static_cast<int>(s.ruby_part), s.packed, std::move(shadows)};
	}

	void ApplyEnhancements();
	void AddItalicPrefetch();
	void MakeInvisibleTextBlack(size_t line_index);
	void PreventShadowClipping(size_t line_index);
	void HardenSpaces(size_t line_index);
	void LimitColors(size_t line_index);
	size_t ExpandLineForMultiShadows(size_t line_index);
	size_t ExpandLineForDarkText(size_t line_index);
	static void ApplyManualLinePadding(Line& line);
	static void AvoidZeroDurationKaraoke(Line& line);

	void WriteWindowPosition(XmlWriter& writer, int id, Line const& position) const;
	static void WriteWindowStyle(XmlWriter& writer, int id, Line const& style);
	static void WritePen(XmlWriter& writer, int id, Section const& format);
	void WriteLine(XmlWriter& writer, Line const& line, AttributeIds<PositionKey> const& positions,
		AttributeIds<WindowStyleKey> const& window_styles, AttributeIds<PenKey> const& pens) const;

public:
	explicit YttDocument(SubtitleDocument const& doc) : SubtitleDocument(doc, true) { }
	std::string Save();
};

std::string YttDocument::Save() {
	CloseGaps();
	MergeSimultaneousLines();
	MergeIdenticallyFormattedSections();
	ApplyEnhancements();
	MergeIdenticallyFormattedSections();

	AttributeIds<PositionKey> position_ids;
	std::vector<Line const *> positions;
	for (auto const& line : lines)
		if (position_ids.Add(GetPositionKey(*line))) positions.push_back(line.get());

	AttributeIds<WindowStyleKey> window_style_ids;
	std::vector<Line const *> window_styles;
	for (auto const& line : lines)
		if (window_style_ids.Add(GetWindowStyleKey(*line))) window_styles.push_back(line.get());

	AttributeIds<PenKey> pen_ids;
	std::vector<Section const *> pens;
	for (auto const& line : lines)
		for (auto const& section : line->sections)
			if (pen_ids.Add(GetPenKey(*section))) pens.push_back(section.get());

	XmlWriter writer;
	writer.WriteStartElement("timedtext");
	writer.WriteAttributeString("format", "3");

	writer.WriteStartElement("head");

	// The iOS app ignores the background color for the first pen and might have other,
	// similar bugs too, so a dummy (unused) item is written for each of the lists
	Line dummy_position(0, 0);
	dummy_position.position = PointF{};
	WriteWindowPosition(writer, 0, dummy_position);
	for (size_t i = 0; i < positions.size(); ++i)
		WriteWindowPosition(writer, static_cast<int>(i) + 1, *positions[i]);

	WriteWindowStyle(writer, 0, Line(0, 0));
	for (size_t i = 0; i < window_styles.size(); ++i)
		WriteWindowStyle(writer, static_cast<int>(i) + 1, *window_styles[i]);

	WritePen(writer, 0, Section());
	for (size_t i = 0; i < pens.size(); ++i)
		WritePen(writer, static_cast<int>(i) + 1, *pens[i]);

	writer.WriteEndElement();

	writer.WriteStartElement("body");
	for (auto const& line : lines)
		WriteLine(writer, *line, position_ids, window_style_ids, pen_ids);
	writer.WriteEndElement();

	writer.WriteEndElement();
	return writer.Finish();
}

void YttDocument::ApplyEnhancements() {
	AddItalicPrefetch();
	for (size_t i = 0; i < lines.size(); i++) {
		MakeInvisibleTextBlack(i);
		PreventShadowClipping(i);
		HardenSpaces(i);
		LimitColors(i);
		i += ExpandLineForMultiShadows(i) - 1;
		i += ExpandLineForDarkText(i) - 1;
	}

	for (auto const& line : lines) {
		ApplyManualLinePadding(*line);
		AvoidZeroDurationKaraoke(*line);
	}
}

/// On PC, the first piece of italic text shows up with a noticeable delay, so an
/// invisible italic subtitle is added at the start to make YouTube load it eagerly
void YttDocument::AddItalicPrefetch() {
	bool any_italic = std::any_of(lines.begin(), lines.end(), [](LinePtr const& l) {
		return std::any_of(l->sections.begin(), l->sections.end(), [](SectionPtr const& s) { return s->italic; });
	});
	if (!any_italic)
		return;

	auto italic_line = std::make_shared<Line>(RoundTimeToFrameCenter(Ms(5000)), RoundTimeToFrameCenter(Ms(5100)));
	italic_line->position = PointF{0, 0};
	italic_line->anchor_point = AnchorPoint::BottomRight;
	auto section = std::make_shared<Section>(ZeroWidthSpace);
	section->fore_color = Color::FromArgb(1, 255, 255, 255);
	section->back_color = Color();
	section->italic = true;
	italic_line->sections.push_back(section);
	lines.push_back(italic_line);
}

/// The Android app doesn't support text transparency, so invisible text is made
/// black to melt into the app's black background
void YttDocument::MakeInvisibleTextBlack(size_t line_index) {
	for (auto const& section : lines[line_index]->sections) {
		if (section->fore_color.A() == 0)
			section->fore_color = Color::FromArgb(0, 0, 0, 0);
	}
}

/// Gives shadows room to fall into by moving a space from the next section (or,
/// for RTL text, the preceding one) into a section with a shadow
void YttDocument::PreventShadowClipping(size_t line_index) {
	Line& line = *lines[line_index];

	for (size_t i = 0; i < line.sections.size(); i++) {
		Section& curr = *line.sections[i];
		if (curr.ruby_part == RubyPart::Base) {
			i += 3;
			continue;
		}

		if (curr.shadow_colors.Count() == 0 || curr.underline)
			continue;

		if (std::any_of(curr.text.begin(), curr.text.end(), IsRightToLeft)) {
			if (CultureStartsWithSpace(curr.text) || i == 0)
				continue;

			Section& prev = *line.sections[i - 1];
			if (prev.ruby_part == RubyPart::Parenthesis || !CultureEndsWithSpace(prev.text))
				continue;

			prev.text.pop_back();
			curr.text = u" " + curr.text;
		}
		else {
			if (CultureEndsWithSpace(curr.text) || i == line.sections.size() - 1)
				continue;

			Section& next = *line.sections[i + 1];
			if (next.ruby_part == RubyPart::Base || !CultureStartsWithSpace(next.text))
				continue;

			curr.text += u' ';
			next.text.erase(0, 1);
		}
	}
}

/// Browsers collapse sequences of spaces, so they're replaced by non-breaking spaces
void YttDocument::HardenSpaces(size_t line_index) {
	for (auto const& section : lines[line_index]->sections) {
		u16& text = section->text;
		for (size_t pos = 0; pos < text.size(); ) {
			if (text[pos] != u' ') {
				++pos;
				continue;
			}
			size_t end = pos;
			while (end < text.size() && text[end] == u' ') ++end;
			if (end - pos >= 2)
				std::fill(text.begin() + static_cast<ptrdiff_t>(pos), text.begin() + static_cast<ptrdiff_t>(end), u'\u00A0');
			pos = end;
		}
	}
}

/// Opacities of 255 get lost during upload, and the Android app ignores pure
/// white, so both are avoided
void YttDocument::LimitColors(size_t line_index) {
	for (auto const& section_ptr : lines[line_index]->sections) {
		Section& section = *section_ptr;
		if ((section.fore_color.ToArgb() & 0xFFFFFF) == 0xFFFFFF)
			section.fore_color = Color::FromArgb(static_cast<uint32_t>(section.fore_color.A()) << 24 | 0xFEFEFE);

		if (section.fore_color.A() == 255)
			section.fore_color = ChangeAlpha(section.fore_color, 254);

		if (section.back_color.A() == 255)
			section.back_color = ChangeAlpha(section.back_color, 254);

		// Remove invisible shadows
		section.shadow_colors.RemoveAll([](ShadowType, Color c) { return c.A() == 0; });

		// YouTube doesn't have a shadow opacity attribute, so it's set to 254 to avoid
		// superfluous pens. The exception is #222222 which does support opacity, in a way.
		std::vector<ShadowType> to_change;
		for (auto const& [type, color] : section.shadow_colors.Items()) {
			if (color.A() != 254 && ((color.ToArgb() & 0xFFFFFF) != 0x222222 || color.A() != section.fore_color.A()))
				to_change.push_back(type);
		}

		for (ShadowType type : to_change)
			section.shadow_colors.Set(type, ChangeAlpha(section.shadow_colors.At(type), 254));
	}
}

/// Multiple shadow types on one subtitle are supported by duplicating it
size_t YttDocument::ExpandLineForMultiShadows(size_t line_index) {
	LinePtr line = lines[line_index];
	if (line->sections.empty())
		Fail("Sequence contains no elements");
	size_t max_num_shadows = 0;
	for (auto const& section : line->sections)
		max_num_shadows = std::max(max_num_shadows, section->shadow_colors.Count());
	if (max_num_shadows <= 1)
		return 1;

	static const ShadowType ordered_shadow_types[] = {ShadowType::SoftShadow, ShadowType::HardShadow, ShadowType::Bevel, ShadowType::Glow};
	std::vector<std::vector<ShadowType>> line_layer_shadow_types;
	for (auto const& section : line->sections) {
		std::vector<ShadowType> section_layer_shadow_types;
		for (ShadowType type : ordered_shadow_types) {
			if (section->shadow_colors.ContainsKey(type))
				section_layer_shadow_types.push_back(type);
		}
		line_layer_shadow_types.push_back(std::move(section_layer_shadow_types));
	}

	lines.erase(lines.begin() + static_cast<ptrdiff_t>(line_index));

	for (size_t layer_idx = 0; layer_idx < max_num_shadows; layer_idx++) {
		LinePtr shadow_line = line->Clone();
		for (size_t section_idx = 0; section_idx < shadow_line->sections.size(); section_idx++) {
			Section& section = *shadow_line->sections[section_idx];
			auto const& section_layer_shadow_types = line_layer_shadow_types[section_idx];

			if (layer_idx > 0)
				section.back_color = Color();

			if (layer_idx < section_layer_shadow_types.size()) {
				ShadowType keep = section_layer_shadow_types[layer_idx];
				section.shadow_colors.RemoveAll([&](ShadowType t, Color) { return t != keep; });
			}
			else
				section.shadow_colors.Clear();
		}

		lines.insert(lines.begin() + static_cast<ptrdiff_t>(line_index + layer_idx), shadow_line);
	}

	return max_num_shadows;
}

/// The mobile apps have an unchangeable black background, so dark text is
/// overlapped with an invisible bright copy that only shows up on Android
size_t YttDocument::ExpandLineForDarkText(size_t line_idx) {
	LinePtr line = lines[line_idx];
	bool has_dark_text = std::any_of(line->sections.begin(), line->sections.end(), [](SectionPtr const& s) {
		return s->fore_color.A() > 0 && IsDark(s->fore_color);
	});
	if (!line->android_dark_text_hack_allowed || !has_dark_text)
		return 1;

	LinePtr bright_line = line->Clone();
	for (auto const& section : bright_line->sections) {
		if (section->fore_color.A() > 0 && IsDark(section->fore_color))
			section->fore_color = Brighten(section->fore_color);

		section->fore_color = ChangeAlpha(section->fore_color, 0);
		section->back_color = ChangeAlpha(section->back_color, 0);
		section->shadow_colors.Clear();
	}

	lines.insert(lines.begin() + static_cast<ptrdiff_t>(line_idx) + 1, bright_line);
	return 2;
}

static SectionPtr CreatePaddingSection(Section const& section_template, u16v text, Ticks start_offset) {
	auto padding = section_template.Clone();
	padding->text = u16(text);
	padding->ruby_part = RubyPart::None;
	padding->underline = false;
	padding->start_offset = start_offset;
	return padding;
}

/// Works around YouTube's padding bugs (lost padding next to line breaks, no RTL
/// awareness) and makes sure every line has multiple sections and a zwsp
void YttDocument::ApplyManualLinePadding(Line& line) {
	if (line.vertical_text_type == VerticalTextType::Positioned)
		return;

	MoveLineBreaksToSeparateSections(line);

	// A (hopefully) unique color combination so the padding sections won't be
	// merged with others during upload
	Section hidden_template;
	hidden_template.back_color = Color::FromArgb(0, 130, 140, 150);
	hidden_template.fore_color = Color::FromArgb(0, 160, 170, 180);

	size_t i = 0;
	while (i < line.sections.size()) {
		SectionPtr section = line.sections[i];
		size_t next_section_idx = i + (section->ruby_part == RubyPart::Base ? 4 : 1);

		hidden_template.scale = section->scale;

		if (Contains(section->text, u"\r\n")) {
			line.sections[i] = CreatePaddingSection(hidden_template, section->text, section->start_offset);
			i = next_section_idx;
			continue;
		}

		if (i == 0 || EndsWith(ItemAt(line.sections, i - 1)->text, u"\r\n")) {
			InsertAt(line.sections, i + 0, CreatePaddingSection(hidden_template, ZeroWidthSpace, section->start_offset));
			InsertAt(line.sections, i + 1, CreatePaddingSection(*section, PaddingSpace, section->start_offset));
			next_section_idx += 2;
		}

		if (next_section_idx == line.sections.size() || StartsWith(ItemAt(line.sections, next_section_idx)->text, u"\r\n")) {
			InsertAt(line.sections, next_section_idx + 0, CreatePaddingSection(*section, PaddingSpace, section->start_offset));
			InsertAt(line.sections, next_section_idx + 1, CreatePaddingSection(hidden_template, ZeroWidthSpace, section->start_offset));
			next_section_idx += 2;
		}

		i = next_section_idx;
	}
}

/// The upload process breaks zero-duration karaoke segments, so they get 1ms instead
void YttDocument::AvoidZeroDurationKaraoke(Line& line) {
	for (size_t i = 1; i < line.sections.size(); i++) {
		auto const& prev = *line.sections[i - 1];
		auto& section = *line.sections[i];
		if (section.start_offset > 0 && prev.start_offset >= section.start_offset)
			section.start_offset = prev.start_offset + Ms(1);
	}
}

void YttDocument::WriteWindowPosition(XmlWriter& writer, int id, Line const& position) const {
	Point pos = GetYouTubePosition(position);
	writer.WriteStartElement("wp");
	writer.WriteAttributeString("id", id);
	writer.WriteAttributeString("ap", GetAnchorPointId(position.anchor_point));
	writer.WriteAttributeString("ah", pos.x);
	writer.WriteAttributeString("av", pos.y);
	writer.WriteEndElement();
}

void YttDocument::WriteWindowStyle(XmlWriter& writer, int id, Line const& style) {
	writer.WriteStartElement("ws");
	writer.WriteAttributeString("id", id);
	writer.WriteAttributeString("ju", GetJustificationId(style.anchor_point));
	auto [pd, sd] = GetPrintAndScrollDirectionIds(style.horizontal_text_direction, style.vertical_text_type);
	writer.WriteAttributeString("pd", pd);
	writer.WriteAttributeString("sd", sd);
	writer.WriteAttributeString("wfo", "0");
	writer.WriteEndElement();
}

void YttDocument::WritePen(XmlWriter& writer, int id, Section const& format) {
	writer.WriteStartElement("pen");
	writer.WriteAttributeString("id", id);

	int font_style_id = GetFontStyleId(format.font);
	if (font_style_id != 0)
		writer.WriteAttributeString("fs", font_style_id);

	writer.WriteAttributeString("sz", GetYouTubeFontScale(format.scale));

	if (format.offset != OffsetType::Regular)
		writer.WriteAttributeString("of", GetOffsetTypeId(format.offset));

	if (format.bold)
		writer.WriteAttributeString("b", "1");
	if (format.italic)
		writer.WriteAttributeString("i", "1");
	if (format.underline)
		writer.WriteAttributeString("u", "1");

	writer.WriteAttributeString("fc", ColorToHtml(format.fore_color.IsEmpty() ? Color::FromArgb(0xFF000000u) : format.fore_color));
	writer.WriteAttributeString("fo", format.fore_color.A());

	if (format.back_color.A() > 0)
		writer.WriteAttributeString("bc", ColorToHtml(format.back_color));

	writer.WriteAttributeString("bo", format.back_color.A());

	if (format.shadow_colors.Count() > 0) {
		if (format.shadow_colors.Count() > 1)
			Fail("YTT lines must be reduced to one shadow color before saving");

		auto [type, color] = format.shadow_colors.First();
		if (color.A() > 0) {
			writer.WriteAttributeString("et", GetEdgeTypeId(type));

			// An "ec" attribute makes the shadow fully opaque, while the default
			// (#222222) follows the foreground transparency, so it's only written
			// when it has to be
			if ((color.ToArgb() & 0xFFFFFF) != 0x222222 || color.A() != format.fore_color.A())
				writer.WriteAttributeString("ec", ColorToHtml(color));
		}
	}

	if (format.ruby_part != RubyPart::None)
		writer.WriteAttributeString("rb", GetRubyPartId(format.ruby_part));

	if (format.packed)
		writer.WriteAttributeString("hg", "1");

	writer.WriteEndElement();
}

void YttDocument::WriteLine(XmlWriter& writer, Line const& line, AttributeIds<PositionKey> const& positions,
	AttributeIds<WindowStyleKey> const& window_styles, AttributeIds<PenKey> const& pens) const
{
	if (line.sections.empty())
		return;

	int line_start_ms = CsInt(TotalMs(line.start));
	int line_duration_ms = CsInt(TotalMs(line.end - line.start));

	// Lines starting at or before 0ms are moved to 1ms, as the Android app doesn't
	// respect the positioning of (or sometimes doesn't display) subtitles at 0ms
	if (line_start_ms <= 0) {
		line_duration_ms -= -line_start_ms + 1;
		line_start_ms = 1;
	}

	if (line_duration_ms <= 0)
		return;

	writer.WriteStartElement("p");
	writer.WriteAttributeString("t", line_start_ms);
	writer.WriteAttributeString("d", line_duration_ms);
	if (line.sections.size() == 1)
		writer.WriteAttributeString("p", pens.At(GetPenKey(*line.sections[0])));

	writer.WriteAttributeString("wp", positions.At(GetPositionKey(line)));
	writer.WriteAttributeString("ws", window_styles.At(GetWindowStyleKey(line)));

	if (line.sections.size() == 1)
		writer.WriteString(line.sections[0]->text);
	else {
		// The server removes the pen of the first section unless the line has text
		// outside of any section, so a zero-width space is added after it
		size_t multi_section_workaround_idx = line.sections[0]->ruby_part == RubyPart::None ? 0 : 3;
		for (size_t i = 0; i < line.sections.size(); i++) {
			Section const& section = *line.sections[i];
			writer.WriteStartElement("s");
			writer.WriteAttributeString("p", pens.At(GetPenKey(section)));
			if (section.start_offset > 0)
				writer.WriteAttributeString("t", CsInt(TotalMs(section.start_offset)));
			writer.WriteString(section.text);
			writer.WriteEndElement();

			if (i == multi_section_workaround_idx)
				writer.WriteString(ZeroWidthSpace);
		}
	}

	writer.WriteEndElement();
}

}

std::string SaveYttDocument(SubtitleDocument const& doc) {
	return YttDocument(doc).Save();
}

}
