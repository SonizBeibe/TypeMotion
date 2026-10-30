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

/// @file json_io.h
/// @brief Reading and writing the JSON the app and the engine exchange
/// @ingroup android
///
/// The values are libaegisub's JSON elements. Its own reader and writer are
/// made for the configuration files, so they don't read \u escapes and
/// don't escape control characters, which the app's JSON needs.

#pragma once

#include <libaegisub/cajun/elements.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace json_io {
	/// Write a value as compact JSON
	std::string Write(json::UnknownElement const& value);

	/// Read a JSON value
	/// @throws json::Exception if the text isn't valid JSON
	json::UnknownElement Read(std::string_view text);

	/// Read a JSON object, or an empty one if the text isn't an object
	json::Object ReadObject(std::string_view text);

	// Values of an object, or the default when the key is missing or the
	// value has another type. Numbers are converted between integers and
	// floating point.
	std::string GetString(json::Object const& obj, std::string_view key, std::string_view def = "");
	int64_t GetInt(json::Object const& obj, std::string_view key, int64_t def = 0);
	double GetDouble(json::Object const& obj, std::string_view key, double def = 0);
	bool GetBool(json::Object const& obj, std::string_view key, bool def = false);
	/// @return nullptr if missing or not an array
	json::Array const *GetArray(json::Object const& obj, std::string_view key);
	/// @return nullptr if missing or not an object
	json::Object const *GetObject(json::Object const& obj, std::string_view key);

	enum class Type { Null, Bool, Int, Double, String, Array, Object };
	Type TypeOf(json::UnknownElement const& value);

	inline void Add(json::Object&) { }

	template<typename Value, typename... Rest>
	void Add(json::Object& obj, std::string key, Value&& value, Rest&&... rest) {
		obj.emplace(std::move(key), json::UnknownElement(std::forward<Value>(value)));
		Add(obj, std::forward<Rest>(rest)...);
	}

	/// Build an object from keys and values: Obj("a", 1, "b", "text").
	/// The elements can't be copied, so an initializer list doesn't work.
	template<typename... Entries>
	json::Object Obj(Entries&&... entries) {
		json::Object obj;
		Add(obj, std::forward<Entries>(entries)...);
		return obj;
	}
}
