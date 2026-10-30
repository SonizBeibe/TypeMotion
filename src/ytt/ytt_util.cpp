// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ytt_util.cpp
/// @brief Model, time, color and .NET BCL helpers for the YTT converter
/// @ingroup ytt

#include "ytt_internal.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

#ifdef __ANDROID__
#include <cerrno>
#include <locale.h>
#endif

namespace ytt {

void Fail(std::string const& message) {
	throw ConversionError(message);
}

// ---------------------------------------------------------------------------
// Time

Ticks Ms(double ms) {
	if (std::isnan(ms) || std::fabs(ms) >= 315537897600000.0)
		Fail("Value to add was out of range.\r\nParameter name: value");
#ifdef YTT_NETCORE_TIME
	// .NET Core 3.0+ keeps sub-millisecond precision. Only used to test the
	// port against a YTSubConverter build that runs on .NET 8.
	double integral = std::trunc(ms);
	return static_cast<int64_t>(integral) * TicksPerMs + static_cast<int64_t>((ms - integral) * TicksPerMs);
#else
	// .NET Framework (which the YTSubConverter release used by TypeMotion
	// runs on) rounds to the nearest whole millisecond
	return static_cast<int64_t>(ms + (ms >= 0 ? 0.5 : -0.5)) * TicksPerMs;
#endif
}

Ticks AddMs(Ticks t, double ms) {
	// DateTime only covers the years 1 to 9999
	constexpr Ticks min_time = -630822816000000000;
	constexpr Ticks max_time = 3155378975999999999 - 630822816000000000;
	Ticks result = t + Ms(ms);
	if (result < min_time || result > max_time)
		Fail("The added or subtracted value results in an un-representable DateTime.\r\nParameter name: value");
	return result;
}

int EndTimeToFrame(Ticks time) {
	return static_cast<int>((TotalMs(time) + 1) / 33.36666666666667);
}

int StartTimeToFrame(Ticks time) {
	if (time <= 0)
		return 0;
	return EndTimeToFrame(time) + 1;
}

static Ticks FrameToTime(int frame) {
	if (frame == 0)
		return 0;
	int ms = static_cast<int>(frame * 33.36666666666667);
	return Ms(ms);
}

Ticks FrameToStartTime(int frame) {
	if (frame <= 0)
		return 0;
	return AddMs(FrameToTime(frame), -16);
}

Ticks FrameToEndTime(int frame) {
	return AddMs(FrameToTime(frame), 16);
}

Ticks RoundTimeToFrameCenter(Ticks time) {
	if (time <= 0)
		return 0;
	return FrameToStartTime(StartTimeToFrame(time));
}

// ---------------------------------------------------------------------------
// Color

Color Color::FromArgb(uint32_t argb) {
	Color c;
	c.argb = argb;
	c.empty = false;
	return c;
}

Color Color::FromArgb(int a, int r, int g, int b) {
	auto check = [](int v) {
		if (v < 0 || v > 255)
			Fail("Value of '" + std::to_string(v) + "' is not valid for 'red', 'green', 'blue' or 'alpha'. 'red', 'green', 'blue' and 'alpha' should be greater than or equal to 0 and less than or equal to 255.");
	};
	check(a); check(r); check(g); check(b);
	return FromArgb(static_cast<uint32_t>(a) << 24 | static_cast<uint32_t>(r) << 16 | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b));
}

Color ChangeAlpha(Color color, int alpha) {
	return Color::FromArgb(static_cast<uint32_t>(alpha) << 24 | (static_cast<uint32_t>(color.ToArgb()) & 0xFFFFFF));
}

bool IsDark(Color color) {
	return std::max({color.R(), color.G(), color.B()}) < 128;
}

Color Brighten(Color color) {
	int r = std::max(color.R(), 1);
	int g = std::max(color.G(), 1);
	int b = std::max(color.B(), 1);
	int highest = std::max({r, g, b});
	float factor = 255.f / highest;
	auto scale = [&](int v) { return static_cast<uint8_t>(std::nearbyint(static_cast<double>(v * factor))); };
	return Color::FromArgb(color.A(), scale(r), scale(g), scale(b));
}

Color ColorFromHtml(u16v html) {
	if (html.empty())
		return Color();
	if (html.size() != 7 || html[0] != u'#')
		Fail("Input string was not in a correct format.");
	uint32_t rgb;
	if (!TryParseHex(html.substr(1), false, rgb))
		Fail("Input string was not in a correct format.");
	return Color::FromArgb(0xFF000000u | rgb);
}

std::string ColorToHtml(Color color) {
	if (color.IsEmpty())
		return {};
	char buf[8];
	snprintf(buf, sizeof buf, "#%06X", static_cast<unsigned>(color.ToArgb()) & 0xFFFFFF);
	return buf;
}

// ---------------------------------------------------------------------------
// Anchor points

bool IsLeftAligned(AnchorPoint a) {
	return a == AnchorPoint::TopLeft || a == AnchorPoint::MiddleLeft || a == AnchorPoint::BottomLeft;
}

bool IsCenterAligned(AnchorPoint a) {
	return a == AnchorPoint::TopCenter || a == AnchorPoint::Center || a == AnchorPoint::BottomCenter;
}

bool IsRightAligned(AnchorPoint a) {
	return a == AnchorPoint::TopRight || a == AnchorPoint::MiddleRight || a == AnchorPoint::BottomRight;
}

bool IsTopAligned(AnchorPoint a) {
	return a == AnchorPoint::TopLeft || a == AnchorPoint::TopCenter || a == AnchorPoint::TopRight;
}

bool IsMiddleAligned(AnchorPoint a) {
	return a == AnchorPoint::MiddleLeft || a == AnchorPoint::Center || a == AnchorPoint::MiddleRight;
}

bool IsBottomAligned(AnchorPoint a) {
	return a == AnchorPoint::BottomLeft || a == AnchorPoint::BottomCenter || a == AnchorPoint::BottomRight;
}

// ---------------------------------------------------------------------------
// System.Random

NetRandom::NetRandom(int seed) {
	constexpr int MBIG = std::numeric_limits<int>::max();
	constexpr int MSEED = 161803398;

	int subtraction = seed == std::numeric_limits<int>::min() ? MBIG : std::abs(seed);
	int mj = MSEED - subtraction;
	seed_array[55] = mj;
	int mk = 1;
	for (int i = 1; i < 55; i++) {
		int ii = (21 * i) % 55;
		seed_array[ii] = mk;
		mk = mj - mk;
		if (mk < 0) mk += MBIG;
		mj = seed_array[ii];
	}
	for (int k = 1; k < 5; k++) {
		for (int i = 1; i < 56; i++) {
			seed_array[i] -= seed_array[1 + (i + 30) % 55];
			if (seed_array[i] < 0) seed_array[i] += MBIG;
		}
	}
	inext = 0;
	inextp = 21;
}

int NetRandom::InternalSample() {
	constexpr int MBIG = std::numeric_limits<int>::max();
	int loc_inext = inext;
	int loc_inextp = inextp;
	if (++loc_inext >= 56) loc_inext = 1;
	if (++loc_inextp >= 56) loc_inextp = 1;
	int ret = seed_array[loc_inext] - seed_array[loc_inextp];
	if (ret == MBIG) ret--;
	if (ret < 0) ret += MBIG;
	seed_array[loc_inext] = ret;
	inext = loc_inext;
	inextp = loc_inextp;
	return ret;
}

bool IsRightToLeft(char16_t c) {
	return CharacterRange{0x600, 0x6FF}.Contains(c) || CharacterRange{0x590, 0x5FF}.Contains(c);
}

// ---------------------------------------------------------------------------
// Sections, lines and animations

static std::vector<AnimationPtr> CloneAnimations(std::vector<AnimationPtr> const& anims) {
	std::vector<AnimationPtr> clones;
	clones.reserve(anims.size());
	for (auto const& a : anims)
		clones.push_back(a->Clone());
	return clones;
}

SectionPtr Section::Clone() const {
	auto clone = std::make_shared<Section>(*this);
	clone->animations = CloneAnimations(animations);
	return clone;
}

Line::Line(Ticks start, Ticks end) : start(start), end(end) { }

u16 Line::Text() const {
	u16 text;
	for (auto const& s : sections)
		text += s->text;
	return text;
}

LinePtr Line::Clone() const {
	auto clone = std::make_shared<Line>(*this);
	for (auto& section : clone->sections)
		section = section->Clone();
	clone->animations = CloneAnimations(animations);
	return clone;
}

void Line::NormalizeAlpha() {
	if (alpha == 255)
		return;

	float factor = alpha / 255.0f;
	auto multiply = [&](Color color) {
		if (color.IsEmpty())
			return color;
		return ChangeAlpha(color, static_cast<int>(color.A() * factor));
	};
	for (auto& section : sections) {
		section->fore_color = multiply(section->fore_color);
		section->back_color = multiply(section->back_color);
		for (auto const& [type, color] : section->shadow_colors.Items())
			section->shadow_colors.Set(type, multiply(color));
	}
	alpha = 255;
}

int Animation::Interpolate(int from, int to, float t) const {
	return from + static_cast<int>((to - from) * std::pow(static_cast<double>(t), static_cast<double>(acceleration)));
}

float Animation::Interpolate(float from, float to, float t) const {
	return from + (to - from) * static_cast<float>(std::pow(static_cast<double>(t), static_cast<double>(acceleration)));
}

Color Animation::Interpolate(Color from, Color to, float t) const {
	return Color::FromArgb(
		Interpolate(from.A(), to.A(), t),
		Interpolate(from.R(), to.R(), t),
		Interpolate(from.G(), to.G(), t),
		Interpolate(from.B(), to.B(), t));
}

void MoveAnimation::Apply(Line& line, Section *, float t) {
	float x = Interpolate(start_position.x, end_position.x, t);
	float y = Interpolate(start_position.y, end_position.y, t);
	line.position = PointF{x, y};
}

ShakeAnimation::ShakeAnimation(Ticks start_time, Ticks end_time, SizeF radius)
: Animation(start_time, end_time, 1)
, random(static_cast<int>(TotalMs(start_time)))
, radius(radius)
{
}

void ShakeAnimation::Apply(Line& line, Section *, float t) {
	if (t <= 0 || t >= 1 || !line.position)
		return;
	float dx = radius.width * (static_cast<float>(random.NextDouble()) * 2 - 1.0f);
	float dy = radius.height * (static_cast<float>(random.NextDouble()) * 2 - 1.0f);
	line.position = PointF{line.position->x + dx, line.position->y + dy};
}

GlitchingCharAnimation::GlitchingCharAnimation(Ticks start_time, Ticks end_time, std::vector<CharacterRange> char_ranges)
: Animation(start_time, end_time, 1)
, random(static_cast<int>(TotalMs(start_time)))
, char_ranges(std::move(char_ranges))
{
}

void GlitchingCharAnimation::Apply(Line&, Section *section, float t) {
	if (t > 0 && t < 1) {
		CharacterRange range = char_ranges[random.Next(static_cast<int>(char_ranges.size()))];
		section->text = u16(1, static_cast<char16_t>(range.start + random.Next(range.end - range.start)));
	}
}

// ---------------------------------------------------------------------------
// Text

u16 Utf8ToUtf16(std::string_view s) {
	// Invalid sequences become U+FFFD, one per maximal invalid subpart
	u16 out;
	out.reserve(s.size());
	size_t i = 0;
	auto byte = [&](size_t idx) { return static_cast<unsigned char>(s[idx]); };
	while (i < s.size()) {
		unsigned char c = byte(i);
		if (c < 0x80) {
			out.push_back(c);
			++i;
			continue;
		}

		int len = 0;
		uint32_t cp = 0;
		unsigned char lo = 0x80, hi = 0xBF;
		if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1F; }
		else if (c >= 0xE0 && c <= 0xEF) {
			len = 3; cp = c & 0x0F;
			if (c == 0xE0) lo = 0xA0;
			if (c == 0xED) hi = 0x9F;
		}
		else if (c >= 0xF0 && c <= 0xF4) {
			len = 4; cp = c & 0x07;
			if (c == 0xF0) lo = 0x90;
			if (c == 0xF4) hi = 0x8F;
		}
		else {
			out.push_back(0xFFFD);
			++i;
			continue;
		}

		size_t j = i + 1;
		bool ok = true;
		for (int k = 1; k < len; ++k, ++j) {
			if (j >= s.size()) { ok = false; break; }
			unsigned char cc = byte(j);
			unsigned char min = k == 1 ? lo : 0x80;
			unsigned char max = k == 1 ? hi : 0xBF;
			if (cc < min || cc > max) { ok = false; break; }
			cp = cp << 6 | (cc & 0x3F);
		}
		if (!ok) {
			out.push_back(0xFFFD);
			i = j;
			continue;
		}
		if (cp >= 0x10000) {
			cp -= 0x10000;
			out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
			out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
		}
		else
			out.push_back(static_cast<char16_t>(cp));
		i = j;
	}
	return out;
}

std::string Utf16ToUtf8(u16v s) {
	std::string out;
	out.reserve(s.size());
	for (size_t i = 0; i < s.size(); ++i) {
		uint32_t cp = s[i];
		if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < s.size() && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
			cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
			++i;
		}
		else if (cp >= 0xD800 && cp <= 0xDFFF)
			cp = 0xFFFD;

		if (cp < 0x80)
			out.push_back(static_cast<char>(cp));
		else if (cp < 0x800) {
			out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else if (cp < 0x10000) {
			out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
		else {
			out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
		}
	}
	return out;
}

static u16 DecodeUtf16(std::string_view s, bool big_endian) {
	u16 out;
	for (size_t i = 0; i + 1 < s.size(); i += 2) {
		auto b0 = static_cast<unsigned char>(s[i]), b1 = static_cast<unsigned char>(s[i + 1]);
		out.push_back(static_cast<char16_t>(big_endian ? b0 << 8 | b1 : b1 << 8 | b0));
	}
	if (s.size() % 2)
		out.push_back(0xFFFD);
	return out;
}

u16 DecodeText(std::string_view bytes) {
	auto starts = [&](std::string_view bom) { return bytes.substr(0, bom.size()) == bom; };
	if (starts("\xEF\xBB\xBF"))
		return Utf8ToUtf16(bytes.substr(3));
	if (starts(std::string_view("\xFF\xFE\0\0", 4)) || starts(std::string_view("\0\0\xFE\xFF", 4))) {
		bool big_endian = bytes[0] == '\0';
		u16 out;
		for (size_t i = 4; i + 3 < bytes.size(); i += 4) {
			auto b = [&](size_t k) { return static_cast<uint32_t>(static_cast<unsigned char>(bytes[i + k])); };
			uint32_t cp = big_endian ? b(0) << 24 | b(1) << 16 | b(2) << 8 | b(3) : b(3) << 24 | b(2) << 16 | b(1) << 8 | b(0);
			if (cp >= 0x10000 && cp <= 0x10FFFF) {
				cp -= 0x10000;
				out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
				out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
			}
			else
				out.push_back(cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF) ? 0xFFFD : static_cast<char16_t>(cp));
		}
		return out;
	}
	if (starts("\xFF\xFE"))
		return DecodeUtf16(bytes.substr(2), false);
	if (starts("\xFE\xFF"))
		return DecodeUtf16(bytes.substr(2), true);
	return Utf8ToUtf16(bytes);
}

u16 Ascii(std::string_view ascii) {
	return u16(ascii.begin(), ascii.end());
}

bool IsWhiteSpace(char16_t c) {
	return (c >= 0x09 && c <= 0x0D) || c == 0x20 || c == 0x85 || c == 0xA0 || c == 0x1680
		|| (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F
		|| c == 0x205F || c == 0x3000;
}

u16 Trim(u16v s) {
	size_t b = 0, e = s.size();
	while (b < e && IsWhiteSpace(s[b])) ++b;
	while (e > b && IsWhiteSpace(s[e - 1])) --e;
	return u16(s.substr(b, e - b));
}

u16 TrimChar(u16v s, char16_t c) {
	size_t b = 0, e = s.size();
	while (b < e && s[b] == c) ++b;
	while (e > b && s[e - 1] == c) --e;
	return u16(s.substr(b, e - b));
}

bool IsNullOrWhiteSpace(u16v s) {
	return std::all_of(s.begin(), s.end(), IsWhiteSpace);
}

bool StartsWith(u16v s, u16v prefix) {
	return s.substr(0, prefix.size()) == prefix;
}

bool EndsWith(u16v s, u16v suffix) {
	return s.size() >= suffix.size() && s.substr(s.size() - suffix.size()) == suffix;
}

bool Contains(u16v s, u16v needle) {
	return s.find(needle) != u16v::npos;
}

u16 ReplaceAll(u16v s, u16v from, u16v to) {
	u16 out;
	size_t pos = 0;
	while (true) {
		size_t next = s.find(from, pos);
		if (next == u16v::npos) break;
		out.append(s.substr(pos, next - pos));
		out.append(to);
		pos = next + from.size();
	}
	out.append(s.substr(pos));
	return out;
}

u16 ToLower(u16v s) {
	u16 out(s);
	for (auto& c : out) {
		if ((c >= u'A' && c <= u'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7))
			c += 0x20;
	}
	return out;
}

std::vector<u16> SplitChar(u16v s, char16_t separator) {
	std::vector<u16> parts;
	size_t start = 0;
	while (true) {
		size_t next = s.find(separator, start);
		if (next == u16v::npos) {
			parts.emplace_back(s.substr(start));
			return parts;
		}
		parts.emplace_back(s.substr(start, next - start));
		start = next + 1;
	}
}

std::vector<u16> SplitString(u16v s, u16v separator, std::optional<size_t> max_items) {
	std::vector<u16> result;
	size_t start = 0;
	while (start <= s.size()) {
		size_t end;
		if (start < s.size() && (!max_items || result.size() + 1 < *max_items)) {
			end = s.find(separator, start);
			if (end == u16v::npos)
				end = s.size();
		}
		else
			end = s.size();
		result.emplace_back(s.substr(start, end - start));
		start = end + separator.size();
	}
	return result;
}

// ---------------------------------------------------------------------------
// Numbers. .NET only accepts ASCII digits and treats \t\n\v\f\r and space as
// white space when parsing.

static bool IsNumberWhite(char16_t c) {
	return c == 0x20 || (c >= 0x09 && c <= 0x0D);
}

static u16v TrimNumberWhite(u16v s) {
	while (!s.empty() && IsNumberWhite(s.front())) s.remove_prefix(1);
	while (!s.empty() && IsNumberWhite(s.back())) s.remove_suffix(1);
	return s;
}

bool TryParseInt(u16v s, int& value) {
	s = TrimNumberWhite(s);
	bool negative = false;
	if (!s.empty() && (s[0] == u'-' || s[0] == u'+')) {
		negative = s[0] == u'-';
		s.remove_prefix(1);
	}
	if (s.empty())
		return false;
	int64_t result = 0;
	for (char16_t c : s) {
		if (c < u'0' || c > u'9')
			return false;
		result = result * 10 + (c - u'0');
		if (result > static_cast<int64_t>(std::numeric_limits<int>::max()) + 1)
			return false;
	}
	if (negative) result = -result;
	if (result > std::numeric_limits<int>::max() || result < std::numeric_limits<int>::min())
		return false;
	value = static_cast<int>(result);
	return true;
}

/// Shared implementation of float.TryParse for NumberStyles.Float and NumberStyles.Any
/// Rough decimal magnitude of an out of range number, to tell overflow from underflow
static long DecimalExponent(std::string const& digits) {
	size_t e = digits.find_first_of("eE");
	std::string mantissa = digits.substr(0, e);
	long exponent = 0;
	if (e != std::string::npos) {
		size_t i = e + 1;
		bool neg = i < digits.size() && digits[i] == '-';
		if (i < digits.size() && (digits[i] == '-' || digits[i] == '+')) ++i;
		for (; i < digits.size() && exponent < 100000; ++i)
			exponent = exponent * 10 + (digits[i] - '0');
		if (neg) exponent = -exponent;
	}
	size_t point = mantissa.find('.');
	std::string int_part = mantissa.substr(0, point);
	size_t first_nonzero = int_part.find_first_not_of('0');
	if (first_nonzero != std::string::npos)
		return exponent + static_cast<long>(int_part.size() - first_nonzero);
	if (point == std::string::npos)
		return exponent;
	std::string frac = mantissa.substr(point + 1);
	size_t frac_nonzero = frac.find_first_not_of('0');
	return exponent - static_cast<long>(frac_nonzero == std::string::npos ? 0 : frac_nonzero);
}

static bool TryParseFloatImpl(u16v s, bool any, float& value) {
	// The symbols are compared after string.Trim(), which removes more than number parsing does
	u16 symbol = Trim(s);
	if (symbol == u"Infinity") { value = std::numeric_limits<float>::infinity(); return true; }
	if (symbol == u"-Infinity") { value = -std::numeric_limits<float>::infinity(); return true; }
	if (symbol == u"NaN") { value = std::numeric_limits<float>::quiet_NaN(); return true; }

	s = TrimNumberWhite(s);
	bool negative = false;
	bool parens = false;
	auto skip_currency = [&] {
		if (any && !s.empty() && s.front() == 0xA4) {
			s.remove_prefix(1);
			s = TrimNumberWhite(s);
		}
	};

	skip_currency();
	if (any && !s.empty() && s.front() == u'(') {
		parens = true;
		s.remove_prefix(1);
	}
	skip_currency();
	if (!s.empty() && (s.front() == u'-' || s.front() == u'+')) {
		negative = s.front() == u'-';
		s.remove_prefix(1);
	}
	skip_currency();

	std::string digits;
	bool seen_digit = false;
	bool seen_point = false;
	size_t i = 0;
	for (; i < s.size(); ++i) {
		char16_t c = s[i];
		if (c >= u'0' && c <= u'9') {
			digits.push_back(static_cast<char>(c));
			seen_digit = true;
		}
		else if (c == u'.' && !seen_point) {
			digits.push_back('.');
			seen_point = true;
		}
		else if (any && c == u',' && seen_digit && !seen_point)
			continue;
		else
			break;
	}
	if (!seen_digit)
		return false;

	if (i < s.size() && (s[i] == u'e' || s[i] == u'E')) {
		size_t j = i + 1;
		std::string exponent = "e";
		if (j < s.size() && (s[j] == u'-' || s[j] == u'+'))
			exponent.push_back(static_cast<char>(s[j++]));
		size_t exp_digits = 0;
		while (j < s.size() && s[j] >= u'0' && s[j] <= u'9') {
			exponent.push_back(static_cast<char>(s[j++]));
			++exp_digits;
		}
		if (exp_digits > 0) {
			digits += exponent;
			i = j;
		}
	}

	u16v rest = TrimNumberWhite(s.substr(i));
	if (any) {
		if (!rest.empty() && rest.front() == 0xA4)
			rest = TrimNumberWhite(rest.substr(1));
		if (!negative && !rest.empty() && (rest.front() == u'-' || rest.front() == u'+')) {
			negative = rest.front() == u'-';
			rest = TrimNumberWhite(rest.substr(1));
		}
		if (parens) {
			if (rest.empty() || rest.front() != u')')
				return false;
			rest = TrimNumberWhite(rest.substr(1));
			negative = true;
		}
		if (!rest.empty() && rest.front() == 0xA4)
			rest = TrimNumberWhite(rest.substr(1));
	}
	if (!rest.empty())
		return false;

	// from_chars rather than strtod, as the latter depends on the C locale's
	// decimal separator
	double d = 0;
#ifdef __ANDROID__
	// The NDK's libc++ has no from_chars for floating point, so strtod_l with
	// the C locale (digits only has digits, a dot and an exponent here)
	static locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
	char *end_ptr = nullptr;
	errno = 0;
	d = strtod_l(digits.c_str(), &end_ptr, c_locale);
	const char *end = end_ptr;
	auto ec = errno == ERANGE ? std::errc::result_out_of_range : std::errc();
#else
	auto [end, ec] = std::from_chars(digits.data(), digits.data() + digits.size(), d, std::chars_format::general);
#endif
	if (ec == std::errc::result_out_of_range) {
		if (DecimalExponent(digits) > 0)
			return false;
		d = 0;
	}
	else if (ec != std::errc() || end != digits.data() + digits.size())
		return false;
	if (negative) d = -d;
	float f = static_cast<float>(d);
	if (std::isinf(f))
		return false;
	value = f;
	return true;
}

bool TryParseFloat(u16v s, float& value) {
	return TryParseFloatImpl(s, false, value);
}

bool TryParseFloatAny(u16v s, float& value) {
	return TryParseFloatImpl(s, true, value);
}

bool TryParseHex(u16v s, bool allow_white, uint32_t& value) {
	if (allow_white)
		s = TrimNumberWhite(s);
	if (s.empty())
		return false;
	uint64_t result = 0;
	for (char16_t c : s) {
		int digit;
		if (c >= u'0' && c <= u'9') digit = c - u'0';
		else if (c >= u'a' && c <= u'f') digit = c - u'a' + 10;
		else if (c >= u'A' && c <= u'F') digit = c - u'A' + 10;
		else return false;
		result = result << 4 | static_cast<uint64_t>(digit);
		if (result > 0xFFFFFFFFull)
			return false;
	}
	value = static_cast<uint32_t>(result);
	return true;
}

std::string IntToString(int64_t value) {
	return std::to_string(value);
}

int CsInt(double value) {
	if (!(value > -2147483649.0 && value < 2147483648.0))
		return std::numeric_limits<int>::min();
	return static_cast<int>(value);
}

// ---------------------------------------------------------------------------
// SubtitleDocument

SubtitleDocument::SubtitleDocument(SubtitleDocument const& doc, bool filter_empty_lines)
: video_dimensions(doc.video_dimensions)
, line_merge_type(doc.line_merge_type)
{
	for (auto const& line : doc.lines) {
		bool has_text = std::any_of(line->sections.begin(), line->sections.end(),
			[](SectionPtr const& s) { return !s->text.empty(); });
		if (!filter_empty_lines || has_text)
			lines.push_back(line);
	}
}

PointF SubtitleDocument::GetDefaultPosition(AnchorPoint anchor_point) const {
	float left = video_dimensions.width * 0.02f;
	float center = video_dimensions.width / 2.0f;
	float right = video_dimensions.width * 0.98f;

	float top = video_dimensions.height * 0.02f;
	float middle = video_dimensions.height / 2.0f;
	float bottom = video_dimensions.height * 0.98f;

	switch (anchor_point) {
		case AnchorPoint::TopLeft:      return {left, top};
		case AnchorPoint::TopCenter:    return {center, top};
		case AnchorPoint::TopRight:     return {right, top};
		case AnchorPoint::MiddleLeft:   return {left, middle};
		case AnchorPoint::Center:       return {center, middle};
		case AnchorPoint::MiddleRight:  return {right, middle};
		case AnchorPoint::BottomLeft:   return {left, bottom};
		case AnchorPoint::BottomCenter: return {center, bottom};
		case AnchorPoint::BottomRight:  return {right, bottom};
	}
	Fail("Specified argument was out of the range of valid values.");
}

}
