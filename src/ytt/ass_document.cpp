// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ass_document.cpp
/// @brief Port of AssDocument, its tag handlers, karaoke types and the Animator
/// @ingroup ytt

#include "ytt_internal.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

namespace ytt {
namespace {

constexpr char16_t ZeroWidthSpace[] = u"\u200B";

template<typename T>
T& Req(T *ptr) {
	if (!ptr)
		Fail("Object reference not set to an instance of an object.");
	return *ptr;
}

struct AssStyle {
	u16 name;
	u16 font;
	float line_height = 0;
	bool bold = false;
	bool italic = false;
	bool underline = false;
	Color primary_color;
	Color secondary_color;
	Color outline_color;
	float outline_thickness = 0;
	bool outline_is_box = false;
	Color shadow_color;
	float shadow_distance = 0;
	AnchorPoint anchor_point = AnchorPoint::BottomCenter;

	bool HasOutline() const { return outline_thickness > 0; }
	bool HasOutlineBox() const { return HasOutline() && outline_is_box; }
	bool HasShadow() const { return shadow_distance > 0; }
};

AnchorPoint GetAnchorPoint(int alignment) {
	switch (alignment) {
		case 1: return AnchorPoint::BottomLeft;
		case 2: return AnchorPoint::BottomCenter;
		case 3: return AnchorPoint::BottomRight;
		case 4: return AnchorPoint::MiddleLeft;
		case 5: return AnchorPoint::Center;
		case 6: return AnchorPoint::MiddleRight;
		case 7: return AnchorPoint::TopLeft;
		case 8: return AnchorPoint::TopCenter;
		case 9: return AnchorPoint::TopRight;
	}
	Fail(std::to_string(alignment) + " is not a valid alignment");
}

// ---------------------------------------------------------------------------
// File structure

struct DocumentSection;

struct DocumentItem {
	DocumentSection const *section;
	u16 type;
	std::vector<u16> values;

	u16 const& GetString(u16 const& field) const;
	int GetInt(u16 const& field, int default_value = 0) const {
		int result;
		return TryParseInt(GetString(field), result) ? result : default_value;
	}
	float GetFloat(u16 const& field, float default_value = 0) const {
		float result;
		return TryParseFloatAny(GetString(field), result) ? result : default_value;
	}
	bool GetBool(u16 const& field) const { return GetInt(field, 0) != 0; }
	Color GetColor(u16 const& field) const;
	Ticks GetTimestamp(u16 const& field) const;
};

struct DocumentSection {
	std::optional<CsDict<u16, int>> format;
	std::vector<DocumentItem> items;

	std::optional<u16> GetItemString(u16v type) const {
		for (auto const& item : items) {
			if (item.type == type) {
				if (item.values.empty()) return std::nullopt;
				return item.values.front();
			}
		}
		return std::nullopt;
	}

	int GetItemInt(u16v type, int default_value) const {
		auto value = GetItemString(type);
		int result;
		return value && TryParseInt(*value, result) ? result : default_value;
	}
};

u16 const& DocumentItem::GetString(u16 const& field) const {
	auto const& format = Req(section->format ? &*section->format : nullptr);
	size_t index = static_cast<size_t>(format.At(field));
	if (index >= values.size())
		Fail("Index was out of range. Must be non-negative and less than the size of the collection.");
	return values[index];
}

Color DocumentItem::GetColor(u16 const& field) const {
	u16 const& value = GetString(field);
	uint32_t abgr;
	if (value.size() != 10 || !StartsWith(value, u"&H") || !TryParseHex(u16v(value).substr(2), false, abgr))
		Fail(Utf16ToUtf8(value) + " is not a valid color");
	int a = 255 - static_cast<int>(abgr >> 24);
	int r = abgr & 0xFF;
	int g = (abgr >> 8) & 0xFF;
	int b = (abgr >> 16) & 0xFF;
	return Color::FromArgb(a & 0xFF, r, g, b);
}

bool IsAsciiDigit(char16_t c) { return c >= u'0' && c <= u'9'; }

Ticks DocumentItem::GetTimestamp(u16 const& field) const {
	// ^(\d+):(\d\d):(\d\d)\.(\d\d)$
	u16 const& value = GetString(field);
	auto fail = [&]() -> Ticks { Fail(Utf16ToUtf8(value) + " is not a valid timestamp"); };
	size_t colon = value.find(u':');
	if (colon == u16::npos || colon == 0 || value.size() != colon + 9)
		return fail();
	for (size_t i = 0; i < colon; ++i)
		if (!IsAsciiDigit(value[i])) return fail();
	auto two = [&](size_t pos) -> int {
		if (!IsAsciiDigit(value[pos]) || !IsAsciiDigit(value[pos + 1])) return -1;
		return (value[pos] - u'0') * 10 + (value[pos + 1] - u'0');
	};
	int minutes = two(colon + 1);
	int seconds = two(colon + 4);
	int centis = two(colon + 7);
	if (value[colon + 3] != u':' || value[colon + 6] != u'.' || minutes < 0 || seconds < 0 || centis < 0)
		return fail();
	int hours;
	if (!TryParseInt(u16v(value).substr(0, colon), hours))
		Fail("Value was either too large or too small for an Int32.");
	if (minutes > 59 || seconds > 59)
		Fail("Hour, Minute, and Second parameters describe an un-representable DateTime.");
	return AddMs((static_cast<Ticks>(minutes) * 60 + seconds) * 1000 * TicksPerMs + Ms(centis * 10), hours * 3600000.0);
}

struct AssDialogue {
	int layer;
	Ticks start;
	Ticks end;
	u16 style;
	u16 effect;
	u16 text;

	explicit AssDialogue(DocumentItem const& item)
	: layer(item.GetInt(u"Layer"))
	, start(item.GetTimestamp(u"Start"))
	, end(item.GetTimestamp(u"End"))
	, style(item.GetString(u"Style"))
	, effect(item.GetString(u"Effect"))
	, text(item.GetString(u"Text"))
	{
	}
};

AssStyle ParseStyle(DocumentItem const& item) {
	AssStyle style;
	style.name = item.GetString(u"Name");
	style.font = item.GetString(u"Fontname");
	style.line_height = item.GetFloat(u"Fontsize");
	style.bold = item.GetBool(u"Bold");
	style.italic = item.GetBool(u"Italic");
	style.underline = item.GetBool(u"Underline");
	style.primary_color = item.GetColor(u"PrimaryColour");
	style.secondary_color = item.GetColor(u"SecondaryColour");
	style.outline_color = item.GetColor(u"OutlineColour");
	style.outline_thickness = item.GetFloat(u"Outline");
	style.outline_is_box = item.GetInt(u"BorderStyle") == 3;
	style.shadow_color = item.GetColor(u"BackColour");
	style.shadow_distance = item.GetFloat(u"Shadow");
	style.anchor_point = GetAnchorPoint(item.GetInt(u"Alignment"));
	return style;
}

/// TextReader.ReadLine over the whole document
std::vector<u16v> ReadLines(u16v text) {
	std::vector<u16v> lines;
	size_t start = 0;
	while (start < text.size()) {
		size_t end = start;
		while (end < text.size() && text[end] != u'\r' && text[end] != u'\n') ++end;
		lines.push_back(text.substr(start, end - start));
		if (end < text.size() && text[end] == u'\r' && end + 1 < text.size() && text[end + 1] == u'\n')
			start = end + 2;
		else
			start = end + 1;
	}
	return lines;
}

/// Approximation of the regex class [\w ]
bool IsWordOrSpace(char16_t c) {
	if (c == u' ' || c == u'_' || IsAsciiDigit(c) || (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'))
		return true;
	if (c < 0x80)
		return false;
	return !IsWhiteSpace(c) && !(c >= 0x2000 && c <= 0x206F) && !(c >= 0x3000 && c <= 0x3003)
		&& !(c >= 0xFF01 && c <= 0xFF0F) && c != 0xA1 && c != 0xBF && c != 0xAB && c != 0xBB;
}

CsDict<u16, std::shared_ptr<DocumentSection>> ReadDocument(u16v text) {
	CsDict<u16, std::shared_ptr<DocumentSection>> sections;
	std::shared_ptr<DocumentSection> current;

	for (u16v line : ReadLines(text)) {
		if (IsNullOrWhiteSpace(line) || StartsWith(line, u";"))
			continue;

		if (StartsWith(line, u"[") && EndsWith(line, u"]") && line.size() >= 2) {
			current = std::make_shared<DocumentSection>();
			sections.Add(u16(line.substr(1, line.size() - 2)), current);
			continue;
		}

		// ^([\w ]+):\s*(.+)$
		size_t key_end = 0;
		while (key_end < line.size() && IsWordOrSpace(line[key_end])) ++key_end;
		if (key_end == 0 || key_end >= line.size() || line[key_end] != u':' || key_end + 1 >= line.size())
			Fail("Unrecognized line in .ass: " + Utf16ToUtf8(line));
		u16v rest = line.substr(key_end + 1);
		size_t ws = 0;
		while (ws < rest.size() && IsWhiteSpace(rest[ws])) ++ws;
		if (ws == rest.size()) --ws;
		u16v value = rest.substr(ws);

		if (!current)
			Fail("Line " + Utf16ToUtf8(line) + " is not inside a section");

		u16 type(line.substr(0, key_end));
		std::optional<size_t> max_items;
		if (current->format) max_items = current->format->Count();
		std::vector<u16> values = SplitString(value, u",", max_items);

		if (type == u"Format") {
			if (current->format)
				Fail("Section has multiple Format items");
			CsDict<u16, int> format;
			for (size_t i = 0; i < values.size(); ++i)
				format.Add(Trim(values[i]), static_cast<int>(i));
			current->format = std::move(format);
		}
		else {
			for (auto& v : values) v = TrimChar(v, u' ');
			current->items.push_back(DocumentItem{current.get(), std::move(type), std::move(values)});
		}
	}
	return sections;
}

// ---------------------------------------------------------------------------
// Karaoke types

class AssDocumentImpl;

struct KaraokeStepContext {
	AssDocumentImpl *document;
	LinePtr original_line;
	std::vector<std::pair<Ticks, int>> const *steps;
	LinePtr step_line;
	size_t step_index;
	int num_active_sections;
	std::vector<SectionPtr> singing_sections;
};

}

class KaraokeType {
public:
	virtual ~KaraokeType() = default;
	virtual std::vector<LinePtr> Apply(KaraokeStepContext& context) const = 0;
	/// line.KaraokeType.GetType() == typeof(SimpleKaraokeType)
	virtual bool IsExactlySimple() const { return false; }
};

namespace {

class SimpleKaraokeType : public KaraokeType {
public:
	std::vector<LinePtr> Apply(KaraokeStepContext& context) const override {
		for (auto const& section : context.singing_sections) {
			if (!section->current_word_fore_color.IsEmpty()) {
				section->fore_color = section->current_word_fore_color;
				RemoveAnimations(section->animations, [](Animation const& a) { return IsKind(a, AnimationKind::ForeColor); });
			}

			if (!section->current_word_shadow_color.IsEmpty()) {
				for (ShadowType shadow_type : section->shadow_colors.Keys()) {
					section->shadow_colors.Set(shadow_type, section->current_word_shadow_color);
					RemoveAnimations(section->animations, [&](Animation const& a) {
						return IsKind(a, AnimationKind::ShadowColor) && static_cast<ShadowColorAnimation const&>(a).shadow_type == shadow_type;
					});
				}
			}

			if (!section->current_word_outline_color.IsEmpty() && section->shadow_colors.ContainsKey(ShadowType::Glow)) {
				section->shadow_colors.Set(ShadowType::Glow, section->current_word_outline_color);
				RemoveAnimations(section->animations, [](Animation const& a) {
					return IsKind(a, AnimationKind::ShadowColor) && static_cast<ShadowColorAnimation const&>(a).shadow_type == ShadowType::Glow;
				});
			}
		}
		return {context.step_line};
	}
};

class ExactSimpleKaraokeType final : public SimpleKaraokeType {
public:
	bool IsExactlySimple() const override { return true; }
};

KaraokeTypePtr const& SimpleKaraokeInstance() {
	static KaraokeTypePtr instance = std::make_shared<ExactSimpleKaraokeType>();
	return instance;
}

Color GetOrDefault(CsDict<ShadowType, Color> const& dict, ShadowType key) {
	auto value = dict.TryGet(key);
	return value ? *value : Color();
}

class FadeKaraokeType final : public KaraokeType {
	static void ApplyFadeIn(Line& step_line, std::vector<SectionPtr> const& singing_sections) {
		Ticks fade_end = std::min(AddMs(step_line.start, 500), step_line.end);
		for (auto const& s : singing_sections) {
			if (s->current_word_fore_color.IsEmpty()) {
				if (!(s->fore_color == s->secondary_color))
					s->animations.push_back(std::make_shared<ForeColorAnimation>(step_line.start, s->secondary_color, fade_end, s->fore_color, 1.f));
			}
			else if (!(s->current_word_fore_color == s->secondary_color))
				s->animations.push_back(std::make_shared<ForeColorAnimation>(step_line.start, s->secondary_color, fade_end, s->current_word_fore_color, 1.f));

			if (!s->current_word_shadow_color.IsEmpty()) {
				for (auto const& [type, color] : s->shadow_colors.Items()) {
					if (!(s->current_word_shadow_color == color))
						s->animations.push_back(std::make_shared<ShadowColorAnimation>(type, step_line.start, color, fade_end, s->current_word_shadow_color, 1.f));
				}
			}

			if (!s->current_word_outline_color.IsEmpty() && !(s->current_word_outline_color == GetOrDefault(s->shadow_colors, ShadowType::Glow)))
				s->animations.push_back(std::make_shared<ShadowColorAnimation>(ShadowType::Glow, step_line.start, s->shadow_colors.At(ShadowType::Glow), fade_end, s->current_word_outline_color, 1.f));
		}
	}

	static void ApplyFadeOut(Line const& original_line, Line& step_line, std::vector<std::pair<Ticks, int>> const& steps, size_t step_idx) {
		int first_idx = 0;
		for (size_t prev = 0; prev < step_idx; ++prev) {
			Ticks fade_start = original_line.start + steps[prev + 1].first;
			Ticks fade_end = AddMs(fade_start, 1000);
			int last_idx = steps[prev].second - 1;
			for (int i = first_idx; i <= last_idx; ++i) {
				auto& s = *step_line.sections.at(i);
				if (!s.current_word_fore_color.IsEmpty() && !(s.current_word_fore_color == s.fore_color))
					s.animations.push_back(std::make_shared<ForeColorAnimation>(fade_start, s.current_word_fore_color, fade_end, s.fore_color, 1.f));

				if (!s.current_word_shadow_color.IsEmpty()) {
					for (auto const& [type, color] : s.shadow_colors.Items()) {
						if (!(s.current_word_shadow_color == color))
							s.animations.push_back(std::make_shared<ShadowColorAnimation>(type, fade_start, s.current_word_shadow_color, fade_end, color, 1.f));
					}
				}

				if (!s.current_word_outline_color.IsEmpty() && !(s.current_word_outline_color == GetOrDefault(s.shadow_colors, ShadowType::Glow)))
					s.animations.push_back(std::make_shared<ShadowColorAnimation>(ShadowType::Glow, fade_start, s.current_word_outline_color, fade_end, s.shadow_colors.At(ShadowType::Glow), 1.f));
			}
			first_idx = last_idx + 1;
		}
	}

public:
	std::vector<LinePtr> Apply(KaraokeStepContext& context) const override {
		ApplyFadeIn(*context.step_line, context.singing_sections);
		ApplyFadeOut(*context.original_line, *context.step_line, *context.steps, context.step_index);
		return {context.step_line};
	}
};

class GlitchKaraokeType final : public SimpleKaraokeType {
	static std::vector<CharacterRange> GetCharacterRanges(char16_t c) {
		static const std::vector<std::vector<CharacterRange>> language_ranges = {
			{{u'A', u'Z'}, {u'a', u'z'}},
			{{0x4E00, 0x9FCC}, {0x3400, 0x4DB6}, {0xF900, 0xFA6B}},
			{{0x3041, 0x3097}},
			{{0x30A0, 0x3100}},
			{{0xAC00, 0xD7A4}},
		};
		for (auto const& ranges : language_ranges) {
			for (auto const& r : ranges)
				if (r.Contains(c)) return ranges;
		}
		return {{0x2300, 0x231A}, {0x231C, 0x23E1}};
	}

public:
	std::vector<LinePtr> Apply(KaraokeStepContext& context) const override {
		SectionPtr singing;
		for (auto const& s : context.singing_sections)
			if (s->ruby_part == RubyPart::None || s->ruby_part == RubyPart::Base) singing = s;
		if (!singing || singing->text.empty())
			return {context.step_line};

		SimpleKaraokeType::Apply(context);
		Ticks glitch_end = FrameToEndTime(StartTimeToFrame(context.step_line->start) + 1);
		singing->animations.push_back(std::make_shared<GlitchingCharAnimation>(context.step_line->start, glitch_end, GetCharacterRanges(singing->text[0])));
		return {context.step_line};
	}
};

class CursorKaraokeType final : public SimpleKaraokeType {
	std::vector<u16> cursors;
	int interval_ms;
	bool before_singing;

	std::vector<SectionPtr> GenerateCursor(AssDocumentImpl& doc, Section const& initial_formatting, u16 const& cursor) const;

public:
	CursorKaraokeType(std::vector<u16> cursors, Ticks interval, bool before_singing)
	: cursors(std::move(cursors))
	, interval_ms(static_cast<int>(TotalMs(interval)))
	, before_singing(before_singing)
	{
	}

	std::vector<LinePtr> Apply(KaraokeStepContext& context) const override;
};

// ---------------------------------------------------------------------------
// Tag handling

struct TagContext {
	AssDocumentImpl *document = nullptr;
	AssStyle const *initial_style = nullptr;
	AssStyleOptions const *initial_style_options = nullptr;
	AssStyle const *style = nullptr;
	AssStyleOptions const *style_options = nullptr;
	LinePtr line;
	SectionPtr section;
	std::vector<std::function<std::vector<LinePtr>()>> post_processors;
};

using TagHandler = void (*)(TagContext& context, u16 const& arg);

struct TagHandlerInfo {
	bool affects_whole_line;
	TagHandler handler;
};

class AssDocumentImpl final : public SubtitleDocument {
	CsDict<u16, std::shared_ptr<AssStyle>> styles;
	std::optional<CsDict<u16, AssStyleOptions>> style_options;
	AssStyle const *default_style = nullptr;

	std::vector<LinePtr> ParseLine(AssDialogue const& dialogue, AssStyle const& style, AssStyleOptions const *options);
	static void CreateRubySections(Line& line);
	std::vector<LinePtr> ExpandLine(LinePtr const& line);
	std::vector<LinePtr> ExpandLineForKaraoke(LinePtr const& line);
	std::vector<LinePtr> CreateKaraokeStepLines(LinePtr const& original, std::vector<std::pair<Ticks, int>> const& steps, size_t step_idx);
	void EmulateKaraokeForSimultaneousLines();

public:
	AssDocumentImpl(u16v text, std::vector<AssStyleOptions> const& options);

	AssStyle const& DefaultStyle() const { return *default_style; }
	AssStyle const *GetStyle(u16 const& name) const {
		auto style = styles.TryGet(name);
		return style ? style->get() : nullptr;
	}
	AssStyleOptions const *GetStyleOptions(u16 const& name) const {
		return style_options ? style_options->TryGet(name) : nullptr;
	}

	void CreateTagSections(Line& line, u16 text, TagContext& context);
	void ApplyStyle(Section& section, AssStyle const& style, AssStyleOptions const *options) const;
	std::vector<LinePtr> CreateEmulatedKaraokeLines(LinePtr const& line);
};

// Argument parsing shared by the tag handlers (AssTagHandlerBase)

u16 RemoveChars(u16v arg, u16v chars) {
	u16 out;
	for (char16_t c : arg)
		if (chars.find(c) == u16v::npos) out.push_back(c);
	return out;
}

bool TagTryParseInt(u16v arg, int& value) {
	return TryParseInt(RemoveChars(arg, u"() "), value);
}

int TagParseInt(u16v arg) {
	int value = 0;
	TagTryParseInt(arg, value);
	return value;
}

bool TagTryParseFloat(u16v arg, float& value) {
	return TryParseFloat(RemoveChars(arg, u"() "), value);
}

int ParseHex(u16v arg) {
	uint32_t value = 0;
	if (!TryParseHex(RemoveChars(arg, u"&H()"), true, value))
		value = 0;
	return static_cast<int>(value);
}

/// Returns nullopt where the original returns null
std::optional<std::vector<u16>> ParseStringList(u16v arg) {
	if (IsNullOrWhiteSpace(arg))
		return std::vector<u16>();
	u16 trimmed = Trim(arg);
	if (!StartsWith(trimmed, u"("))
		return std::nullopt;
	auto items = SplitChar(RemoveChars(trimmed, u"()"), u',');
	for (auto& item : items) item = Trim(item);
	return items;
}

std::optional<std::vector<float>> ParseFloatList(u16v arg) {
	auto items = ParseStringList(arg);
	if (!items)
		return std::nullopt;
	std::vector<float> list;
	for (auto const& item : *items) {
		float value;
		if (!TryParseFloat(RemoveChars(item, u" "), value))
			value = 0;
		list.push_back(value);
	}
	return list;
}

Color ParseColor(u16v arg, int alpha) {
	int bgr = ParseHex(arg);
	return Color::FromArgb(alpha, bgr & 0xFF, (bgr >> 8) & 0xFF, (bgr >> 16) & 0xFF);
}

template<typename Pred>
void RemoveSectionAnims(TagContext& context, Pred pred) {
	RemoveAnimations(context.section->animations, pred);
}

auto KindIs(AnimationKind kind) {
	return [kind](Animation const& a) { return a.Kind() == kind; };
}

void HandleAlignment(TagContext& context, u16 const& arg) {
	int alignment = TagParseInt(arg);
	if (alignment >= 1 && alignment <= 9)
		context.line->anchor_point = GetAnchorPoint(alignment);
}

void HandleAlpha(TagContext& context, u16 const& arg) {
	int alpha = 255 - (ParseHex(arg) & 255);
	auto& s = *context.section;
	s.fore_color = ChangeAlpha(s.fore_color, alpha);
	s.secondary_color = ChangeAlpha(s.secondary_color, alpha);
	if (Req(context.style).HasOutlineBox())
		s.back_color = ChangeAlpha(s.back_color, alpha);
	for (ShadowType type : s.shadow_colors.Keys())
		s.shadow_colors.Set(type, ChangeAlpha(s.shadow_colors.At(type), alpha));
	RemoveSectionAnims(context, [](Animation const& a) { return a.IsColor(); });
}

void HandleToggle(bool& field, bool style_value, u16 const& arg) {
	if (arg.empty()) {
		field = style_value;
		return;
	}
	int value;
	if (!TagTryParseInt(arg, value))
		field = false;
	else if (value == 0)
		field = false;
	else if (value == 1)
		field = true;
	else
		field = style_value;
}

void HandleBold(TagContext& context, u16 const& arg) {
	// Only dereference the style when the original would
	bool style_value = arg.empty() || (TagParseInt(arg) != 0 && TagParseInt(arg) != 1 && [&] { int v; return TagTryParseInt(arg, v); }())
		? Req(context.initial_style).bold : false;
	HandleToggle(context.section->bold, style_value, arg);
}

void HandleItalic(TagContext& context, u16 const& arg) {
	bool style_value = arg.empty() || (TagParseInt(arg) != 0 && TagParseInt(arg) != 1 && [&] { int v; return TagTryParseInt(arg, v); }())
		? Req(context.initial_style).italic : false;
	HandleToggle(context.section->italic, style_value, arg);
}

void HandleUnderline(TagContext& context, u16 const& arg) {
	bool style_value = arg.empty() || (TagParseInt(arg) != 0 && TagParseInt(arg) != 1 && [&] { int v; return TagTryParseInt(arg, v); }())
		? Req(context.initial_style).underline : false;
	HandleToggle(context.section->underline, style_value, arg);
}

bool TryParseChromaArgs(u16 const& arg, std::vector<Color>& colors, int& alpha, int& max_offset_x, int& max_offset_y, int& chroma_in_ms, int& chroma_out_ms) {
	colors.clear();
	alpha = 128;
	max_offset_x = 20;
	max_offset_y = 0;
	chroma_in_ms = 270;
	chroma_out_ms = 270;

	auto args_opt = ParseStringList(arg);
	if (!args_opt)
		return false;
	auto const& args = *args_opt;
	size_t n = args.size();
	if (n >= 5)
		alpha = ParseHex(args[n - 5]) & 255;
	if (n >= 4 && !TryParseInt(args[n - 4], max_offset_x))
		return false;
	if (n >= 3 && !TryParseInt(args[n - 3], max_offset_y))
		return false;
	if (n >= 2 && !TryParseInt(args[n - 2], chroma_in_ms))
		return false;
	if (n >= 1 && !TryParseInt(args[n - 1], chroma_out_ms))
		return false;

	alpha = 255 - alpha;
	for (int i = 0; i < static_cast<int>(n) - 5; ++i)
		colors.push_back(ParseColor(args[i], alpha));
	return true;
}

std::vector<LinePtr> CreateChromaLines(Line const& original, std::vector<Color> const& colors, PointF center, int max_offset_x, int max_offset_y, int duration_ms, bool move_in) {
	std::vector<LinePtr> lines;
	if (original.sections.empty())
		return lines;

	for (size_t i = 0; i < colors.size(); ++i) {
		if (colors[i].A() == 0)
			continue;

		LinePtr chroma = original.Clone();
		for (auto& section : chroma->sections) {
			int alpha = static_cast<int>(section->fore_color.A() * (colors[i].A() / 255.0f));
			section->fore_color = Color::FromArgb(alpha, colors[i].R(), colors[i].G(), colors[i].B());
			section->back_color = Color();
			section->shadow_colors.Clear();
		}

		float offset_factor = colors.size() > 1 ? static_cast<float>(i) / static_cast<int>(colors.size() - 1) : 0.5f;
		float offset_x = offset_factor * (-max_offset_x * 2) + max_offset_x;
		float offset_y = offset_factor * (-max_offset_y * 2) + max_offset_y;
		PointF far_position{center.x + offset_x, center.y + offset_y};
		PointF near_position{center.x + offset_x / 5, center.y + offset_y / 5};

		if (move_in) {
			chroma->end = RoundTimeToFrameCenter(AddMs(original.start, duration_ms));
			chroma->animations.push_back(std::make_shared<MoveAnimation>(chroma->start, far_position, chroma->end, near_position));
		}
		else {
			chroma->start = RoundTimeToFrameCenter(AddMs(original.end, -duration_ms));
			chroma->animations.push_back(std::make_shared<MoveAnimation>(chroma->start, near_position, chroma->end, far_position));
		}
		lines.push_back(chroma);
	}
	return lines;
}

void HandleChroma(TagContext& context, u16 const& arg) {
	auto colors = std::make_shared<std::vector<Color>>();
	int alpha, max_offset_x, max_offset_y, chroma_in_ms, chroma_out_ms;
	if (!TryParseChromaArgs(arg, *colors, alpha, max_offset_x, max_offset_y, chroma_in_ms, chroma_out_ms))
		return;

	TagContext *ctx = &context;
	context.post_processors.push_back([=]() {
		Line& original = *ctx->line;
		PointF center = original.position ? *original.position : ctx->document->GetDefaultPosition(original.anchor_point);
		std::vector<LinePtr> chroma_lines;

		if (colors->empty()) {
			Color base = !original.sections.empty() ? original.sections[0]->fore_color : Color::FromArgb(0xFFFFFFFF);
			colors->push_back(Color::FromArgb(static_cast<int>(base.R() * alpha / 255.0f), 255, 0, 0));
			colors->push_back(Color::FromArgb(static_cast<int>(base.G() * alpha / 255.0f), 0, 255, 0));
			colors->push_back(Color::FromArgb(static_cast<int>(base.B() * alpha / 255.0f), 0, 0, 255));
		}

		if (chroma_in_ms > 0) {
			auto lines = CreateChromaLines(original, *colors, center, max_offset_x, max_offset_y, chroma_in_ms, true);
			chroma_lines.insert(chroma_lines.end(), lines.begin(), lines.end());
			original.start = RoundTimeToFrameCenter(AddMs(original.start, chroma_in_ms));
		}

		if (chroma_out_ms > 0) {
			auto lines = CreateChromaLines(original, *colors, center, max_offset_x, max_offset_y, chroma_out_ms, false);
			chroma_lines.insert(chroma_lines.end(), lines.begin(), lines.end());
			original.end = RoundTimeToFrameCenter(AddMs(original.end, -chroma_out_ms));
		}

		return chroma_lines;
	});
}

void HandleComplexFade(TagContext& context, u16 const& arg) {
	auto args = ParseFloatList(arg);
	if (!args || args->size() != 7)
		return;

	auto const& a = *args;
	Line& line = *context.line;
	int initial_alpha = 255 - static_cast<int>(a[0]);
	int mid_alpha = 255 - static_cast<int>(a[1]);
	int final_alpha = 255 - static_cast<int>(a[2]);
	Ticks fade_in_start = AddMs(line.start, a[3]);
	Ticks fade_in_end = AddMs(line.start, a[4]);
	Ticks fade_out_start = AddMs(line.start, a[5]);
	Ticks fade_out_end = AddMs(line.start, a[6]);

	if (fade_in_end > fade_in_start)
		line.animations.push_back(std::make_shared<FadeAnimation>(fade_in_start, initial_alpha, fade_in_end, mid_alpha));
	if (fade_out_end > fade_out_start)
		line.animations.push_back(std::make_shared<FadeAnimation>(fade_out_start, mid_alpha, fade_out_end, final_alpha));
}

void HandleFontSize(TagContext& context, u16 const& arg) {
	float line_height;
	if (!TagTryParseFloat(arg, line_height))
		line_height = Req(context.style).line_height;
	context.section->scale = line_height / context.document->DefaultStyle().line_height;
	RemoveSectionAnims(context, KindIs(AnimationKind::Scale));
}

void HandleFont(TagContext& context, u16 const& arg) {
	context.section->font = !arg.empty() ? arg : Req(context.style).font;
}

void HandleForeAlpha(TagContext& context, u16 const& arg) {
	int alpha = !arg.empty() ? 255 - (ParseHex(arg) & 255) : Req(context.style).primary_color.A();
	context.section->fore_color = ChangeAlpha(context.section->fore_color, alpha);
	RemoveSectionAnims(context, KindIs(AnimationKind::ForeColor));
}

void HandleForeColor(TagContext& context, u16 const& arg) {
	context.section->fore_color = !arg.empty() ? ParseColor(arg, context.section->fore_color.A()) : Req(context.style).primary_color;
	RemoveSectionAnims(context, KindIs(AnimationKind::ForeColor));
}

void HandleHorizontalTextDirection(TagContext& context, u16 const& arg) {
	int direction;
	if (!TryParseInt(arg, direction))
		direction = 6;
	context.line->horizontal_text_direction = direction == 4 ? HorizontalTextDirection::RightToLeft : HorizontalTextDirection::LeftToRight;
}

void HandleKaraoke(TagContext& context, u16 const& arg) {
	int centiseconds = 0;
	if (!TryParseInt(arg, centiseconds))
		centiseconds = 0;
	context.section->duration = Ms(centiseconds * 10.0);
}

KaraokeTypePtr CreateCursorKaraokeType(std::optional<std::vector<u16>> const& args, bool before_singing) {
	Ticks interval;
	std::vector<u16> cursors;
	if (!args || args->size() == 1) {
		interval = TicksPerHour;
		cursors.push_back(u"_");
	}
	else if (args->size() == 2) {
		interval = TicksPerHour;
		cursors.push_back((*args)[1]);
	}
	else if (args->size() == 3) {
		interval = TicksPerHour;
		cursors.push_back(u"{" + (*args)[1] + u"}" + (*args)[2]);
	}
	else {
		int interval_ms;
		if (!TryParseInt((*args)[1], interval_ms))
			interval_ms = std::numeric_limits<int>::max();
		interval = Ms(interval_ms);
		for (size_t i = 2; i < args->size(); i += 2)
			cursors.push_back(u"{" + (*args)[i] + u"}" + (i + 1 < args->size() ? (*args)[i + 1] : u16(u"_")));
	}
	return std::make_shared<CursorKaraokeType>(std::move(cursors), interval, before_singing);
}

void HandleKaraokeType(TagContext& context, u16 const& arg) {
	static const KaraokeTypePtr simple = std::make_shared<ExactSimpleKaraokeType>();
	static const KaraokeTypePtr fade = std::make_shared<FadeKaraokeType>();
	static const KaraokeTypePtr glitch = std::make_shared<GlitchKaraokeType>();

	auto args = ParseStringList(arg);
	u16 type_name = ToLower(args && !args->empty() ? (*args)[0] : arg);
	if (type_name == u"fade")
		context.line->karaoke_type = fade;
	else if (type_name == u"glitch")
		context.line->karaoke_type = glitch;
	else if (type_name == u"cursor")
		context.line->karaoke_type = CreateCursorKaraokeType(args, false);
	else if (type_name == u"lcursor")
		context.line->karaoke_type = CreateCursorKaraokeType(args, true);
	else
		context.line->karaoke_type = simple;
}

void HandleMove(TagContext& context, u16 const& arg) {
	auto args = ParseFloatList(arg);
	if (!args || args->size() < 4)
		return;

	auto const& a = *args;
	Line& line = *context.line;
	PointF start_pos{a[0], a[1]};
	PointF end_pos{a[2], a[3]};
	Ticks start_time = line.start;
	Ticks end_time = line.end;
	if (a.size() >= 6) {
		start_time = AddMs(line.start, a[4]);
		end_time = AddMs(line.start, a[5]);
	}
	if (end_time > start_time)
		line.animations.push_back(std::make_shared<MoveAnimation>(start_time, start_pos, end_time, end_pos));
}

void HandleOutlineAlpha(TagContext& context, u16 const& arg) {
	auto const& style = Req(context.style);
	if (!style.HasOutline())
		return;
	int alpha = !arg.empty() ? 255 - (ParseHex(arg) & 255) : style.outline_color.A();
	auto& s = *context.section;
	if (style.outline_is_box) {
		s.back_color = ChangeAlpha(s.back_color, alpha);
		RemoveSectionAnims(context, KindIs(AnimationKind::BackColor));
	}
	else {
		s.shadow_colors.Set(ShadowType::Glow, ChangeAlpha(s.shadow_colors.At(ShadowType::Glow), alpha));
		RemoveSectionAnims(context, KindIs(AnimationKind::ShadowColor));
	}
}

void HandleOutlineColor(TagContext& context, u16 const& arg) {
	auto const& style = Req(context.style);
	if (!style.HasOutline())
		return;
	auto& s = *context.section;
	if (style.outline_is_box) {
		s.back_color = !arg.empty() ? ParseColor(arg, s.back_color.A()) : style.outline_color;
		RemoveSectionAnims(context, KindIs(AnimationKind::BackColor));
	}
	else {
		s.shadow_colors.Set(ShadowType::Glow, !arg.empty() ? ParseColor(arg, s.shadow_colors.At(ShadowType::Glow).A()) : style.outline_color);
		RemoveSectionAnims(context, KindIs(AnimationKind::ShadowColor));
	}
}

void HandlePack(TagContext& context, u16 const& arg) {
	context.section->packed = arg != u"0";
}

void HandlePosition(TagContext& context, u16 const& arg) {
	auto coords = ParseFloatList(arg);
	if (!coords || coords->size() != 2 || context.line->position)
		return;
	context.line->position = PointF{(*coords)[0], (*coords)[1]};
}

void HandleRegularScript(TagContext& context, u16 const&) {
	context.section->offset = OffsetType::Regular;
}

void HandleReset(TagContext& context, u16 const& arg) {
	auto style = context.document->GetStyle(arg);
	context.style = style ? style : context.initial_style;
	auto options = context.document->GetStyleOptions(arg);
	context.style_options = options ? options : context.initial_style_options;
	context.document->ApplyStyle(*context.section, Req(context.style), context.style_options);
	context.section->offset = OffsetType::Regular;
	context.section->ruby_part = RubyPart::None;
	context.section->animations.clear();
}

void HandleRuby(TagContext& context, u16 const& arg) {
	int ruby_pos = 0;
	if (!TryParseInt(arg, ruby_pos))
		ruby_pos = 0;
	context.section->ruby_part = ruby_pos == 2 ? RubyPart::TextAfter : RubyPart::TextBefore;
}

void HandleSecondaryAlpha(TagContext& context, u16 const& arg) {
	int alpha = !arg.empty() ? 255 - (ParseHex(arg) & 255) : Req(context.style).secondary_color.A();
	context.section->secondary_color = ChangeAlpha(context.section->secondary_color, alpha);
	RemoveSectionAnims(context, KindIs(AnimationKind::SecondaryColor));
}

void HandleSecondaryColor(TagContext& context, u16 const& arg) {
	context.section->secondary_color = !arg.empty() ? ParseColor(arg, context.section->secondary_color.A()) : Req(context.style).secondary_color;
	RemoveSectionAnims(context, KindIs(AnimationKind::SecondaryColor));
}

void HandleShadowAlpha(TagContext& context, u16 const& arg) {
	auto const& style = Req(context.style);
	if (!style.HasShadow())
		return;
	int alpha = !arg.empty() ? 255 - (ParseHex(arg) & 255) : style.shadow_color.A();
	auto& s = *context.section;
	for (auto const& [type, color] : s.shadow_colors.Items()) {
		if (type != ShadowType::Glow || !style.HasOutline() || style.HasOutlineBox())
			s.shadow_colors.Set(type, ChangeAlpha(color, alpha));
	}
	RemoveSectionAnims(context, KindIs(AnimationKind::ShadowColor));
}

void HandleShadowColor(TagContext& context, u16 const& arg) {
	auto const& style = Req(context.style);
	if (!style.HasShadow())
		return;
	auto& s = *context.section;
	for (auto const& [type, color] : s.shadow_colors.Items()) {
		if (type != ShadowType::Glow || !style.HasOutline() || style.HasOutlineBox())
			s.shadow_colors.Set(type, !arg.empty() ? ParseColor(arg, color.A()) : style.shadow_color);
	}
	RemoveSectionAnims(context, KindIs(AnimationKind::ShadowColor));
}

void HandleShake(TagContext& context, u16 const& arg) {
	SizeF radius{20, 20};
	Ticks start_time = context.line->start;
	Ticks end_time = context.line->end;

	if (!IsNullOrWhiteSpace(arg)) {
		auto args = ParseFloatList(arg);
		if (!args)
			return;
		auto const& a = *args;
		switch (a.size()) {
			case 0:
				break;
			case 1:
				radius = {a[0], a[0]};
				break;
			case 2:
				radius = {a[0], a[1]};
				break;
			case 3:
				radius = {a[0], a[0]};
				start_time = AddMs(context.line->start, a[1]);
				end_time = AddMs(context.line->start, a[2]);
				break;
			case 4:
				radius = {a[0], a[1]};
				start_time = AddMs(context.line->start, a[2]);
				end_time = AddMs(context.line->start, a[3]);
				break;
			default:
				return;
		}
	}

	TagContext *ctx = &context;
	context.post_processors.push_back([=]() {
		ctx->line->animations.push_back(std::make_shared<ShakeAnimation>(start_time, end_time, radius));
		return std::vector<LinePtr>();
	});
}

void HandleSimpleFade(TagContext& context, u16 const& arg) {
	auto times = ParseFloatList(arg);
	if (!times || times->size() != 2)
		return;

	Line& line = *context.line;
	Ticks fade_in_start = line.start;
	Ticks fade_in_end = AddMs(line.start, (*times)[0]);
	Ticks fade_out_start = AddMs(line.end, -(*times)[1]);
	Ticks fade_out_end = line.end;

	if (fade_in_end > fade_in_start)
		line.animations.push_back(std::make_shared<FadeAnimation>(fade_in_start, 0, fade_in_end, 255));
	if (fade_out_end > fade_out_start)
		line.animations.push_back(std::make_shared<FadeAnimation>(fade_out_start, 255, fade_out_end, 0));
}

void HandleSubscript(TagContext& context, u16 const&) {
	context.section->offset = OffsetType::Subscript;
}

void HandleSuperscript(TagContext& context, u16 const&) {
	context.section->offset = OffsetType::Superscript;
}

void HandleVerticalType(TagContext& context, u16 const& arg) {
	int vert_type;
	if (!TryParseInt(arg, vert_type))
		vert_type = 9;
	auto& line = *context.line;
	switch (vert_type) {
		case 9: line.horizontal_text_direction = HorizontalTextDirection::RightToLeft; line.vertical_text_type = VerticalTextType::Positioned; break;
		case 7: line.horizontal_text_direction = HorizontalTextDirection::LeftToRight; line.vertical_text_type = VerticalTextType::Positioned; break;
		case 1: line.horizontal_text_direction = HorizontalTextDirection::LeftToRight; line.vertical_text_type = VerticalTextType::Rotated; break;
		case 3: line.horizontal_text_direction = HorizontalTextDirection::RightToLeft; line.vertical_text_type = VerticalTextType::Rotated; break;
		default: line.horizontal_text_direction = HorizontalTextDirection::LeftToRight; line.vertical_text_type = VerticalTextType::None; break;
	}
}

// \t (AssTransformTagHandler)

template<typename T, typename Pred, typename GetColor, typename Create>
T& FetchColorAnimation(TagContext& context, AnimationKind kind, Ticks start_time, Ticks end_time, Pred pred, GetColor get_section_color, Create create) {
	auto& anims = context.section->animations;
	RemoveAnimations(anims, [&](Animation const& a) { return a.Kind() == kind && a.start_time > start_time; });

	for (auto const& a : anims) {
		if (a->Kind() == kind && a->start_time == start_time && a->end_time == end_time && pred(static_cast<T&>(*a)))
			return static_cast<T&>(*a);
	}

	T *prev = nullptr;
	for (auto const& a : anims) {
		if (a->Kind() == kind && pred(static_cast<T&>(*a)))
			prev = static_cast<T *>(a.get());
	}
	auto anim = create(start_time, end_time, prev ? prev->end_color : get_section_color(*context.section));
	anims.push_back(anim);
	return *anim;
}

auto Always = [](auto const&) { return true; };

ForeColorAnimation& FetchForeColor(TagContext& context, Ticks start, Ticks end, float accel) {
	return FetchColorAnimation<ForeColorAnimation>(context, AnimationKind::ForeColor, start, end, Always,
		[](Section& s) { return s.fore_color; },
		[&](Ticks s, Ticks e, Color c) { return std::make_shared<ForeColorAnimation>(s, c, e, c, accel); });
}

SecondaryColorAnimation& FetchSecondaryColor(TagContext& context, Ticks start, Ticks end, float accel) {
	return FetchColorAnimation<SecondaryColorAnimation>(context, AnimationKind::SecondaryColor, start, end, Always,
		[](Section& s) { return s.secondary_color; },
		[&](Ticks s, Ticks e, Color c) { return std::make_shared<SecondaryColorAnimation>(s, c, e, c, accel); });
}

BackColorAnimation& FetchBackColor(TagContext& context, Ticks start, Ticks end, float accel) {
	return FetchColorAnimation<BackColorAnimation>(context, AnimationKind::BackColor, start, end, Always,
		[](Section& s) { return s.back_color; },
		[&](Ticks s, Ticks e, Color c) { return std::make_shared<BackColorAnimation>(s, c, e, c, accel); });
}

ShadowColorAnimation& FetchShadowColor(TagContext& context, ShadowType type, Ticks start, Ticks end, float accel) {
	return FetchColorAnimation<ShadowColorAnimation>(context, AnimationKind::ShadowColor, start, end,
		[type](ShadowColorAnimation const& a) { return a.shadow_type == type; },
		[type](Section& s) { return s.shadow_colors.At(type); },
		[&](Ticks s, Ticks e, Color c) { return std::make_shared<ShadowColorAnimation>(type, s, c, e, c, accel); });
}

using TransformHandler = void (*)(TagContext&, Ticks, Ticks, float, u16 const&);

void TransformForeColor(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchForeColor(c, s, e, accel);
	anim.end_color = ParseColor(arg, anim.end_color.A());
}

void TransformSecondaryColor(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchSecondaryColor(c, s, e, accel);
	anim.end_color = ParseColor(arg, anim.end_color.A());
}

void TransformShadowColorOf(TagContext& c, ShadowType type, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchShadowColor(c, type, s, e, accel);
	anim.end_color = ParseColor(arg, anim.end_color.A());
}

void TransformOutlineColor(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto const& style = Req(c.style);
	if (!style.HasOutline())
		return;
	if (style.outline_is_box) {
		auto& anim = FetchBackColor(c, s, e, accel);
		anim.end_color = ParseColor(arg, anim.end_color.A());
	}
	else
		TransformShadowColorOf(c, ShadowType::Glow, s, e, accel, arg);
}

void TransformShadowColor(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	for (ShadowType type : c.section->shadow_colors.Keys()) {
		auto const& style = Req(c.style);
		if (type != ShadowType::Glow || !style.HasOutline() || style.HasOutlineBox())
			TransformShadowColorOf(c, type, s, e, accel, arg);
	}
}

void TransformForeAlpha(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchForeColor(c, s, e, accel);
	anim.end_color = ChangeAlpha(anim.end_color, 255 - (ParseHex(arg) & 255));
}

void TransformSecondaryAlpha(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchSecondaryColor(c, s, e, accel);
	anim.end_color = ChangeAlpha(anim.end_color, 255 - (ParseHex(arg) & 255));
}

void TransformShadowAlphaOf(TagContext& c, ShadowType type, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto& anim = FetchShadowColor(c, type, s, e, accel);
	anim.end_color = ChangeAlpha(anim.end_color, 255 - (ParseHex(arg) & 255));
}

void TransformOutlineAlpha(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	auto const& style = Req(c.style);
	if (!style.HasOutline())
		return;
	if (style.outline_is_box) {
		auto& anim = FetchBackColor(c, s, e, accel);
		anim.end_color = ChangeAlpha(anim.end_color, 255 - (ParseHex(arg) & 255));
	}
	else
		TransformShadowAlphaOf(c, ShadowType::Glow, s, e, accel, arg);
}

void TransformShadowAlpha(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	for (ShadowType type : c.section->shadow_colors.Keys()) {
		auto const& style = Req(c.style);
		if (type != ShadowType::Glow || !style.HasOutline() || style.HasOutlineBox())
			TransformShadowAlphaOf(c, type, s, e, accel, arg);
	}
}

void TransformAlpha(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	TransformForeAlpha(c, s, e, accel, arg);
	TransformSecondaryAlpha(c, s, e, accel, arg);
	TransformOutlineAlpha(c, s, e, accel, arg);
	TransformShadowAlpha(c, s, e, accel, arg);
}

void TransformFontSize(TagContext& c, Ticks s, Ticks e, float accel, u16 const& arg) {
	float line_height;
	if (!TagTryParseFloat(arg, line_height))
		return;

	auto& anims = c.section->animations;
	RemoveAnimations(anims, [&](Animation const& a) { return a.Kind() == AnimationKind::Scale && a.start_time >= s; });
	ScaleAnimation const *prev = nullptr;
	for (auto const& a : anims)
		if (a->Kind() == AnimationKind::Scale) prev = static_cast<ScaleAnimation const *>(a.get());
	float start_scale = prev ? prev->end_scale : c.section->scale;
	float end_scale = line_height / c.document->DefaultStyle().line_height;
	anims.push_back(std::make_shared<ScaleAnimation>(s, start_scale, e, end_scale, accel));
}

TransformHandler GetTransformHandler(u16v tag) {
	if (tag == u"c" || tag == u"1c") return TransformForeColor;
	if (tag == u"2c") return TransformSecondaryColor;
	if (tag == u"3c") return TransformOutlineColor;
	if (tag == u"4c") return TransformShadowColor;
	if (tag == u"alpha") return TransformAlpha;
	if (tag == u"1a") return TransformForeAlpha;
	if (tag == u"2a") return TransformSecondaryAlpha;
	if (tag == u"3a") return TransformOutlineAlpha;
	if (tag == u"4a") return TransformShadowAlpha;
	if (tag == u"fs") return TransformFontSize;
	return nullptr;
}

bool IsAsciiLower(char16_t c) { return c >= u'a' && c <= u'z'; }

/// Matches `\d?[a-z]+` at pos, returning its end or npos
size_t MatchTagName(u16v text, size_t pos) {
	size_t p = pos;
	if (p < text.size() && IsAsciiDigit(text[p])) {
		size_t q = p + 1;
		size_t r = q;
		while (r < text.size() && IsAsciiLower(text[r])) ++r;
		if (r > q) return r;
	}
	size_t r = p;
	while (r < text.size() && IsAsciiLower(text[r])) ++r;
	return r > p ? r : u16v::npos;
}

void HandleTransform(TagContext& context, u16 const& arg) {
	Ticks start_time = context.line->start;
	Ticks end_time = context.line->end;
	float accel = 1;
	u16 modifiers;

	auto args_opt = ParseStringList(arg);
	if (!args_opt)
		return;
	auto const& args = *args_opt;
	switch (args.size()) {
		case 1:
			modifiers = args[0];
			break;
		case 2:
			if (!TagTryParseFloat(args[0], accel))
				return;
			modifiers = args[1];
			break;
		case 3: {
			int t1, t2;
			if (!TagTryParseInt(args[0], t1) || !TagTryParseInt(args[1], t2))
				return;
			start_time = AddMs(context.line->start, t1);
			end_time = AddMs(context.line->start, t2);
			modifiers = args[2];
			break;
		}
		case 4: {
			int t1, t2;
			if (!TagTryParseInt(args[0], t1) || !TagTryParseInt(args[1], t2) || !TagTryParseFloat(args[2], accel))
				return;
			start_time = AddMs(context.line->start, t1);
			end_time = AddMs(context.line->start, t2);
			modifiers = args[3];
			break;
		}
		default:
			return;
	}

	context.line->android_dark_text_hack_allowed = false;

	// \\(?<tag>\d?[a-z]+)(?<arg>[^\\]*)
	size_t pos = 0;
	while ((pos = modifiers.find(u'\\', pos)) != u16::npos) {
		size_t name_end = MatchTagName(modifiers, pos + 1);
		if (name_end == u16v::npos) {
			++pos;
			continue;
		}
		size_t arg_end = modifiers.find(u'\\', name_end);
		if (arg_end == u16::npos) arg_end = modifiers.size();
		u16v tag = u16v(modifiers).substr(pos + 1, name_end - pos - 1);
		if (auto handler = GetTransformHandler(tag))
			handler(context, start_time, end_time, accel, Trim(u16v(modifiers).substr(name_end, arg_end - name_end)));
		pos = arg_end;
	}
}

TagHandlerInfo const *GetTagHandler(u16v tag) {
	static const std::map<u16, TagHandlerInfo> handlers = {
		{u"an",      {true,  HandleAlignment}},
		{u"alpha",   {false, HandleAlpha}},
		{u"b",       {false, HandleBold}},
		{u"ytchroma",{true,  HandleChroma}},
		{u"fade",    {true,  HandleComplexFade}},
		{u"fs",      {false, HandleFontSize}},
		{u"fn",      {false, HandleFont}},
		{u"1a",      {false, HandleForeAlpha}},
		{u"c",       {false, HandleForeColor}},
		{u"1c",      {false, HandleForeColor}},
		{u"ytdir",   {true,  HandleHorizontalTextDirection}},
		{u"i",       {false, HandleItalic}},
		{u"k",       {false, HandleKaraoke}},
		{u"ytkt",    {true,  HandleKaraokeType}},
		{u"move",    {true,  HandleMove}},
		{u"3a",      {false, HandleOutlineAlpha}},
		{u"3c",      {false, HandleOutlineColor}},
		{u"ytpack",  {false, HandlePack}},
		{u"pos",     {true,  HandlePosition}},
		{u"ytsur",   {false, HandleRegularScript}},
		{u"r",       {false, HandleReset}},
		{u"ytruby",  {false, HandleRuby}},
		{u"2a",      {false, HandleSecondaryAlpha}},
		{u"2c",      {false, HandleSecondaryColor}},
		{u"4a",      {false, HandleShadowAlpha}},
		{u"4c",      {false, HandleShadowColor}},
		{u"ytshake", {true,  HandleShake}},
		{u"fad",     {true,  HandleSimpleFade}},
		{u"ytsub",   {false, HandleSubscript}},
		{u"ytsup",   {false, HandleSuperscript}},
		{u"t",       {false, HandleTransform}},
		{u"u",       {false, HandleUnderline}},
		{u"ytvert",  {true,  HandleVerticalType}},
	};
	auto it = handlers.find(u16(tag));
	return it == handlers.end() ? nullptr : &it->second;
}

u16 ResolveEscapeSequences(u16v text) {
	return ReplaceAll(ReplaceAll(ReplaceAll(text, u"\\h", u"\u00A0"), u"\\n", u"\u00A0"), u"\\N", u"\r\n");
}

/// Regex.IsMatch(text, @"\\k\s*\d+")
bool HasKaraokeTag(u16v text) {
	for (size_t pos = text.find(u"\\k"); pos != u16v::npos; pos = text.find(u"\\k", pos + 1)) {
		size_t p = pos + 2;
		while (p < text.size() && IsWhiteSpace(text[p])) ++p;
		if (p < text.size() && IsAsciiDigit(text[p]))
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// AssDocument

AssDocumentImpl::AssDocumentImpl(u16v text, std::vector<AssStyleOptions> const& options) {
	auto file_sections = ReadDocument(text);
	auto const& info = *file_sections.At(u"Script Info");
	video_dimensions = Size{info.GetItemInt(u"PlayResX", 384), info.GetItemInt(u"PlayResY", 288)};
	auto collisions = info.GetItemString(u"Collisions");
	line_merge_type = collisions && *collisions == u"Reverse" ? LineMergeType::MoveExisting : LineMergeType::MoveNew;

	for (auto const& item : file_sections.At(u"V4+ Styles")->items) {
		if (item.type != u"Style") continue;
		auto style = std::make_shared<AssStyle>(ParseStyle(item));
		styles.Set(style->name, style);
	}

	style_options.emplace();
	for (auto const& o : options) {
		if (o.name_is_null)
			Fail("Value cannot be null.\r\nParameter name: key");
		style_options->Set(o.name, o);
	}

	auto default_style_ptr = styles.TryGet(u"Default");
	default_style = default_style_ptr ? default_style_ptr->get() : styles.First().second.get();

	for (auto const& item : file_sections.At(u"Events")->items) {
		if (item.type != u"Dialogue") continue;
		AssDialogue dialogue(item);
		auto style = GetStyle(dialogue.style);
		if (!style)
			Fail("Line \"" + Utf16ToUtf8(dialogue.text) + "\" refers to style \"" + Utf16ToUtf8(dialogue.style) + "\" which doesn't exist.");
		auto parsed = ParseLine(dialogue, *style, GetStyleOptions(dialogue.style));
		for (auto const& line : parsed) {
			auto expanded = ExpandLine(line);
			lines.insert(lines.end(), expanded.begin(), expanded.end());
		}
	}

	EmulateKaraokeForSimultaneousLines();
	for (auto const& line : lines)
		line->NormalizeAlpha();
}

std::vector<LinePtr> AssDocumentImpl::ParseLine(AssDialogue const& dialogue, AssStyle const& style, AssStyleOptions const *options) {
	auto line = std::make_shared<Line>(RoundTimeToFrameCenter(dialogue.start), RoundTimeToFrameCenter(dialogue.end));
	line->layer = dialogue.layer;
	line->anchor_point = style.anchor_point;
	line->karaoke_type = SimpleKaraokeInstance();

	for (auto const& effect : SplitChar(dialogue.effect, u';')) {
		if (effect == u"no_android_dark_text_hack")
			line->android_dark_text_hack_allowed = false;
	}

	TagContext context;
	context.document = this;
	context.initial_style = &style;
	context.initial_style_options = options;
	context.style = &style;
	context.style_options = options;
	context.line = line;
	context.section = std::make_shared<Section>();

	ApplyStyle(*context.section, style, options);
	CreateTagSections(*line, dialogue.text, context);
	CreateRubySections(*line);

	std::vector<LinePtr> lines{line};
	for (auto const& post_processor : context.post_processors) {
		auto extra = post_processor();
		lines.insert(lines.end(), extra.begin(), extra.end());
	}
	return lines;
}

void AssDocumentImpl::CreateTagSections(Line& line, u16 text, TagContext& context) {
	while (EndsWith(text, u"\\N"))
		text.resize(text.size() - 2);

	std::set<u16> handled_whole_line_tags;
	size_t start = 0;

	// \{(.*?)\}
	size_t search = 0;
	while (search < text.size()) {
		size_t open = text.find(u'{', search);
		if (open == u16::npos) break;
		size_t close = open + 1;
		while (close < text.size() && text[close] != u'}' && text[close] != u'\n') ++close;
		if (close >= text.size() || text[close] != u'}') {
			search = open + 1;
			continue;
		}
		u16v group = u16v(text).substr(open + 1, close - open - 1);

		size_t end = open;
		if (end > start || (context.section->duration > 0 && HasKaraokeTag(group))) {
			if (end == start)
				context.section->text = ZeroWidthSpace;
			else
				context.section->text = ResolveEscapeSequences(u16v(text).substr(start, end - start));
			line.sections.push_back(context.section);
			context.section = context.section->Clone();
			context.section->text.clear();
			context.section->duration = 0;
		}

		// \\(?<tag>fn|r|\d?[a-z]+)\s*(?<arg>\([^\(\)]*(?:\)|$)|[^\\\(\)]*)
		size_t pos = 0;
		while ((pos = group.find(u'\\', pos)) != u16v::npos) {
			size_t p = pos + 1;
			size_t name_end;
			if (group.substr(p, 2) == u"fn")
				name_end = p + 2;
			else if (p < group.size() && group[p] == u'r')
				name_end = p + 1;
			else
				name_end = MatchTagName(group, p);
			if (name_end == u16v::npos) {
				++pos;
				continue;
			}
			u16v tag = group.substr(p, name_end - p);

			size_t q = name_end;
			while (q < group.size() && IsWhiteSpace(group[q])) ++q;
			size_t arg_start = q;
			size_t arg_end = q;
			bool matched_parens = false;
			if (q < group.size() && group[q] == u'(') {
				size_t r = q + 1;
				while (r < group.size() && group[r] != u'(' && group[r] != u')') ++r;
				if (r < group.size() && group[r] == u')') {
					arg_end = r + 1;
					matched_parens = true;
				}
				else if (r == group.size()) {
					arg_end = r;
					matched_parens = true;
				}
			}
			if (!matched_parens) {
				while (arg_end < group.size() && group[arg_end] != u'\\' && group[arg_end] != u'(' && group[arg_end] != u')') ++arg_end;
			}
			pos = arg_end;

			auto handler = GetTagHandler(tag);
			if (!handler)
				continue;
			if (handler->affects_whole_line && !handled_whole_line_tags.insert(u16(tag)).second)
				continue;
			handler->handler(context, Trim(group.substr(arg_start, arg_end - arg_start)));
		}

		start = close + 1;
		search = close + 1;
	}

	if (start < text.size()) {
		context.section->text = ResolveEscapeSequences(u16v(text).substr(start));
		line.sections.push_back(context.section);
	}
}

void AssDocumentImpl::ApplyStyle(Section& section, AssStyle const& style, AssStyleOptions const *options) const {
	section.font = style.font;
	section.scale = style.line_height / DefaultStyle().line_height;
	section.bold = style.bold;
	section.italic = style.italic;
	section.underline = style.underline;
	section.fore_color = style.primary_color;
	section.secondary_color = style.secondary_color;
	if (options && options->is_karaoke) {
		section.current_word_fore_color = options->current_word_text_color;
		section.current_word_outline_color = options->current_word_outline_color;
		section.current_word_shadow_color = options->current_word_shadow_color;
	}
	else {
		section.current_word_fore_color = Color();
		section.current_word_outline_color = Color();
		section.current_word_shadow_color = Color();
	}

	section.back_color = Color();
	section.shadow_colors.Clear();
	if (style.HasShadow()) {
		std::vector<ShadowType> types = options ? options->shadow_types : std::vector<ShadowType>{ShadowType::SoftShadow};
		for (ShadowType type : types)
			section.shadow_colors.Set(type, style.shadow_color);
	}

	if (style.HasOutline()) {
		if (style.outline_is_box)
			section.back_color = style.outline_color;
		else
			section.shadow_colors.Set(ShadowType::Glow, style.outline_color);
	}

	section.blur = 0;
}

/// \[(?<text>.+?)/(?<ruby>.+?)\] at pos
bool MatchRuby(u16v text, size_t pos, size_t& base_len, size_t& ruby_len, size_t& match_len) {
	if (text[pos] != u'[')
		return false;
	size_t base_start = pos + 1;
	for (size_t k = 1; base_start + k < text.size(); ++k) {
		if (text[base_start + k - 1] == u'\n')
			return false;
		if (text[base_start + k] != u'/')
			continue;
		size_t ruby_start = base_start + k + 1;
		for (size_t m = 1; ruby_start + m < text.size(); ++m) {
			if (text[ruby_start + m - 1] == u'\n')
				break;
			if (text[ruby_start + m] == u']') {
				base_len = k;
				ruby_len = m;
				match_len = ruby_start + m + 1 - pos;
				return true;
			}
		}
	}
	return false;
}

void InsertRubySection(Line& line, Section const& format, u16 text, RubyPart ruby_part, size_t index, size_t& num_sub_sections) {
	auto section = format.Clone();
	section->text = std::move(text);
	section->ruby_part = ruby_part;
	section->duration = 0;
	line.sections.insert(line.sections.begin() + static_cast<ptrdiff_t>(index + num_sub_sections), section);
	num_sub_sections++;
}

void AssDocumentImpl::CreateRubySections(Line& line) {
	for (ptrdiff_t idx = static_cast<ptrdiff_t>(line.sections.size()) - 1; idx >= 0; --idx) {
		SectionPtr section = line.sections[idx];
		if (section->ruby_part == RubyPart::None)
			continue;

		u16v text = section->text;
		struct Match { size_t index, base_len, ruby_len, length; };
		std::vector<Match> matches;
		for (size_t pos = 0; pos < text.size(); ) {
			Match m{pos, 0, 0, 0};
			if (MatchRuby(text, pos, m.base_len, m.ruby_len, m.length)) {
				matches.push_back(m);
				pos += m.length;
			}
			else
				++pos;
		}

		if (matches.empty()) {
			section->ruby_part = RubyPart::None;
			continue;
		}

		line.sections.erase(line.sections.begin() + idx);
		size_t inter_start = 0;
		size_t num_sub_sections = 0;
		size_t index = static_cast<size_t>(idx);
		for (auto const& m : matches) {
			if (m.index > inter_start)
				InsertRubySection(line, *section, u16(text.substr(inter_start, m.index - inter_start)), RubyPart::None, index, num_sub_sections);
			InsertRubySection(line, *section, u16(text.substr(m.index + 1, m.base_len)), RubyPart::Base, index, num_sub_sections);
			InsertRubySection(line, *section, u"(", RubyPart::Parenthesis, index, num_sub_sections);
			InsertRubySection(line, *section, u16(text.substr(m.index + 2 + m.base_len, m.ruby_len)), section->ruby_part, index, num_sub_sections);
			InsertRubySection(line, *section, u")", RubyPart::Parenthesis, index, num_sub_sections);
			inter_start = m.index + m.length;
		}
		if (inter_start < text.size())
			InsertRubySection(line, *section, u16(text.substr(inter_start)), RubyPart::None, index, num_sub_sections);

		line.sections[index]->duration = section->duration;
	}
}

// ---------------------------------------------------------------------------
// Animator

struct TimeRange {
	Ticks start;
	Ticks end;

	bool Contains(Ticks point) const { return point >= start && point < end; }
	bool Overlaps(TimeRange const& other) const { return start < other.end && end > other.start; }
	void IntersectWith(TimeRange const& other) {
		if (!Overlaps(other)) Fail("Can't intersect with a non-overlapping time range");
		start = std::max(start, other.start);
		end = std::min(end, other.end);
	}
	void UnionWith(TimeRange const& other) {
		if (!Overlaps(other)) Fail("Can't union with a non-overlapping time range");
		start = std::min(start, other.start);
		end = std::max(end, other.end);
	}
};

struct AnimationWithSectionIndex {
	AnimationPtr animation;
	int section_index;
};

using Cluster = std::pair<TimeRange, std::vector<AnimationWithSectionIndex>>;

std::vector<AnimationWithSectionIndex> GetAnimationsWithSectionIndex(Line const& line) {
	std::vector<AnimationWithSectionIndex> anims;
	for (auto const& a : line.animations)
		if (a->Kind() == AnimationKind::Move) anims.push_back({a, -1});
	for (auto const& a : line.animations)
		if (a->Kind() != AnimationKind::Move) anims.push_back({a, -1});
	for (size_t i = 0; i < line.sections.size(); ++i)
		for (auto const& a : line.sections[i]->animations)
			anims.push_back({a, static_cast<int>(i)});
	return anims;
}

std::vector<TimeRange> GetAnimationClusterTimeRanges(std::vector<AnimationWithSectionIndex> const& anims) {
	std::vector<TimeRange> ranges;
	for (auto const& a : anims) {
		TimeRange range{a.animation->start_time, a.animation->end_time};
		for (ptrdiff_t i = static_cast<ptrdiff_t>(ranges.size()) - 1; i >= 0; --i) {
			if (ranges[i].Overlaps(range)) {
				range.UnionWith(ranges[i]);
				ranges.erase(ranges.begin() + i);
			}
		}
		ranges.push_back(range);
	}
	return ranges;
}

/// SortedList<TimeRange, ...> keyed (and deduplicated) on the start time only
std::vector<Cluster> ClusterAnimations(Line const& line, std::vector<AnimationWithSectionIndex> const& all) {
	std::vector<Cluster> clusters;
	TimeRange line_range{line.start, line.end};
	for (TimeRange range : GetAnimationClusterTimeRanges(all)) {
		if (!range.Overlaps(line_range))
			continue;

		std::vector<AnimationWithSectionIndex> cluster_anims;
		for (auto const& a : all) {
			if (range.Contains(a.animation->start_time) && a.animation->end_time > a.animation->start_time)
				cluster_anims.push_back(a);
		}

		range.IntersectWith(line_range);
		range.start = RoundTimeToFrameCenter(range.start);
		range.end = RoundTimeToFrameCenter(range.end);

		auto it = std::lower_bound(clusters.begin(), clusters.end(), range.start,
			[](Cluster const& c, Ticks start) { return c.first.start < start; });
		if (it == clusters.end() || it->first.start != range.start)
			it = clusters.insert(it, Cluster{range, {}});
		it->second.insert(it->second.end(), cluster_anims.begin(), cluster_anims.end());
	}
	return clusters;
}

void ApplyAnimation(Line& line, AnimationWithSectionIndex const& a, float t) {
	Section *section = a.section_index >= 0 ? line.sections.at(static_cast<size_t>(a.section_index)).get() : nullptr;
	a.animation->Apply(line, section, t);
}

void ResetText(Line& frame_line, Line const& original) {
	for (size_t i = 0; i < frame_line.sections.size(); ++i)
		frame_line.sections[i]->text = original.sections.at(i)->text;
}

LinePtr CreateInitialLine(Line const& original, std::vector<AnimationWithSectionIndex> const& anims) {
	LinePtr line = original.Clone();
	line->start = RoundTimeToFrameCenter(line->start);

	std::vector<AnimationWithSectionIndex> before;
	for (auto const& a : anims)
		if (a.animation->end_time < original.start) before.push_back(a);
	std::stable_sort(before.begin(), before.end(), [](auto const& a, auto const& b) { return a.animation->end_time < b.animation->end_time; });
	for (auto const& a : before)
		ApplyAnimation(*line, a, 1);

	std::vector<AnimationWithSectionIndex> past;
	for (auto const& a : anims)
		if (a.animation->AffectsPast() && a.animation->start_time >= original.start) past.push_back(a);
	std::stable_sort(past.begin(), past.end(), [](auto const& a, auto const& b) { return a.animation->start_time > b.animation->start_time; });
	for (auto const& a : past)
		ApplyAnimation(*line, a, 0);

	return line;
}

LinePtr CreatePostAnimationClusterLine(Line const& original, Line const& last, Ticks start, Ticks end, std::vector<AnimationWithSectionIndex> anims) {
	LinePtr line = last.Clone();
	line->start = start;
	line->end = end;
	std::stable_sort(anims.begin(), anims.end(), [](auto const& a, auto const& b) { return a.animation->end_time < b.animation->end_time; });
	for (auto const& a : anims) {
		if (a.animation->AffectsText())
			ResetText(*line, original);
		ApplyAnimation(*line, a, 1);
	}
	return line;
}

template<typename Emit>
void CreateFrameLines(SubtitleDocument const& document, LinePtr const base, TimeRange range, std::vector<AnimationWithSectionIndex> const& anims, Emit emit) {
	int range_start_frame = StartTimeToFrame(range.start);
	int range_end_frame = EndTimeToFrame(range.end);
	constexpr int frame_step_size = 2;
	int sub_step_frames = (range_end_frame + 1 - range_start_frame) % frame_step_size;
	int last_iteration_frame = range_end_frame + 1 - sub_step_frames - frame_step_size;
	bool need_text_reset = std::any_of(anims.begin(), anims.end(), [](auto const& a) { return a.animation->AffectsText(); });

	LinePtr frame_line = base;
	PointF base_position = base->position ? *base->position : document.GetDefaultPosition(base->anchor_point);
	for (int frame = range_start_frame; frame <= last_iteration_frame; frame += frame_step_size) {
		frame_line = frame_line->Clone();
		frame_line->start = FrameToStartTime(frame);
		frame_line->end = frame < last_iteration_frame ? FrameToEndTime(frame + frame_step_size - 1) : range.end;
		frame_line->position = base_position;
		if (need_text_reset)
			ResetText(*frame_line, *base);

		float interp_frame = frame + (frame_step_size - 1) / 2.0f;
		for (auto const& a : anims) {
			int anim_start_frame = StartTimeToFrame(a.animation->start_time);
			int anim_end_frame = EndTimeToFrame(a.animation->end_time);
			if (interp_frame >= anim_start_frame && interp_frame < anim_end_frame) {
				float t = (interp_frame - anim_start_frame) / (anim_end_frame - anim_start_frame);
				ApplyAnimation(*frame_line, a, t);
			}
			else if (interp_frame >= anim_end_frame && interp_frame < anim_end_frame + frame_step_size) {
				ApplyAnimation(*frame_line, a, 1);
				if (a.animation->Kind() == AnimationKind::Move)
					base_position = *frame_line->position;
			}
		}

		emit(frame_line);
	}
}

std::vector<LinePtr> AnimatorExpand(SubtitleDocument const& document, LinePtr const& original) {
	auto anims = GetAnimationsWithSectionIndex(*original);
	if (anims.empty())
		return {original};

	auto clusters = ClusterAnimations(*original, anims);
	std::vector<LinePtr> result;
	LinePtr last = CreateInitialLine(*original, anims);
	if (clusters.empty() || clusters[0].first.start > last->start)
		result.push_back(last);

	for (size_t i = 0; i < clusters.size(); ++i) {
		TimeRange range = clusters[i].first;
		auto const& cluster_anims = clusters[i].second;
		CreateFrameLines(document, last, range, cluster_anims, [&](LinePtr const& frame_line) {
			last->end = frame_line->start;
			result.push_back(last = frame_line);
		});

		Ticks inter_start = range.end;
		Ticks inter_end = i < clusters.size() - 1 ? clusters[i + 1].first.start : original->end;
		if (inter_end > inter_start)
			result.push_back(last = CreatePostAnimationClusterLine(*original, *last, inter_start, inter_end, cluster_anims));
	}

	last->end = original->end;
	return result;
}

// ---------------------------------------------------------------------------
// Karaoke

std::vector<LinePtr> AssDocumentImpl::ExpandLine(LinePtr const& line) {
	std::vector<LinePtr> result;
	for (auto const& karaoke_line : ExpandLineForKaraoke(line)) {
		auto expanded = AnimatorExpand(*this, karaoke_line);
		result.insert(result.end(), expanded.begin(), expanded.end());
	}
	return result;
}

bool CanUseNativeKaraoke(Line const& line) {
	if (!line.karaoke_type->IsExactlySimple() || !line.animations.empty())
		return false;
	return std::all_of(line.sections.begin(), line.sections.end(), [](SectionPtr const& s) {
		return s->secondary_color.A() == 0
			&& s->current_word_fore_color.IsEmpty()
			&& s->current_word_outline_color.IsEmpty()
			&& s->current_word_shadow_color.IsEmpty()
			&& s->animations.empty()
			&& s->duration != 0;
	});
}

std::vector<LinePtr> AssDocumentImpl::ExpandLineForKaraoke(LinePtr const& line) {
	if (std::all_of(line->sections.begin(), line->sections.end(), [](SectionPtr const& s) { return s->duration == 0; }))
		return {line};

	if (CanUseNativeKaraoke(*line)) {
		Ticks offset = 0;
		for (auto const& section : line->sections) {
			section->start_offset = offset;
			offset += section->duration;
		}
		return {line};
	}

	return CreateEmulatedKaraokeLines(line);
}

std::vector<std::pair<Ticks, int>> GetKaraokeSteps(Line const& line) {
	std::map<Ticks, int> steps;
	Ticks offset = 0;
	int num_active = 0;
	for (auto const& section : line.sections) {
		num_active++;
		if (section->duration > 0) {
			steps[offset] = num_active;
			offset += section->duration;
		}
		else {
			Ticks prev_offset = !steps.empty() ? steps.rbegin()->first : 0;
			steps[prev_offset] = num_active;
		}
	}
	return {steps.begin(), steps.end()};
}

std::vector<LinePtr> AssDocumentImpl::CreateEmulatedKaraokeLines(LinePtr const& line) {
	auto steps = GetKaraokeSteps(*line);
	std::vector<LinePtr> result;
	for (size_t i = 0; i < steps.size(); ++i) {
		for (auto const& step_line : CreateKaraokeStepLines(line, steps, i)) {
			// Remove empty sections that were added in CreateTagSections()
			std::erase_if(step_line->sections, [](SectionPtr const& s) { return s->text == ZeroWidthSpace; });
			result.push_back(step_line);
		}
	}
	return result;
}

std::vector<LinePtr> AssDocumentImpl::CreateKaraokeStepLines(LinePtr const& original, std::vector<std::pair<Ticks, int>> const& steps, size_t step_idx) {
	Ticks time_offset = steps[step_idx].first;
	int num_active = steps[step_idx].second;
	Ticks start_time = RoundTimeToFrameCenter(original->start + time_offset);
	if (start_time >= original->end)
		return {};

	Ticks end_time;
	if (step_idx < steps.size() - 1) {
		end_time = RoundTimeToFrameCenter(original->start + steps[step_idx + 1].first);
		if (end_time > original->end)
			end_time = original->end;
	}
	else
		end_time = original->end;

	LinePtr step_line = original->Clone();
	step_line->start = start_time;
	step_line->end = end_time;

	for (size_t i = 0; i < step_line->sections.size(); ++i) {
		auto& section = *step_line->sections[i];
		section.duration = 0;
		section.start_offset = 0;
		if (static_cast<int>(i) < num_active) {
			RemoveAnimations(section.animations, KindIs(AnimationKind::SecondaryColor));
			continue;
		}

		section.fore_color = section.secondary_color;
		RemoveAnimations(section.animations, KindIs(AnimationKind::ForeColor));
		std::vector<AnimationPtr> secondary;
		for (auto const& a : section.animations)
			if (a->Kind() == AnimationKind::SecondaryColor) secondary.push_back(a);
		for (auto const& a : secondary) {
			std::erase(section.animations, a);
			auto const& anim = static_cast<SecondaryColorAnimation const&>(*a);
			section.animations.push_back(std::make_shared<ForeColorAnimation>(anim.start_time, anim.start_color, anim.end_time, anim.end_color, anim.acceleration));
		}

		bool has_fore_anim = std::any_of(section.animations.begin(), section.animations.end(), [](AnimationPtr const& a) { return a->Kind() == AnimationKind::ForeColor; });
		if (section.fore_color.A() == 0 && !has_fore_anim) {
			section.fore_color = Color::FromArgb(0u);
			section.back_color = Color::FromArgb(0u);
			section.shadow_colors.Clear();
		}
	}

	int prev_num_active = step_idx > 0 ? steps[step_idx - 1].second : 0;
	KaraokeStepContext context;
	context.document = this;
	context.original_line = original;
	context.steps = &steps;
	context.step_line = step_line;
	context.step_index = step_idx;
	context.num_active_sections = num_active;
	for (int i = prev_num_active; i < num_active && i < static_cast<int>(step_line->sections.size()); ++i)
		context.singing_sections.push_back(step_line->sections[i]);
	return step_line->karaoke_type->Apply(context);
}

void AssDocumentImpl::EmulateKaraokeForSimultaneousLines() {
	for (size_t i = 0; i < lines.size(); ++i) {
		LinePtr line = lines[i];
		if (line->position || !std::any_of(line->sections.begin(), line->sections.end(), [](SectionPtr const& s) { return s->start_offset > 0; }))
			continue;

		bool overlaps = std::any_of(lines.begin(), lines.end(), [&](LinePtr const& l) {
			return l->start < line->end && l->end > line->start && !l->position && l->anchor_point == line->anchor_point;
		});
		if (!overlaps)
			continue;

		lines.erase(lines.begin() + static_cast<ptrdiff_t>(i));
		auto emulated = CreateEmulatedKaraokeLines(line);
		lines.insert(lines.begin() + static_cast<ptrdiff_t>(i), emulated.begin(), emulated.end());
	}
}

std::vector<SectionPtr> CursorKaraokeType::GenerateCursor(AssDocumentImpl& doc, Section const& initial_formatting, u16 const& cursor) const {
	auto section = initial_formatting.Clone();
	section->text.clear();
	Line line(0, 0);

	TagContext context;
	context.document = &doc;
	context.line = std::shared_ptr<Line>(&line, [](Line *) { });
	context.section = section;
	doc.CreateTagSections(line, cursor, context);
	return line.sections;
}

std::vector<LinePtr> CursorKaraokeType::Apply(KaraokeStepContext& context) const {
	SimpleKaraokeType::Apply(context);

	auto const& step_line = *context.step_line;
	auto const& original = *context.original_line;
	int start_step = static_cast<int>(TotalMs(step_line.start - original.start)) / interval_ms;
	int end_step = static_cast<int>(TotalMs(step_line.end - original.start)) / interval_ms;

	std::vector<LinePtr> result;
	for (int step = start_step; step <= end_step; ++step) {
		LinePtr cursor_line = step_line.Clone();
		if (step > start_step)
			cursor_line->start = RoundTimeToFrameCenter(AddMs(original.start, static_cast<double>(step * interval_ms)));
		if (step < end_step)
			cursor_line->end = RoundTimeToFrameCenter(AddMs(original.start, static_cast<double>((step + 1) * interval_ms)));
		if (cursor_line->start == cursor_line->end)
			continue;

		int cursor_idx = before_singing ? context.num_active_sections - static_cast<int>(context.singing_sections.size()) : context.num_active_sections;
		auto const& initial = *cursor_line->sections.at(static_cast<size_t>(std::max(cursor_idx - 1, 0)));
		auto cursor_sections = GenerateCursor(*context.document, initial, cursors[static_cast<size_t>(step) % cursors.size()]);
		cursor_line->sections.insert(cursor_line->sections.begin() + cursor_idx, cursor_sections.begin(), cursor_sections.end());
		result.push_back(cursor_line);
	}
	return result;
}

}

std::unique_ptr<SubtitleDocument> LoadAssDocument(u16v text, std::vector<AssStyleOptions> const& style_options) {
	return std::make_unique<AssDocumentImpl>(text, style_options);
}

}
