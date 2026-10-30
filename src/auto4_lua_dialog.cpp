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

/// @file auto4_lua_dialog.cpp
/// @brief Lua 5.1-based scripting engine (configuration-dialogue interface)
/// @ingroup scripting
///

#include "auto4_lua_dialog.h"

#include <libaegisub/log.h>
#include <libaegisub/lua/utils.h>
#include <libaegisub/split.h>
#include <libaegisub/string.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/range/algorithm.hpp>
#include <cfloat>
#include <climits>
#include <unordered_map>

using namespace agi::lua;
namespace {
	inline void get_if_right_type(lua_State *L, std::string &def) {
		if (lua_isstring(L, -1))
			def = lua_tostring(L, -1);
	}

	inline void get_if_right_type(lua_State *L, double &def) {
		if (lua_isnumber(L, -1))
			def = lua_tonumber(L, -1);
	}

	inline void get_if_right_type(lua_State *L, int &def) {
		if (lua_isnumber(L, -1))
			def = lua_tointeger(L, -1);
	}

	inline void get_if_right_type(lua_State *L, bool &def) {
		if (lua_isboolean(L, -1))
			def = !!lua_toboolean(L, -1);
	}

	template<class T>
	T get_field(lua_State *L, const char *name, T def) {
		lua_getfield(L, -1, name);
		get_if_right_type(L, def);
		lua_pop(L, 1);
		return def;
	}

	inline std::string get_field(lua_State *L, const char *name) {
		return get_field(L, name, std::string());
	}

	template<class T>
	void read_string_array(lua_State *L, T &cont) {
		lua_for_each(L, [&] {
			if (lua_isstring(L, -1))
				cont.push_back(lua_tostring(L, -1));
		});
	}

	int string_to_button_id(std::string const& str) {
		using Automation4::LuaDialog;
		static std::unordered_map<std::string, int> ids;
		if (ids.empty()) {
			ids["ok"] = LuaDialog::BUTTON_OK;
			ids["yes"] = LuaDialog::BUTTON_YES;
			ids["save"] = LuaDialog::BUTTON_SAVE;
			ids["apply"] = LuaDialog::BUTTON_APPLY;
			ids["close"] = LuaDialog::BUTTON_CLOSE;
			ids["no"] = LuaDialog::BUTTON_NO;
			ids["cancel"] = LuaDialog::BUTTON_CANCEL;
			ids["help"] = LuaDialog::BUTTON_HELP;
			ids["context_help"] = LuaDialog::BUTTON_CONTEXT_HELP;
		}
		auto it = ids.find(str);
		return it == end(ids) ? LuaDialog::BUTTON_CUSTOM : it->second;
	}
}

namespace Automation4 {
	// LuaDialogControl
	LuaDialogControl::LuaDialogControl(lua_State *L)
	// Assume top of stack is a control table (don't do checking)
	: name(get_field(L, "name"))
	, hint(get_field(L, "hint"))
	, x(get_field(L, "x", 0))
	, y(get_field(L, "y", 0))
	, width(get_field(L, "width", 1))
	, height(get_field(L, "height", 1))
	{
		LOG_D("automation/lua/dialog") << "created control: '" << name << "', (" << x << "," << y << ")(" << width << "," << height << "), " << hint;
	}

	namespace LuaControl {
		Label::Label(lua_State *L) : LuaDialogControl(L), label(get_field(L, "label")) { }

		void Label::LuaReadBack(lua_State *L) {
			// Label doesn't produce output, so let it be nil
			lua_pushnil(L);
		}

		Edit::Edit(lua_State *L)
		: LuaDialogControl(L)
		, text(get_field(L, "value"))
		{
			// Undocumented behaviour, 'value' is also accepted as key for text,
			// mostly so a text control can stand in for other things.
			// This shouldn't be exploited and might change later.
			text = get_field(L, "text", text);
		}

		void Edit::LuaReadBack(lua_State *L) {
			lua_pushstring(L, text.c_str());
		}

		Color::Color(lua_State *L, bool alpha)
		: LuaDialogControl(L)
		, color(get_field(L, "value"))
		, alpha(alpha)
		{
		}

		void Color::LuaReadBack(lua_State *L) {
			lua_pushstring(L, color.GetHexFormatted(alpha).c_str());
		}

		IntEdit::IntEdit(lua_State *L)
		: Edit(L)
		, value(get_field(L, "value", 0))
		, min(get_field(L, "min", INT_MIN))
		, max(get_field(L, "max", INT_MAX))
		{
			if (min >= max) {
				max = INT_MAX;
				min = INT_MIN;
			}
		}

		void IntEdit::LuaReadBack(lua_State *L) {
			lua_pushinteger(L, value);
		}

		FloatEdit::FloatEdit(lua_State *L)
		: Edit(L)
		, value(get_field(L, "value", 0.0))
		, min(get_field(L, "min", -DBL_MAX))
		, max(get_field(L, "max", DBL_MAX))
		, step(get_field(L, "step", 0.0))
		{
			if (min >= max) {
				max = DBL_MAX;
				min = -DBL_MAX;
			}
			if (step != 0.0) {
				min = min == -DBL_MAX ? 0.0 : min;
				max = max == DBL_MAX ? 100.0 : max;
			}
		}

		void FloatEdit::LuaReadBack(lua_State *L) {
			lua_pushnumber(L, value);
		}

		Dropdown::Dropdown(lua_State *L)
		: LuaDialogControl(L)
		, value(get_field(L, "value"))
		{
			lua_getfield(L, -1, "items");
			read_string_array(L, items);

#ifdef __WXMAC__
			// Work around a wxOSX bug where combo boxes with the empty string as value still select some item
			// https://github.com/wxWidgets/wxWidgets/issues/25011
			if (value == "" && std::find(items.begin(), items.end(), value) == items.end()) {
				items.insert(items.begin(), value);
			}
#endif
		}

		void Dropdown::LuaReadBack(lua_State *L) {
			lua_pushstring(L, value.c_str());
		}

		Checkbox::Checkbox(lua_State *L)
		: LuaDialogControl(L)
		, label(get_field(L, "label"))
		, value(get_field(L, "value", false))
		{
		}

		void Checkbox::LuaReadBack(lua_State *L) {
			lua_pushboolean(L, value);
		}
	}

	// LuaDialog
	LuaDialog::LuaDialog(lua_State *L, bool include_buttons)
	: use_buttons(include_buttons)
	{
		LOG_D("automation/lua/dialog") << "creating LuaDialoug, addr: " << this;

		// assume top of stack now contains a dialog table
		if (!lua_istable(L, 1))
			error(L, "Cannot create config dialog from something non-table");

		// Ok, so there is a table with controls
		lua_pushvalue(L, 1);
		lua_for_each(L, [&] {
			if (!lua_istable(L, -1))
				error(L, "bad control table entry");

			std::string controlclass = get_field(L, "class");
			boost::to_lower(controlclass);

			std::unique_ptr<LuaDialogControl> ctl;

			// Check control class and create relevant control
			if (controlclass == "label")
				ctl = std::make_unique<LuaControl::Label>(L);
			else if (controlclass == "edit")
				ctl = std::make_unique<LuaControl::Edit>(L);
			else if (controlclass == "intedit")
				ctl = std::make_unique<LuaControl::IntEdit>(L);
			else if (controlclass == "floatedit")
				ctl = std::make_unique<LuaControl::FloatEdit>(L);
			else if (controlclass == "textbox")
				ctl = std::make_unique<LuaControl::Textbox>(L);
			else if (controlclass == "dropdown")
				ctl = std::make_unique<LuaControl::Dropdown>(L);
			else if (controlclass == "checkbox")
				ctl = std::make_unique<LuaControl::Checkbox>(L);
			else if (controlclass == "color")
				ctl = std::make_unique<LuaControl::Color>(L, false);
			else if (controlclass == "coloralpha")
				ctl = std::make_unique<LuaControl::Color>(L, true);
			else if (controlclass == "alpha")
				// FIXME
				ctl = std::make_unique<LuaControl::Edit>(L);
			else
				error(L, "bad control table entry");

			controls.emplace_back(std::move(ctl));
		});

		if (include_buttons && lua_istable(L, 2)) {
			lua_pushvalue(L, 2);
			lua_for_each(L, [&]{
				buttons.emplace_back(BUTTON_CUSTOM, check_string(L, -1));
			});
		}

		if (include_buttons && lua_istable(L, 3)) {
			lua_pushvalue(L, 3);
			lua_for_each(L, [&]{
				int id = string_to_button_id(check_string(L, -2));
				std::string label = check_string(L, -1);
				auto btn = boost::find_if(buttons,
					[&](std::pair<int, std::string>& btn) { return btn.second == label; });
				if (btn == end(buttons))
					error(L, "Invalid button for id %s", lua_tostring(L, -2));
				btn->first = id;
			});
		}

		if (use_buttons && buttons.size() == 0) {
			buttons.emplace_back(BUTTON_OK, "");
			buttons.emplace_back(BUTTON_CANCEL, "");
		}
	}

	int LuaDialog::LuaReadBack(lua_State *L) {
		// First read back which button was pressed, if any
		if (use_buttons) {
			if (button_pushed == -1 || buttons[button_pushed].first == BUTTON_CANCEL)
				lua_pushboolean(L, false);
			else
				lua_pushstring(L, buttons[button_pushed].second.c_str());
		}

		// Then read controls back
		lua_createtable(L, 0, controls.size());
		for (auto& control : controls) {
			control->LuaReadBack(L);
			lua_setfield(L, -2, control->name.c_str());
		}

		return use_buttons ? 2 : 1;
	}

	std::string LuaDialog::Serialise() {
		std::string res;

		// Format into "name1:value1|name2:value2|name3:value3"
		for (auto& control : controls) {
			if (control->CanSerialiseValue()) {
				if (!res.empty())
					res += "|";
				agi::AppendStr(res, agi::ass::inline_string_encode(control->name), ":", control->SerialiseValue());
			}
		}

		return res;
	}

	void LuaDialog::Unserialise(const std::string &serialised) {
		for (auto tok : agi::Split(serialised, '|')) {
			auto pos = tok.find(':');
			if (pos == tok.npos) continue;

			std::string name = agi::ass::inline_string_decode(tok.substr(0, pos));

			// Hand value to all controls matching name
			for (auto& control : controls) {
				if (control->name == name && control->CanSerialiseValue())
					control->UnserialiseValue(tok.substr(pos + 1));
			}
		}
	}
}
