# Native ASS -> YTT conversion (port of YTSubConverter)

# The output has to match YTSubConverter's float math exactly, so the
# compiler must not fuse multiplications and additions (arm64 would)
ytt_args = meson.get_compiler('cpp').get_supported_arguments(['-ffp-contract=off'])

libytt = static_library('ytt',
    'ass_document.cpp',
    'style_options.cpp',
    'ytt_document.cpp',
    'ytt_util.cpp',
    cpp_args: ytt_args)

# Command line converter, used to compare the output with YTSubConverter's
ytt_convert = executable('ytt_convert', 'ytt_convert_main.cpp',
    link_with: libytt,
    build_by_default: false,
    install: false)
