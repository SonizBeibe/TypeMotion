// Copyright (c) 2006, 2007, Niels Martin Hansen
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

/// @file auto4_lua_dialog.h
/// @brief Controls of Lua configuration dialogs, shared by every frontend
/// @ingroup scripting
///

#pragma once

#include "auto4_lua.h"

#include <libaegisub/ass/string_codec.h>
#include <libaegisub/color.h>

#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

namespace Automation4::LuaControl {
	/// A static text label
	class Label final : public LuaDialogControl {
	public:
		std::string label;

		Label(lua_State *L);

		Kind GetKind() const override { return Kind::Label; }

		void LuaReadBack(lua_State *L) override;
	};

	/// A single-line text edit control
	class Edit : public LuaDialogControl {
	public:
		std::string text;

		Edit(lua_State *L);

		Kind GetKind() const override { return Kind::Edit; }

		bool CanSerialiseValue() const override { return true; }
		std::string SerialiseValue() const override { return agi::ass::inline_string_encode(text); }
		void UnserialiseValue(std::string_view serialised) override { text = agi::ass::inline_string_decode(serialised); }

		void LuaReadBack(lua_State *L) override;
	};

	/// A color-picker button
	class Color final : public LuaDialogControl {
	public:
		agi::Color color;
		bool alpha;

		Color(lua_State *L, bool alpha);

		Kind GetKind() const override { return alpha ? Kind::ColorAlpha : Kind::Color; }

		bool CanSerialiseValue() const override { return true; }
		std::string SerialiseValue() const override { return agi::ass::inline_string_encode(color.GetHexFormatted(alpha)); }
		void UnserialiseValue(std::string_view serialised) override {
			color = std::string_view(agi::ass::inline_string_decode(serialised));
		}

		void LuaReadBack(lua_State *L) override;
	};

	/// A multiline text edit control
	class Textbox final : public Edit {
	public:
		Textbox(lua_State *L) : Edit(L) { }

		// Same serialisation interface as single-line edit
		Kind GetKind() const override { return Kind::Textbox; }
	};

	/// Integer only edit
	class IntEdit final : public Edit {
	public:
		int value;
		int min, max;

		IntEdit(lua_State *L);

		Kind GetKind() const override { return Kind::IntEdit; }

		bool CanSerialiseValue() const override  { return true; }
		std::string SerialiseValue() const override { return std::to_string(value); }
		void UnserialiseValue(std::string_view serialised) override { value = atoi(std::string(serialised).c_str()); }

		void LuaReadBack(lua_State *L) override;
	};

	// Float only edit
	class FloatEdit final : public Edit {
	public:
		double value;
		double min;
		double max;
		/// Spin step; a step of zero or less means a plain text field
		double step;

		FloatEdit(lua_State *L);

		Kind GetKind() const override { return Kind::FloatEdit; }

		bool CanSerialiseValue() const override { return true; }
		std::string SerialiseValue() const override { return std::to_string(value); }
		void UnserialiseValue(std::string_view serialised) override { value = atof(std::string(serialised).c_str()); }

		void LuaReadBack(lua_State *L) override;
	};

	/// A dropdown list
	class Dropdown final : public LuaDialogControl {
	public:
		std::vector<std::string> items;
		std::string value;

		Dropdown(lua_State *L);

		Kind GetKind() const override { return Kind::Dropdown; }

		bool CanSerialiseValue() const override { return true; }
		std::string SerialiseValue() const override { return agi::ass::inline_string_encode(value); }
		void UnserialiseValue(std::string_view serialised) override { value = agi::ass::inline_string_decode(serialised); }

		void LuaReadBack(lua_State *L) override;
	};

	class Checkbox final : public LuaDialogControl {
	public:
		std::string label;
		bool value;

		Checkbox(lua_State *L);

		Kind GetKind() const override { return Kind::Checkbox; }

		bool CanSerialiseValue() const override { return true; }
		std::string SerialiseValue() const override { return value ? "1" : "0"; }
		void UnserialiseValue(std::string_view serialised) override { value = serialised != "0"; }

		void LuaReadBack(lua_State *L) override;
	};
}
