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

/// @file json_io.cpp
/// @brief Reading and writing the JSON the app and the engine exchange
/// @ingroup android

#include "json_io.h"

#include <libaegisub/cajun/visitor.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {
	class TypeVisitor final : public json::ConstVisitor {
	public:
		json_io::Type type = json_io::Type::Null;

		void Visit(json::Array const&) override { type = json_io::Type::Array; }
		void Visit(json::Object const&) override { type = json_io::Type::Object; }
		void Visit(int64_t) override { type = json_io::Type::Int; }
		void Visit(double) override { type = json_io::Type::Double; }
		void Visit(json::String const&) override { type = json_io::Type::String; }
		void Visit(bool) override { type = json_io::Type::Bool; }
		void Visit(json::Null const&) override { type = json_io::Type::Null; }
	};

	class Writer final : public json::ConstVisitor {
		std::string& out;

	public:
		Writer(std::string& out) : out(out) { }

		void Visit(json::Array const& array) override {
			out += '[';
			bool first = true;
			for (auto const& value : array) {
				if (!first) out += ',';
				first = false;
				value.Accept(*this);
			}
			out += ']';
		}

		void Visit(json::Object const& object) override {
			out += '{';
			bool first = true;
			for (auto const& entry : object) {
				if (!first) out += ',';
				first = false;
				Visit(entry.first);
				out += ':';
				entry.second.Accept(*this);
			}
			out += '}';
		}

		void Visit(int64_t number) override {
			out += std::to_string(number);
		}

		void Visit(double number) override {
			if (!std::isfinite(number)) {
				out += "null";
				return;
			}
			char buf[32];
			snprintf(buf, sizeof buf, "%.17g", number);
			out += buf;
		}

		void Visit(json::String const& str) override {
			out += '"';
			for (char c : str) {
				switch (c) {
					case '"':  out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\b': out += "\\b";  break;
					case '\f': out += "\\f";  break;
					case '\n': out += "\\n";  break;
					case '\r': out += "\\r";  break;
					case '\t': out += "\\t";  break;
					default:
						if (static_cast<unsigned char>(c) < 0x20) {
							char buf[8];
							snprintf(buf, sizeof buf, "\\u%04x", c);
							out += buf;
						}
						else
							out += c;
				}
			}
			out += '"';
		}

		void Visit(bool boolean) override {
			out += boolean ? "true" : "false";
		}

		void Visit(json::Null const&) override {
			out += "null";
		}
	};

	class Reader {
		std::string_view text;
		size_t pos = 0;

		[[noreturn]] void fail(const char *what) {
			throw json::Exception(std::string(what) + " at offset " + std::to_string(pos));
		}

		void skip_space() {
			while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' || text[pos] == '\r'))
				++pos;
		}

		bool consume(std::string_view word) {
			if (text.substr(pos, word.size()) != word) return false;
			pos += word.size();
			return true;
		}

		unsigned hex4() {
			if (pos + 4 > text.size()) fail("Truncated \\u escape");
			unsigned value = 0;
			for (int i = 0; i < 4; ++i) {
				char c = text[pos++];
				value <<= 4;
				if (c >= '0' && c <= '9') value |= c - '0';
				else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
				else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
				else fail("Invalid \\u escape");
			}
			return value;
		}

		static void append_utf8(std::string& out, uint32_t cp) {
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

		std::string string() {
			++pos; // opening quote
			std::string out;
			while (true) {
				if (pos >= text.size()) fail("Unterminated string");
				char c = text[pos++];
				if (c == '"') return out;
				if (c != '\\') {
					out += c;
					continue;
				}

				if (pos >= text.size()) fail("Unterminated string");
				switch (char e = text[pos++]) {
					case '"': case '\\': case '/': out += e; break;
					case 'b': out += '\b'; break;
					case 'f': out += '\f'; break;
					case 'n': out += '\n'; break;
					case 'r': out += '\r'; break;
					case 't': out += '\t'; break;
					case 'u': {
						uint32_t cp = hex4();
						if (cp >= 0xD800 && cp < 0xDC00 && text.substr(pos, 2) == "\\u") {
							size_t before = pos;
							pos += 2;
							uint32_t low = hex4();
							if (low >= 0xDC00 && low < 0xE000)
								cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
							else
								pos = before;
						}
						if (cp >= 0xD800 && cp < 0xE000)
							cp = 0xFFFD;
						append_utf8(out, cp);
						break;
					}
					default: fail("Invalid escape");
				}
			}
		}

		json::UnknownElement number() {
			size_t start = pos;
			bool is_double = false;
			if (text[pos] == '-') ++pos;
			while (pos < text.size()) {
				char c = text[pos];
				if (c >= '0' && c <= '9') { ++pos; continue; }
				if (c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-') {
					is_double = true;
					++pos;
					continue;
				}
				break;
			}
			std::string num(text.substr(start, pos - start));
			char *end = nullptr;
			if (!is_double) {
				long long value = strtoll(num.c_str(), &end, 10);
				if (end && *end == '\0')
					return static_cast<int64_t>(value);
			}
			double value = strtod(num.c_str(), &end);
			if (!end || *end != '\0' || num.empty()) fail("Invalid number");
			return value;
		}

		json::UnknownElement value() {
			skip_space();
			if (pos >= text.size()) fail("Unexpected end");
			char c = text[pos];
			if (c == '{') {
				++pos;
				json::Object obj;
				skip_space();
				if (pos < text.size() && text[pos] == '}') { ++pos; return obj; }
				while (true) {
					skip_space();
					if (pos >= text.size() || text[pos] != '"') fail("Expected a key");
					auto key = string();
					skip_space();
					if (!consume(":")) fail("Expected ':'");
					obj[key] = value();
					skip_space();
					if (consume(",")) continue;
					if (consume("}")) return obj;
					fail("Expected ',' or '}'");
				}
			}
			if (c == '[') {
				++pos;
				json::Array array;
				skip_space();
				if (pos < text.size() && text[pos] == ']') { ++pos; return array; }
				while (true) {
					array.push_back(value());
					skip_space();
					if (consume(",")) continue;
					if (consume("]")) return array;
					fail("Expected ',' or ']'");
				}
			}
			if (c == '"') return string();
			if (consume("true")) return true;
			if (consume("false")) return false;
			if (consume("null")) return json::Null();
			if (c == '-' || (c >= '0' && c <= '9')) return number();
			fail("Unexpected character");
		}

	public:
		Reader(std::string_view text) : text(text) { }

		json::UnknownElement document() {
			auto result = value();
			skip_space();
			if (pos != text.size()) fail("Text after the value");
			return result;
		}
	};

	json::UnknownElement const *find(json::Object const& obj, std::string_view key) {
		auto it = obj.find(key);
		return it == obj.end() ? nullptr : &it->second;
	}
}

namespace json_io {
	std::string Write(json::UnknownElement const& value) {
		std::string out;
		Writer writer(out);
		value.Accept(writer);
		return out;
	}

	json::UnknownElement Read(std::string_view text) {
		return Reader(text).document();
	}

	json::Object ReadObject(std::string_view text) {
		try {
			auto value = Read(text);
			if (TypeOf(value) == Type::Object)
				return std::move(static_cast<json::Object&>(value));
		}
		catch (json::Exception const&) { }
		return {};
	}

	Type TypeOf(json::UnknownElement const& value) {
		TypeVisitor visitor;
		value.Accept(visitor);
		return visitor.type;
	}

	std::string GetString(json::Object const& obj, std::string_view key, std::string_view def) {
		auto value = find(obj, key);
		if (value && TypeOf(*value) == Type::String)
			return static_cast<json::String const&>(*value);
		return std::string(def);
	}

	int64_t GetInt(json::Object const& obj, std::string_view key, int64_t def) {
		auto value = find(obj, key);
		if (!value) return def;
		switch (TypeOf(*value)) {
			case Type::Int: return static_cast<json::Integer const&>(*value);
			case Type::Double: return static_cast<int64_t>(static_cast<json::Double const&>(*value));
			default: return def;
		}
	}

	double GetDouble(json::Object const& obj, std::string_view key, double def) {
		auto value = find(obj, key);
		if (!value) return def;
		switch (TypeOf(*value)) {
			case Type::Int: return static_cast<double>(static_cast<json::Integer const&>(*value));
			case Type::Double: return static_cast<json::Double const&>(*value);
			default: return def;
		}
	}

	bool GetBool(json::Object const& obj, std::string_view key, bool def) {
		auto value = find(obj, key);
		if (value && TypeOf(*value) == Type::Bool)
			return static_cast<json::Boolean const&>(*value);
		return def;
	}

	json::Array const *GetArray(json::Object const& obj, std::string_view key) {
		auto value = find(obj, key);
		if (value && TypeOf(*value) == Type::Array)
			return &static_cast<json::Array const&>(*value);
		return nullptr;
	}

	json::Object const *GetObject(json::Object const& obj, std::string_view key) {
		auto value = find(obj, key);
		if (value && TypeOf(*value) == Type::Object)
			return &static_cast<json::Object const&>(*value);
		return nullptr;
	}
}
