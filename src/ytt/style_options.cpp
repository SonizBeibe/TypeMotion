// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file style_options.cpp
/// @brief StyleOptions.xml loading and the conversion entry point
/// @ingroup ytt

#include "ytt_internal.h"

namespace ytt {
namespace {

/// Thrown for anything that would make XmlSerializer throw
struct XmlError { };

struct XmlElement {
	std::string name;
	std::string text;
	bool has_child_elements = false;
	std::vector<XmlElement> children;
	std::vector<std::pair<std::string, std::string>> attributes;
};

/// Just enough of an XML parser for the files XmlSerializer writes
class XmlParser {
	std::string_view s;
	size_t pos = 0;

	bool Eof() const { return pos >= s.size(); }
	char Peek() const { return Eof() ? '\0' : s[pos]; }
	bool Consume(std::string_view prefix) {
		if (s.substr(pos, prefix.size()) != prefix) return false;
		pos += prefix.size();
		return true;
	}
	void Expect(std::string_view prefix) { if (!Consume(prefix)) throw XmlError(); }
	static bool IsSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
	void SkipSpace() { while (!Eof() && IsSpace(s[pos])) ++pos; }
	static bool IsNameChar(char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
			|| c == '_' || c == ':' || c == '-' || c == '.' || static_cast<unsigned char>(c) >= 0x80;
	}

	void SkipUntil(std::string_view terminator) {
		size_t end = s.find(terminator, pos);
		if (end == std::string_view::npos) throw XmlError();
		pos = end + terminator.size();
	}

	/// Comments and processing instructions
	bool SkipMisc() {
		if (Consume("<!--")) { SkipUntil("-->"); return true; }
		if (s.substr(pos, 2) == "<?") { pos += 2; SkipUntil("?>"); return true; }
		return false;
	}

	std::string ReadName() {
		size_t start = pos;
		while (!Eof() && IsNameChar(s[pos])) ++pos;
		if (pos == start) throw XmlError();
		return std::string(s.substr(start, pos - start));
	}

	static void AppendUtf8(std::string& out, uint32_t cp) {
		if (cp == 0 || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) throw XmlError();
		if (cp < 0x80) out += static_cast<char>(cp);
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

	void ReadReference(std::string& out) {
		size_t end = s.find(';', pos);
		if (end == std::string_view::npos) throw XmlError();
		std::string_view ref = s.substr(pos, end - pos);
		pos = end + 1;
		if (ref == "lt") out += '<';
		else if (ref == "gt") out += '>';
		else if (ref == "amp") out += '&';
		else if (ref == "quot") out += '"';
		else if (ref == "apos") out += '\'';
		else if (ref.size() > 1 && ref[0] == '#') {
			bool hex = ref[1] == 'x';
			std::string_view digits = ref.substr(hex ? 2 : 1);
			if (digits.empty() || digits.size() > 8) throw XmlError();
			uint32_t cp = 0;
			for (char c : digits) {
				int d;
				if (c >= '0' && c <= '9') d = c - '0';
				else if (hex && c >= 'a' && c <= 'f') d = c - 'a' + 10;
				else if (hex && c >= 'A' && c <= 'F') d = c - 'A' + 10;
				else throw XmlError();
				cp = cp * (hex ? 16 : 10) + d;
			}
			AppendUtf8(out, cp);
		}
		else
			throw XmlError();
	}

	std::string ReadAttributeValue() {
		char quote = Peek();
		if (quote != '"' && quote != '\'') throw XmlError();
		++pos;
		std::string value;
		while (!Eof() && s[pos] != quote) {
			if (s[pos] == '<') throw XmlError();
			if (s[pos] == '&') { ++pos; ReadReference(value); }
			else value += s[pos++];
		}
		Expect(std::string_view(&quote, 1));
		return value;
	}

	XmlElement ReadElement() {
		Expect("<");
		XmlElement elem;
		elem.name = ReadName();
		for (;;) {
			bool had_space = IsSpace(Peek());
			SkipSpace();
			if (Consume("/>")) return elem;
			if (Consume(">")) break;
			if (!had_space) throw XmlError();
			std::string name = ReadName();
			SkipSpace();
			Expect("=");
			SkipSpace();
			elem.attributes.emplace_back(std::move(name), ReadAttributeValue());
		}

		for (;;) {
			if (Eof()) throw XmlError();
			if (Consume("</")) {
				if (ReadName() != elem.name) throw XmlError();
				SkipSpace();
				Expect(">");
				return elem;
			}
			if (Consume("<![CDATA[")) {
				size_t end = s.find("]]>", pos);
				if (end == std::string_view::npos) throw XmlError();
				elem.text += s.substr(pos, end - pos);
				pos = end + 3;
			}
			else if (SkipMisc())
				;
			else if (Peek() == '<') {
				elem.children.push_back(ReadElement());
				elem.has_child_elements = true;
			}
			else if (Peek() == '&') {
				++pos;
				ReadReference(elem.text);
			}
			else if (Peek() == '\r') {
				// Line breaks are normalized to \n
				++pos;
				Consume("\n");
				elem.text += '\n';
			}
			else
				elem.text += s[pos++];
		}
	}

public:
	explicit XmlParser(std::string_view s) : s(s) { }

	XmlElement ReadDocument() {
		Consume("\xEF\xBB\xBF");
		for (;;) {
			SkipSpace();
			if (!SkipMisc()) break;
		}
		if (s.substr(pos, 2) == "<!") throw XmlError();
		XmlElement root = ReadElement();
		for (;;) {
			SkipSpace();
			if (!SkipMisc()) break;
		}
		if (!Eof()) throw XmlError();
		return root;
	}
};

bool HasDefaultNamespace(XmlElement const& elem) {
	for (auto const& [name, value] : elem.attributes)
		if (name == "xmlns" && !value.empty()) return true;
	return false;
}

std::string const& ElementString(XmlElement const& elem) {
	if (elem.has_child_elements) throw XmlError();
	return elem.text;
}

/// Flags enum deserialization of AssStyleOptions.XmlShadowTypes
std::vector<ShadowType> ParseShadowTypes(std::string const& text) {
	enum { Glow = 1, HardShadow = 2, SoftShadow = 4, Bevel = 8 };
	int value = 0;
	size_t pos = 0;
	while (pos < text.size()) {
		while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n')) ++pos;
		size_t end = pos;
		while (end < text.size() && !(text[end] == ' ' || text[end] == '\t' || text[end] == '\r' || text[end] == '\n')) ++end;
		if (end == pos) break;
		std::string token = text.substr(pos, end - pos);
		if (token == "None") ;
		else if (token == "Glow") value |= Glow;
		else if (token == "HardShadow") value |= HardShadow;
		else if (token == "SoftShadow") value |= SoftShadow;
		else if (token == "Bevel") value |= Bevel;
		else throw XmlError();
		pos = end;
	}

	std::vector<ShadowType> types;
	if (value & Glow) types.push_back(ShadowType::Glow);
	if (value & Bevel) types.push_back(ShadowType::Bevel);
	if (value & SoftShadow) types.push_back(ShadowType::SoftShadow);
	if (value & HardShadow) types.push_back(ShadowType::HardShadow);
	return types;
}

/// XmlConvert.ToBoolean
bool ParseBoolean(std::string const& text) {
	u16 trimmed = Trim(Utf8ToUtf16(text));
	if (trimmed == u"true" || trimmed == u"1") return true;
	if (trimmed == u"false" || trimmed == u"0") return false;
	throw XmlError();
}

/// ColorUtil.FromHtml
Color ParseHtmlColor(std::string const& text) {
	if (text.empty())
		return Color();
	u16 html = Utf8ToUtf16(text);
	uint32_t rgb;
	if (html.size() != 7 || html[0] != u'#' || !TryParseHex(u16v(html).substr(1), false, rgb))
		throw XmlError();
	return Color::FromArgb(0xFF000000u | rgb);
}

AssStyleOptions ReadStyle(XmlElement const& elem) {
	AssStyleOptions options;
	options.name_is_null = true;
	for (auto const& child : elem.children) {
		if (child.name == "Name") {
			options.name = Utf8ToUtf16(ElementString(child));
			options.name_is_null = false;
		}
		else if (child.name == "ShadowType")
			options.shadow_types = ParseShadowTypes(ElementString(child));
		else if (child.name == "IsKaraoke")
			options.is_karaoke = ParseBoolean(ElementString(child));
		else if (child.name == "CurrentWordTextColor")
			options.current_word_text_color = ParseHtmlColor(ElementString(child));
		else if (child.name == "CurrentWordOutlineColor")
			options.current_word_outline_color = ParseHtmlColor(ElementString(child));
		else if (child.name == "CurrentWordShadowColor")
			options.current_word_shadow_color = ParseHtmlColor(ElementString(child));
	}
	return options;
}

constexpr char DefaultStyleOptions[] = R"xml(<?xml version="1.0"?>
<StyleOptions xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <Style>
    <Name>YTPlain</Name>
    <ShadowType>None</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTPlainBox</Name>
    <ShadowType>None</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTGlow</Name>
    <ShadowType>Glow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTGlowBox</Name>
    <ShadowType>Glow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTSoftShadow</Name>
    <ShadowType>SoftShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTSoftShadowBox</Name>
    <ShadowType>SoftShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTHardShadow</Name>
    <ShadowType>HardShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTHardShadowBox</Name>
    <ShadowType>HardShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTBevel</Name>
    <ShadowType>Bevel</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>YTBevelBox</Name>
    <ShadowType>Bevel</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>__ytplain</Name>
    <ShadowType>None</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>__ytglow</Name>
    <ShadowType>Glow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>__ytsoftshadow</Name>
    <ShadowType>SoftShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>__ythardshadow</Name>
    <ShadowType>HardShadow</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
  <Style>
    <Name>__ytbevel</Name>
    <ShadowType>Bevel</ShadowType>
    <IsKaraoke>false</IsKaraoke>
    <CurrentWordTextColor />
    <CurrentWordOutlineColor />
    <CurrentWordShadowColor />
  </Style>
</StyleOptions>)xml";

}

std::vector<AssStyleOptions> LoadStyleOptions(std::string_view xml) {
	try {
		XmlElement root = XmlParser(xml).ReadDocument();
		if (root.name != "StyleOptions" || HasDefaultNamespace(root))
			return {};

		std::vector<AssStyleOptions> list;
		for (auto const& child : root.children) {
			if (child.name == "Style" && !HasDefaultNamespace(child))
				list.push_back(ReadStyle(child));
		}
		return list;
	}
	catch (XmlError const&) {
		return {};
	}
	catch (ConversionError const&) {
		return {};
	}
}

std::string_view DefaultStyleOptionsXml() {
	return DefaultStyleOptions;
}

std::string ConvertAssToYtt(std::string_view ass, std::string_view user_style_options) {
	try {
		// User options first, so that the built-in ones take precedence like in YTSubConverter
		std::vector<AssStyleOptions> options = LoadStyleOptions(user_style_options);
		auto defaults = LoadStyleOptions(DefaultStyleOptionsXml());
		options.insert(options.end(), defaults.begin(), defaults.end());

		auto document = LoadAssDocument(DecodeText(ass), options);
		return SaveYttDocument(*document);
	}
	catch (ConversionError const&) {
		throw;
	}
	catch (std::exception const& e) {
		throw ConversionError(e.what());
	}
}

}
