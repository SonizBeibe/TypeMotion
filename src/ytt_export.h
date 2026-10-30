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

/// @file ytt_export.h
/// @brief Exporting subtitles to YouTube's timed text format (.ytt)
/// @ingroup ytt

#pragma once

#include <libaegisub/fs.h>

class AssFile;
namespace agi::vfr { class Framerate; }

/// Export subtitles to a YTT file, the way YTSubConverter converts an
/// exported .ass file
/// @param file Subtitles to export
/// @param fps Timecodes to use for the export
/// @param filename File to write
/// @throws ytt::ConversionError if the subtitles can't be converted
/// @throws agi::Exception if a file can't be read or written
void ExportYtt(AssFile *file, agi::vfr::Framerate const& fps, agi::fs::path const& filename);
