// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ytt_internal.h
/// @brief Data model shared by the ASS reader and the YTT writer
/// @ingroup ytt
///
/// The goal of this port is to produce exactly the same output as the C#
/// original, so a few .NET behaviours are reproduced on purpose:
///  - strings are UTF-16 so that lengths, indices and character ranges match
///  - DateTime/TimeSpan are 100ns ticks, with .NET Framework millisecond rounding
///  - Dictionary keeps .NET's enumeration order, including after removals
///  - System.Random is reimplemented so seeded effects (shake, glitch) match
///  - lines, sections and animations are reference types (shared_ptr), since
///    the original mutates lines after handing them out

#pragma once

#include "ytt.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace ytt {

using u16 = std::u16string;
using u16v = std::u16string_view;

[[noreturn]] void Fail(std::string const& message);

// ---------------------------------------------------------------------------
// Time

/// Both DateTime (relative to SubtitleDocument.TimeBase) and TimeSpan
using Ticks = int64_t;
constexpr Ticks TicksPerMs = 10000;
constexpr Ticks TicksPerHour = TicksPerMs * 1000 * 3600;

inline double TotalMs(Ticks t) { return static_cast<double>(t) / TicksPerMs; }
/// DateTime.AddMilliseconds / TimeSpan.FromMilliseconds
Ticks Ms(double ms);
/// DateTime.AddMilliseconds
Ticks AddMs(Ticks t, double ms);

int StartTimeToFrame(Ticks time);
int EndTimeToFrame(Ticks time);
Ticks FrameToStartTime(int frame);
Ticks FrameToEndTime(int frame);
Ticks RoundTimeToFrameCenter(Ticks time);

// ---------------------------------------------------------------------------
// System.Drawing

/// System.Drawing.Color, including the distinction between Color.Empty and
/// transparent black that the original relies on
class Color {
	uint32_t argb = 0;
	bool empty = true;

public:
	Color() = default;
	static Color FromArgb(uint32_t argb);
	static Color FromArgb(int a, int r, int g, int b);
	static Color FromArgb(int a, Color base) { return FromArgb(a, base.R(), base.G(), base.B()); }

	int A() const { return argb >> 24; }
	int R() const { return (argb >> 16) & 0xFF; }
	int G() const { return (argb >> 8) & 0xFF; }
	int B() const { return argb & 0xFF; }
	int32_t ToArgb() const { return static_cast<int32_t>(argb); }
	bool IsEmpty() const { return empty; }

	bool operator==(Color const& o) const { return argb == o.argb && empty == o.empty; }
};

Color ChangeAlpha(Color color, int alpha);
bool IsDark(Color color);
Color Brighten(Color color);
Color ColorFromHtml(u16v html);
std::string ColorToHtml(Color color);

struct PointF {
	float x = 0, y = 0;
};

struct SizeF {
	float width = 0, height = 0;
};

struct Size {
	int width = 0, height = 0;
};

// ---------------------------------------------------------------------------
// System.Collections.Generic.Dictionary with .NET's enumeration order: items
// are enumerated by slot, and a freed slot is reused by the next insertion.

template<typename K, typename V>
class CsDict {
	struct Entry {
		K key;
		V value;
		bool used;
	};
	std::vector<Entry> entries;
	std::vector<size_t> free_list;
	size_t count = 0;

	Entry *Find(K const& key) {
		for (auto& e : entries)
			if (e.used && e.key == key) return &e;
		return nullptr;
	}
	Entry const *Find(K const& key) const { return const_cast<CsDict *>(this)->Find(key); }

public:
	CsDict() = default;
	/// Copying behaves like `new Dictionary(); dict.AddRange(other)`
	CsDict(CsDict const& other) { for (auto const& e : other.entries) if (e.used) Add(e.key, e.value); }
	CsDict& operator=(CsDict const& other) {
		if (this != &other) {
			Clear();
			for (auto const& e : other.entries) if (e.used) Add(e.key, e.value);
		}
		return *this;
	}

	size_t Count() const { return count; }
	bool ContainsKey(K const& key) const { return Find(key) != nullptr; }
	V const *TryGet(K const& key) const { auto e = Find(key); return e ? &e->value : nullptr; }

	V const& At(K const& key) const {
		if (auto e = Find(key)) return e->value;
		Fail("The given key was not present in the dictionary.");
	}

	/// dict[key] = value
	void Set(K const& key, V const& value) {
		if (auto e = Find(key))
			e->value = value;
		else
			Insert(key, value);
	}

	void Add(K const& key, V const& value) {
		if (Find(key)) Fail("An item with the same key has already been added.");
		Insert(key, value);
	}

	bool Remove(K const& key) {
		for (size_t i = 0; i < entries.size(); ++i) {
			if (entries[i].used && entries[i].key == key) {
				entries[i].used = false;
				free_list.push_back(i);
				--count;
				return true;
			}
		}
		return false;
	}

	void Clear() {
		entries.clear();
		free_list.clear();
		count = 0;
	}

	template<typename Pred>
	void RemoveAll(Pred pred) {
		std::vector<K> keys;
		for (auto const& e : entries)
			if (e.used && pred(e.key, e.value)) keys.push_back(e.key);
		for (auto const& key : keys) Remove(key);
	}

	/// Snapshot of the items in enumeration order
	std::vector<std::pair<K, V>> Items() const {
		std::vector<std::pair<K, V>> items;
		for (auto const& e : entries)
			if (e.used) items.emplace_back(e.key, e.value);
		return items;
	}

	std::vector<K> Keys() const {
		std::vector<K> keys;
		for (auto const& e : entries)
			if (e.used) keys.push_back(e.key);
		return keys;
	}

	std::pair<K, V> First() const {
		for (auto const& e : entries)
			if (e.used) return {e.key, e.value};
		Fail("Sequence contains no elements");
	}

private:
	void Insert(K const& key, V const& value) {
		if (!free_list.empty()) {
			size_t slot = free_list.back();
			free_list.pop_back();
			entries[slot] = Entry{key, value, true};
		}
		else
			entries.push_back(Entry{key, value, true});
		++count;
	}
};

// ---------------------------------------------------------------------------
// Enumerations

enum class LineMergeType { MoveNew, MoveExisting };

enum class AnchorPoint {
	TopLeft, TopCenter, TopRight,
	MiddleLeft, Center, MiddleRight,
	BottomLeft, BottomCenter, BottomRight
};

enum class ShadowType { Glow, Bevel, HardShadow, SoftShadow };
enum class OffsetType { Regular, Subscript, Superscript };
enum class RubyPart { None, Base, Parenthesis, TextBefore, TextAfter };
enum class HorizontalTextDirection { LeftToRight, RightToLeft };
enum class VerticalTextType { None, Positioned, Rotated };

bool IsLeftAligned(AnchorPoint a);
bool IsCenterAligned(AnchorPoint a);
bool IsRightAligned(AnchorPoint a);
bool IsTopAligned(AnchorPoint a);
bool IsMiddleAligned(AnchorPoint a);
bool IsBottomAligned(AnchorPoint a);

// ---------------------------------------------------------------------------
// System.Random (the seeded, Knuth subtractive variant)

class NetRandom {
	int inext = 0;
	int inextp = 21;
	int seed_array[56] = {};

	int InternalSample();

public:
	explicit NetRandom(int seed);
	double NextDouble() { return InternalSample() * (1.0 / 2147483647); }
	int Next(int max_value) { return static_cast<int>(NextDouble() * max_value); }
};

struct CharacterRange {
	char16_t start, end;
	bool Contains(char16_t c) const { return c >= start && c < end; }
};

bool IsRightToLeft(char16_t c);

// ---------------------------------------------------------------------------
// Lines, sections and animations

struct Line;
struct Section;
class Animation;
class KaraokeType;
using LinePtr = std::shared_ptr<Line>;
using SectionPtr = std::shared_ptr<Section>;
using AnimationPtr = std::shared_ptr<Animation>;
using KaraokeTypePtr = std::shared_ptr<KaraokeType const>;

/// Section and AssSection merged: the YTT writer never looks at the ASS-only
/// fields, so keeping them on every section changes nothing
struct Section {
	u16 text;
	std::optional<u16> font;
	float scale = 1;
	OffsetType offset = OffsetType::Regular;
	bool bold = false;
	bool italic = false;
	bool underline = false;
	Color fore_color;
	Color back_color;
	CsDict<ShadowType, Color> shadow_colors;
	RubyPart ruby_part = RubyPart::None;
	bool packed = false;
	Ticks start_offset = 0;

	Color secondary_color;
	float blur = 0;
	Color current_word_fore_color;
	Color current_word_outline_color;
	Color current_word_shadow_color;
	Ticks duration = 0;
	std::vector<AnimationPtr> animations;

	Section() = default;
	explicit Section(u16 text) : text(std::move(text)) { }

	SectionPtr Clone() const;
};

/// Line and AssLine merged
struct Line {
	Ticks start = 0;
	Ticks end = 0;
	std::vector<SectionPtr> sections;
	AnchorPoint anchor_point = AnchorPoint::BottomCenter;
	std::optional<PointF> position;
	HorizontalTextDirection horizontal_text_direction = HorizontalTextDirection::LeftToRight;
	VerticalTextType vertical_text_type = VerticalTextType::None;
	bool android_dark_text_hack_allowed = true;

	int layer = 0;
	int alpha = 255;
	std::vector<AnimationPtr> animations;
	KaraokeTypePtr karaoke_type;

	Line(Ticks start, Ticks end);

	u16 Text() const;
	LinePtr Clone() const;
	void NormalizeAlpha();
};

enum class AnimationKind {
	Move, Fade, Scale, Shake, GlitchingChar,
	ForeColor, BackColor, SecondaryColor, ShadowColor
};

class Animation {
public:
	Ticks start_time;
	Ticks end_time;
	float acceleration;

	Animation(Ticks start_time, Ticks end_time, float acceleration)
	: start_time(start_time), end_time(end_time), acceleration(acceleration) { }
	virtual ~Animation() = default;

	virtual AnimationKind Kind() const = 0;
	virtual bool AffectsPast() const = 0;
	virtual bool AffectsText() const { return false; }
	virtual void Apply(Line& line, Section *section, float t) = 0;
	virtual AnimationPtr Clone() const = 0;

	bool IsColor() const { return Kind() >= AnimationKind::ForeColor; }

protected:
	int Interpolate(int from, int to, float t) const;
	float Interpolate(float from, float to, float t) const;
	Color Interpolate(Color from, Color to, float t) const;
};

class ColorAnimation : public Animation {
public:
	Color start_color;
	Color end_color;

	ColorAnimation(Ticks start_time, Color start_color, Ticks end_time, Color end_color, float acceleration)
	: Animation(start_time, end_time, acceleration), start_color(start_color), end_color(end_color) { }

	bool AffectsPast() const override { return false; }

protected:
	Color GetColor(float t) const { return Interpolate(start_color, end_color, t); }
};

class ForeColorAnimation final : public ColorAnimation {
public:
	using ColorAnimation::ColorAnimation;
	AnimationKind Kind() const override { return AnimationKind::ForeColor; }
	void Apply(Line&, Section *section, float t) override { section->fore_color = GetColor(t); }
	AnimationPtr Clone() const override { return std::make_shared<ForeColorAnimation>(*this); }
};

class BackColorAnimation final : public ColorAnimation {
public:
	using ColorAnimation::ColorAnimation;
	AnimationKind Kind() const override { return AnimationKind::BackColor; }
	void Apply(Line&, Section *section, float t) override { section->back_color = GetColor(t); }
	AnimationPtr Clone() const override { return std::make_shared<BackColorAnimation>(*this); }
};

class SecondaryColorAnimation final : public ColorAnimation {
public:
	using ColorAnimation::ColorAnimation;
	AnimationKind Kind() const override { return AnimationKind::SecondaryColor; }
	void Apply(Line&, Section *section, float t) override { section->secondary_color = GetColor(t); }
	AnimationPtr Clone() const override { return std::make_shared<SecondaryColorAnimation>(*this); }
};

class ShadowColorAnimation final : public ColorAnimation {
public:
	ShadowType shadow_type;

	ShadowColorAnimation(ShadowType shadow_type, Ticks start_time, Color start_color, Ticks end_time, Color end_color, float acceleration)
	: ColorAnimation(start_time, start_color, end_time, end_color, acceleration), shadow_type(shadow_type) { }

	AnimationKind Kind() const override { return AnimationKind::ShadowColor; }
	void Apply(Line&, Section *section, float t) override { section->shadow_colors.Set(shadow_type, GetColor(t)); }
	AnimationPtr Clone() const override { return std::make_shared<ShadowColorAnimation>(*this); }
};

class FadeAnimation final : public Animation {
public:
	int start_alpha;
	int end_alpha;

	FadeAnimation(Ticks start_time, int start_alpha, Ticks end_time, int end_alpha)
	: Animation(start_time, end_time, 1), start_alpha(start_alpha), end_alpha(end_alpha) { }

	AnimationKind Kind() const override { return AnimationKind::Fade; }
	bool AffectsPast() const override { return true; }
	void Apply(Line& line, Section *, float t) override { line.alpha = Interpolate(start_alpha, end_alpha, t); }
	AnimationPtr Clone() const override { return std::make_shared<FadeAnimation>(*this); }
};

class MoveAnimation final : public Animation {
public:
	PointF start_position;
	PointF end_position;

	MoveAnimation(Ticks start_time, PointF start_pos, Ticks end_time, PointF end_pos)
	: Animation(start_time, end_time, 1), start_position(start_pos), end_position(end_pos) { }

	AnimationKind Kind() const override { return AnimationKind::Move; }
	bool AffectsPast() const override { return true; }
	void Apply(Line& line, Section *, float t) override;
	AnimationPtr Clone() const override { return std::make_shared<MoveAnimation>(*this); }
};

class ScaleAnimation final : public Animation {
public:
	float start_scale;
	float end_scale;

	ScaleAnimation(Ticks start_time, float start_scale, Ticks end_time, float end_scale, float acceleration)
	: Animation(start_time, end_time, acceleration), start_scale(start_scale), end_scale(end_scale) { }

	AnimationKind Kind() const override { return AnimationKind::Scale; }
	bool AffectsPast() const override { return false; }
	void Apply(Line&, Section *section, float t) override { section->scale = Interpolate(start_scale, end_scale, t); }
	AnimationPtr Clone() const override { return std::make_shared<ScaleAnimation>(start_time, start_scale, end_time, end_scale, acceleration); }
};

class ShakeAnimation final : public Animation {
	NetRandom random;

public:
	SizeF radius;

	ShakeAnimation(Ticks start_time, Ticks end_time, SizeF radius);

	AnimationKind Kind() const override { return AnimationKind::Shake; }
	bool AffectsPast() const override { return false; }
	void Apply(Line& line, Section *, float t) override;
	/// A clone starts over with a fresh random sequence, like the original
	AnimationPtr Clone() const override { return std::make_shared<ShakeAnimation>(start_time, end_time, radius); }
};

class GlitchingCharAnimation final : public Animation {
	NetRandom random;
	std::vector<CharacterRange> char_ranges;

public:
	GlitchingCharAnimation(Ticks start_time, Ticks end_time, std::vector<CharacterRange> char_ranges);

	AnimationKind Kind() const override { return AnimationKind::GlitchingChar; }
	bool AffectsPast() const override { return false; }
	bool AffectsText() const override { return true; }
	void Apply(Line& line, Section *section, float t) override;
	AnimationPtr Clone() const override { return std::make_shared<GlitchingCharAnimation>(start_time, end_time, char_ranges); }
};

/// List<Animation>.RemoveAll
template<typename Pred>
void RemoveAnimations(std::vector<AnimationPtr>& anims, Pred pred) {
	std::erase_if(anims, [&](AnimationPtr const& a) { return pred(*a); });
}

inline bool IsKind(Animation const& a, AnimationKind kind) { return a.Kind() == kind; }

// ---------------------------------------------------------------------------
// Text and number helpers mirroring the .NET BCL

u16 Utf8ToUtf16(std::string_view utf8);
std::string Utf16ToUtf8(u16v utf16);
/// StreamReader with byte order mark detection
u16 DecodeText(std::string_view bytes);
u16 Ascii(std::string_view ascii);

bool IsWhiteSpace(char16_t c);
u16 Trim(u16v s);
u16 TrimChar(u16v s, char16_t c);
bool IsNullOrWhiteSpace(u16v s);
bool StartsWith(u16v s, u16v prefix);
bool EndsWith(u16v s, u16v suffix);
bool Contains(u16v s, u16v needle);
u16 ReplaceAll(u16v s, u16v from, u16v to);
u16 ToLower(u16v s);
/// string.Split(char)
std::vector<u16> SplitChar(u16v s, char16_t separator);
/// YTSubConverter's StringExtensions.Split(string, int?)
std::vector<u16> SplitString(u16v s, u16v separator, std::optional<size_t> max_items);

/// int.TryParse(string) (NumberStyles.Integer)
bool TryParseInt(u16v s, int& value);
/// float.TryParse(string, NumberStyles.Float, InvariantCulture)
bool TryParseFloat(u16v s, float& value);
/// float.TryParse(string, NumberStyles.Any, InvariantCulture)
bool TryParseFloatAny(u16v s, float& value);
/// uint.TryParse(string, NumberStyles.AllowHexSpecifier [| AllowLeadingWhite | AllowTrailingWhite])
bool TryParseHex(u16v s, bool allow_white, uint32_t& value);

std::string IntToString(int64_t value);

/// (int) cast of a double as the x86/x64 .NET JIT does it: out of range
/// values and NaN become int.MinValue
int CsInt(double value);

// ---------------------------------------------------------------------------
// Style options (StyleOptions.xml)

struct AssStyleOptions {
	u16 name;
	/// The <Name> element was missing, which makes the original throw
	bool name_is_null = false;
	std::vector<ShadowType> shadow_types;
	bool is_karaoke = false;
	Color current_word_text_color;
	Color current_word_outline_color;
	Color current_word_shadow_color;
};

/// AssStyleOptionsList.LoadFromString. Returns an empty list on any error.
std::vector<AssStyleOptions> LoadStyleOptions(std::string_view xml);
/// The style options YTSubConverter ships in its resources
std::string_view DefaultStyleOptionsXml();

// ---------------------------------------------------------------------------
// Documents

class AssDocument;

class SubtitleDocument {
public:
	Size video_dimensions{1280, 720};
	LineMergeType line_merge_type = LineMergeType::MoveNew;
	std::vector<LinePtr> lines;

	PointF GetDefaultPosition(AnchorPoint anchor_point) const;

protected:
	SubtitleDocument() = default;
	explicit SubtitleDocument(SubtitleDocument const& doc, bool filter_empty_lines);

	void CloseGaps();
	void MergeSimultaneousLines();
	void MergeIdenticallyFormattedSections();
	static void MergeIdenticallyFormattedSections(Line& line);
	static void MoveLineBreaksToSeparateSections(Line& line);
};

/// Reads an .ass file the way YTSubConverter does, expanding karaoke,
/// animations and effects into plain timed lines
std::unique_ptr<SubtitleDocument> LoadAssDocument(u16v text, std::vector<AssStyleOptions> const& style_options);

/// Writes a document in YouTube's timed text format
std::string SaveYttDocument(SubtitleDocument const& doc);

}
