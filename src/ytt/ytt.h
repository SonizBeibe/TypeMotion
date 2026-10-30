// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ytt.h
/// @brief Native ASS -> YTT (SRV3) conversion, replacing the external ytsubconverter.exe
/// @ingroup ytt

#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace ytt {

/// Thrown when the subtitles can't be converted. The message matches the one
/// YTSubConverter would have shown for the same input.
struct ConversionError final : std::runtime_error {
	using std::runtime_error::runtime_error;
};

/// Convert the text of an .ass file to the text of a .ytt file
/// @param ass        Contents of the .ass file (UTF-8 or UTF-16 with BOM)
/// @param user_style_options Contents of a StyleOptions.xml written by
///                   YTSubConverter, or empty. As in YTSubConverter, the
///                   built-in YT* style options take precedence.
/// @return UTF-8 encoded .ytt document, byte for byte what YTSubConverter
///         would write for the same input
std::string ConvertAssToYtt(std::string_view ass, std::string_view user_style_options = {});

}
