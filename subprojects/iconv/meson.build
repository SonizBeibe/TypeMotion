project('iconv', 'c', license: 'LGPL',
        meson_version: '>=0.54.0')

iconv_src = [
    'libcharset/localcharset.c',
    'libiconv/iconv.c'
]

iconv_headers = ['include/iconv.h']

iconv_incs = include_directories(
    'include',
    'libcharset',
    'libiconv'
)

# Where libcharset looks for charset.alias outside of Windows
iconv_args = []
if host_machine.system() != 'windows'
    iconv_args += '-DLIBDIR="' + get_option('prefix') / get_option('libdir') + '"'
endif

libiconv = static_library('iconv', iconv_src, include_directories: iconv_incs, c_args: iconv_args)
libiconv_dep = declare_dependency(link_with: libiconv,
                                  include_directories: iconv_incs)

meson.override_dependency('iconv', libiconv_dep)
