// Copyright (c) 2026, TypeMotion
// C++ port of YTSubConverter (https://github.com/arcusmaximus/YTSubConverter),
// Copyright (c) 2018 arcusmaximus, MIT License. See LICENSE-YTSubConverter.txt.

/// @file ytt_convert_main.cpp
/// @brief Command line converter, used to test the port against YTSubConverter
/// @ingroup ytt

#include "ytt.h"

#include <cstdio>
#include <fstream>
#include <iterator>

static bool ReadFile(char const *path, std::string& contents) {
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;
	contents.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	return true;
}

int main(int argc, char **argv) {
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: %s input.ass output.ytt [StyleOptions.xml]\n", argv[0]);
		return 2;
	}

	std::string ass, style_options;
	if (!ReadFile(argv[1], ass)) {
		fprintf(stderr, "Can't read %s\n", argv[1]);
		return 1;
	}
	if (argc == 4)
		ReadFile(argv[3], style_options);

	try {
		std::string ytt = ytt::ConvertAssToYtt(ass, style_options);
		std::ofstream out(argv[2], std::ios::binary);
		out.write(ytt.data(), static_cast<std::streamsize>(ytt.size()));
		if (!out) {
			fprintf(stderr, "Can't write %s\n", argv[2]);
			return 1;
		}
	}
	catch (ytt::ConversionError const& e) {
		fprintf(stderr, "%s\n", e.what());
		return 1;
	}
	return 0;
}
