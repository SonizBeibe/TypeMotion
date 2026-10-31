project('Aegisub', ['c', 'cpp'],
        license: 'BSD-3-Clause',
        meson_version: '>=0.60.0',
        default_options: [
            'cpp_std=c++20',
            'warning_level=3',
            'buildtype=debugoptimized',
            'b_ndebug=if-release',

            'harfbuzz:icu=disabled',
            'ffmpeg:libdav1d=enabled',
            'luajit:default_library=static',
            'luajit:amalgam=true',
            'luajit:lua52compat=true',
            'luajit:luajit=false',
        ],
        version: '3.4.1')

cmake = import('cmake')

# Build only the subtitle core, without wxWidgets or anything else that only
# the desktop interface uses (used by the Android app)
core_only = get_option('core_only')

if host_machine.system() == 'windows'
    add_project_arguments('-DUNICODE', '-DNOMINMAX', '-D_WIN32_WINNT=0x0601', '-wd4702', '-wd4458', '-wd4100', language: 'cpp')

    if not get_option('csri').disabled()
        add_global_arguments('-DCSRI_NO_EXPORT', language: 'c')
    endif

    if get_option('fallback_nasm') or \
       not find_program('nasm', required: false).found()
        nasm = subproject('nasm').get_variable('nasm')
        meson.override_find_program('nasm', nasm)
    endif
endif

if build_machine.system() == 'windows'
    powershell_exe = find_program('powershell')
    version_sh = [powershell_exe,
                  '-ExecutionPolicy', 'Bypass',
                  '-File', meson.current_source_dir() / 'tools' / 'version.ps1']
else
    version_sh = find_program('tools/version.sh')
endif
version_inc = include_directories('.')
version_h = custom_target('git_version.h',
                          command: [version_sh, meson.current_build_dir(), meson.current_source_dir()],
                          build_by_default: true,
                          build_always_stale: true, # has internal check whether target file will be refreshed
                          output: ['git_version.h'])

meson.add_dist_script(version_sh, meson.current_build_dir(), meson.current_source_dir(), 'dist')

if host_machine.system() == 'darwin' and get_option('build_osx_bundle')
    prefix = meson.current_build_dir() / 'Aegisub.app' / 'Contents'
    bindir = prefix / 'MacOS'
    datadir = prefix / 'SharedSupport'
    localedir = prefix / 'Resources'
    dataroot = datadir
else
    prefix = get_option('prefix')
    bindir = prefix / get_option('bindir')
    datadir = prefix / get_option('datadir')
    localedir = prefix / get_option('localedir')
    dataroot = datadir / 'aegisub'
endif
docdir = prefix / 'doc'

# MSVC sets this automatically with -MDd, but it has a different meaning on other platforms
if get_option('debug') and host_machine.system() != 'windows'
    add_project_arguments('-D_DEBUG', language: 'cpp')
endif

conf = configuration_data()
conf.set_quoted('P_DATA', dataroot)
conf.set_quoted('P_LOCALE', localedir)
if get_option('credit') != ''
    conf.set_quoted('BUILD_CREDIT', get_option('credit'))
endif
if get_option('official_release')
    conf.set('AEGI_OFFICIAL_RELEASE', 1)
endif
conf.set('WITH_UPDATE_CHECKER', get_option('enable_update_checker'))
if get_option('enable_update_checker')
    conf.set_quoted('UPDATE_CHECKER_SERVER', get_option('update_server'))
    conf.set_quoted('UPDATE_CHECKER_BASE_URL', get_option('update_url'))
endif

deps = []

if host_machine.system() == 'darwin'
    add_languages('objc', 'objcpp')
    add_project_arguments('-DGL_SILENCE_DEPRECATION', language: 'cpp')
    # meson neither supports objcpp_std nor inherits cpp_std https://github.com/mesonbuild/meson/issues/5495
    add_project_arguments('-std=c++20', language: 'objcpp')
elif host_machine.system() != 'windows' and not core_only
    conf.set('WITH_FONTCONFIG', 1)
    deps += dependency('fontconfig')
endif

cxx = meson.get_compiler('cpp')
cc = meson.get_compiler('c')
deps += cc.find_library('m', required: false)
deps += cc.find_library('dl', required: false)

deps += dependency('iconv', fallback: ['iconv', 'libiconv_dep'])

deps += dependency('libass', version: '>=0.9.7',
                   fallback: ['libass', 'libass_dep'])

boost_static = meson.version().version_compare('>=0.63.0') and get_option('prefer_static')    # boost dependencies do not respect prefer_static https://github.com/mesonbuild/meson/issues/15458
boost_modules = ['chrono', 'locale']

if boost_static
    boost_modules += 'thread'   # static boost dependencies do not include transitive dependencies https://github.com/mesonbuild/meson/issues/15457
endif

if not get_option('local_boost')
    boost_dep = dependency('boost', version: '>=1.70.0',
                            modules: boost_modules,
                            required: false,
                            static: boost_static)

    if boost_dep.version().version_compare('<1.77.0')
        # boost regex is header-only since version 1.77.0, and some distributions have stopped shipping the stub module since then.
        # So we need to require boost.regex for older versions but can't require it for newer versions.
        boost_dep = dependency('boost', version: '>=1.70.0',
                                modules: boost_modules + ['regex'],
                                required: false,
                                static: boost_static)
    endif
endif

if get_option('local_boost') or not boost_dep.found()
    boost_dep = []
    boost = subproject('boost')
    foreach module: (boost_modules + ['regex'])
        boost_dep += boost.get_variable('boost_' + module + '_dep')
    endforeach
endif

if cc.get_argument_syntax() == 'gcc'
  # Work around compile error in Boost GIL, can be removed once
  # https://github.com/boostorg/gil/commit/2956a0e9ef71f5c4f3163c8b8d675679e0fbaaff
  # is in a boost release that's available everywhere
  add_project_arguments(cc.get_supported_arguments(['-Wno-missing-template-arg-list-after-template-kw']), language: 'cpp')
endif

deps += boost_dep
if host_machine.system() == 'windows'
    conf.set('BOOST_USE_WINDOWS_H', 1)
endif

deps += dependency('zlib')

if core_only
    # Measuring text for automation scripts reads the fonts with FreeType
    deps += dependency('freetype2', fallback: ['freetype2', 'freetype_dep'])
    wx_dep = dependency('', required: false)
else
    wx_dep = dependency('wxWidgets', version: '>=' + get_option('wx_version'),
                        required: false,
                        modules: ['std', 'stc', 'gl'])
endif

if core_only
    # The core doesn't use wxWidgets
elif wx_dep.found()
    deps += wx_dep
    wx_defines_dep = wx_dep
else
    build_shared = true
    if get_option('default_library') == 'static'
        build_shared = false
    endif
    build_type = 'Release'
    if get_option('buildtype') == 'debug'
        build_type = 'Debug'
    endif

    opt_var = cmake.subproject_options()
    opt_var.set_override_option('cpp_std', 'c++20')
    opt_var.add_cmake_defines({
        'CMAKE_POLICY_VERSION_MINIMUM': '3.31',

        'wxBUILD_INSTALL': false,
        'wxBUILD_PRECOMP': 'OFF',       # otherwise breaks project generation w/ meson
        'wxBUILD_SHARED': build_shared,

        'wxUSE_WEBVIEW': false,         # breaks build on linux
        'CMAKE_BUILD_TYPE': build_type,
        'wxUSE_IMAGE': true,
        'wxBUILD_MONOLITHIC': true      # otherwise breaks project generation w/ meson
    })

    wx = cmake.subproject('wxWidgets', options: opt_var)

    deps += wx.dependency('wxmono')
    foreach cmake_target: ['wxregex', 'wxscintilla', 'wxlexilla']
        if wx.target_list().contains(cmake_target)
            deps += wx.dependency(cmake_target)
        endif
    endforeach

    # meson doesn't let you get defines from a library that has sources which need to be built,
    # but allows it if you use partial_dependency() to get only includes and compile args.
    wx_defines_dep = wx.dependency('wxmono').partial_dependency(compile_args: true, includes: true)

    if host_machine.system() == 'windows' or host_machine.system() == 'darwin'
        deps += [
            wx.dependency('wxpng'),
        ]
    endif

    if host_machine.system() == 'windows'
        deps += [
            wx.dependency('wxzlib'),
            wx.dependency('wxexpat'),
        ]

        if cc.has_header('rpc.h')
            deps += cc.find_library('rpcrt4', required: true)
        else
            error('Missing Windows SDK RPC Library (rpc.h / rpcrt4.lib)')
        endif
        if cc.has_header('commctrl.h')
            deps += cc.find_library('comctl32', required: true)
        else
            error('Missing Windows SDK Common Controls Library (commctrl.h / comctl32.lib)')
        endif
    endif

    conf.set('WITH_INTERNAL_WXWIDGETS', 1)
endif

deps += dependency('icu-uc', version: '>=4.8.1.1')
deps += dependency('icu-i18n', version: '>=4.8.1.1')

check_deps = [
    # audio, in order of precedence
    ['libpulse', [], 'PulseAudio', [], []],
    ['alsa', [], 'ALSA', [], []],
    ['portaudio-2.0', [], 'PortAudio', [], []],
    ['openal', '>=0.0.8', 'OpenAL', [], ['OpenAL']],
    # video
    ['ffms2', '>=2.22', 'FFMS2', ['ffms2', 'ffms2_dep'], []],
    # other
    ['fftw3', [], 'FFTW3', [], []],
    ['hunspell', [], 'Hunspell', ['hunspell', 'hunspell_dep'], []],
    ['uchardet', [], 'uchardet', ['uchardet', 'uchardet_dep'], []],
]

if core_only
    # Audio output, FFTW (audio spectrum), Hunspell and libportal are only
    # used by the desktop interface
    check_deps = [
        ['ffms2', '>=2.22', 'FFMS2', ['ffms2', 'ffms2_dep'], []],
        ['uchardet', [], 'uchardet', ['uchardet', 'uchardet_dep'], []],
    ]
elif host_machine.system() == 'linux'
    if cc.get_define('__WXGTK3__', dependencies: wx_defines_dep, prefix: '#include <wx/defs.h>') != ''
        check_deps += [['libportal-gtk3', [], 'libportal', [], []]]
    else
        check_deps += [['libportal', [], 'libportal', [], []]]
    endif
endif

dep_avail = []
foreach dep: check_deps
    optname = dep[0].split('-')[0]
    if not get_option(optname).disabled()
        # [provide] section is ignored if required is false;
        # must provided define fallback explicitly
        # (with meson 0.56 you can do allow_fallback: true):
        #d = dependency(dep[0], version: dep[1],
        #               required: false, allow_fallback: true)
        if dep[3].length() > 0
            d = dependency(dep[0], version: dep[1], fallback: dep[3])
        else
            d = dependency(dep[0], version: dep[1], required: false)
        endif

        if not d.found() and dep[4].length() > 0 and host_machine.system() == 'darwin'
            d = dependency('appleframeworks', modules: dep[4], required: false)
        endif

        if d.found()
            deps += d
            conf.set('WITH_@0@'.format(dep[0].split('-')[0].to_upper()), 1)
            dep_avail += dep[2]
        elif get_option(optname).enabled()
            error('@0@ enabled but not found'.format(dep[2]))
        endif
    endif
endforeach

if host_machine.system() == 'windows' and get_option('avisynth').enabled()
    conf.set('WITH_AVISYNTH', 1) # bundled separately with installer
endif

if host_machine.system() == 'windows' and not get_option('directsound').disabled() and not core_only
    dsound_dep = cc.find_library('dsound', required: get_option('directsound'))
    winmm_dep = cc.find_library('winmm', required: get_option('directsound'))
    ole32_dep = cc.find_library('ole32', required: get_option('directsound'))
    have_dsound_h = cc.has_header('dsound.h')
    if not have_dsound_h and get_option('directsound').enabled()
        error('DirectSound enabled but dsound.h not found')
    endif

    dxguid_dep = cc.find_library('dxguid', required: true)
    if dsound_dep.found() and winmm_dep.found() and ole32_dep.found() and dxguid_dep.found() and have_dsound_h
        deps += [dsound_dep, winmm_dep, ole32_dep, dxguid_dep]
        conf.set('WITH_DIRECTSOUND', 1)
        dep_avail = ['DirectSound'] + dep_avail
    endif
endif

if core_only and host_machine.system() == 'android'
    # Audio output of the Android app
    deps += cc.find_library('aaudio')
    conf.set('WITH_AAUDIO', 1)
    dep_avail += 'AAudio'
endif

if core_only and host_machine.system() == 'darwin'
    # libaegisub/osx uses Foundation (NSBundle, NSProcessInfo) and the fonts
    # are listed with CoreText, which iOS has too
    deps += dependency('appleframeworks', modules: ['Foundation', 'CoreFoundation', 'CoreText'])
    # Audio output of the iPhone app
    deps += dependency('appleframeworks', modules: ['AudioToolbox'])
    conf.set('WITH_AUDIOUNIT', 1)
    dep_avail += 'AudioUnit'
endif

if host_machine.system() == 'windows' and cc.has_header('dwrite_3.h')
    conf.set('HAVE_DWRITE_3', 1)
endif

if host_machine.system() == 'darwin' and not core_only
    deps += dependency('appleframeworks', modules : ['CoreText', 'CoreFoundation', 'AppKit', 'Carbon', 'IOKit', 'QuartzCore'])
    deps += dependency('appleframeworks', modules : ['ScreenCaptureKit'], required : false)
endif

# TODO: OSS

def_audio = get_option('default_audio_output')
if def_audio != 'auto'
    if not dep_avail.contains(def_audio)
        error('Default audio output "@0@" selected but not available'.format(def_audio))
    endif
elif dep_avail.length() != 0
    def_audio = dep_avail[0]
else
    def_audio = ''
endif

conf_meson = configuration_data()
conf_meson.set('DEFAULT_PLAYER_AUDIO', def_audio)

if get_option('enable_update_checker') and not core_only
    curl_disable = ['tool', 'dict', 'file', 'ftp', 'gopher', 'imap', 'ldap', 'ldaps', 'mqtt', 'pop3', 'rtmp', 'rtsp', 'smb', 'smtp', 'telnet', 'tftp']
    curl_options = []
    foreach opt : curl_disable
        curl_options += opt + '=disabled'
    endforeach

    if host_machine.system() == 'windows'
        curl_options += 'schannel=enabled'
    elif host_machine.system() == 'darwin'
        curl_options += 'secure-transport=enabled'
    else
        curl_options += 'openssl=enabled'
    endif

    deps += dependency('libcurl', allow_fallback: true, default_options: curl_options)
endif

luajit = dependency('luajit', version: '>=2.0.0', required: get_option('system_luajit'),
                    allow_fallback: false, method: 'pkg-config')
if luajit.found() and luajit.type_name() != 'internal'
    luajit_test = cc.run(files('tests/tests/luajit_52.c'), dependencies: luajit)

    if luajit_test.returncode() != 0
        if get_option('system_luajit')
            error('System luajit found but not compiled in 5.2 mode')
        else
            message('System luajit found but not compiled in 5.2 mode')
            luajit = dependency('', required: false)
        endif
    else
        deps += luajit
    endif
endif

if not luajit.found()
    message('Using built-in luajit')
    subproject('luajit') # Initialize subproject to ignore system luajit
    luajit = dependency('luajit')
    assert(luajit.type_name() == 'internal', 'System luajit used instead of built-in luajit')
endif

if luajit.type_name() == 'internal'
    deps += luajit
endif

deps += subproject('luabins').get_variable('luabins_dep')

if core_only
    dep_gl = dependency('', required: false)
else
    dep_gl = dependency('gl', required: false)
endif
if not dep_gl.found() and not core_only
    if host_machine.system() == 'windows'
        dep_gl = cc.find_library('opengl32', required: false)
    else
        dep_gl = cc.find_library('GL', required: false)
    endif

    if not cc.has_header('GL/gl.h')
        dep_gl = dependency('', required: false)
    endif
endif
if host_machine.system() == 'darwin'
    conf.set('HAVE_OPENGL_GL_H', 1)
else
    conf.set('HAVE_GL_GLEXT_H', cc.has_header('GL/glext.h'))
endif

if not dep_gl.found() and not core_only
    error('OpenGL implementation not found')
endif

deps += dep_gl

if not get_option('csri').disabled() and host_machine.system() == 'windows'
    conf.set('WITH_CSRI', 1)

    csri_sp = subproject('csri')
    deps += csri_sp.get_variable('csri_dep')
endif

if cc.has_function('alloca', prefix: '#include <stdlib.h>')
    # The files using alloca() already include <stdlib.h> unconditionally.
elif cc.has_function('alloca', prefix: '#include <alloca.h>')
    conf.set('HAVE_ALLOCA_H', true)
elif cc.has_header_symbol('malloc.h', '_alloca', prefix: '#include <stdlib.h>')
    # cc.has_function() can't find _alloca() intrinsic on cl.exe,
    # so we use cc.has_header_symbol().
    conf.set('HAVE_MALLOC_H', true)
    conf.set('HAVE_UNDERLINE_ALLOCA', true)
elif cc.has_header_symbol('malloc.h', 'alloca', prefix: '#include <stdlib.h>')
    # clang-cl.exe has the wrong name for cl.exe's _alloca()
    conf.set('HAVE_MALLOC_H', true)
endif

# the configuration_data as compiler flags
conf_defines = []
foreach key : conf.keys()
    # Some hacks since meson apparently can't give you the type of a variable
    if '@0@'.format(conf.get(key)) == 'true'
        assert(conf.get(key) == true)
        conf_defines += '-D@0@'.format(key)
    elif '@0@'.format(conf.get(key)) != 'false'
        # conf.set() with a string quotes the string so conf.get(key) can't be the actual string 'false'
        conf_defines += '-D@0@=@1@'.format(key, conf.get(key))
    endif
endforeach

aegisub_order_dep = []
aegisub_defines = []

if get_option('b_pch')
    # Write the feature flags into a configure_file to not bloat the compiler args too much
    aegisub_order_dep += configure_file(output: 'acconf.h', configuration: conf)
else
    # Manually pass the feature flags as compiler args
    aegisub_defines += conf_defines

    if host_machine.system() == 'windows'
        # This is also part of all the PCHs
        aegisub_defines += '-DWIN32_LEAN_AND_MEAN'
    endif
endif

subdir('automation')
subdir('libaegisub')
if not core_only
    subdir('packages')
    subdir('po')
endif
subdir('src')
if get_option('tests')
    subdir('tests')
endif

if core_only
    # The subtitle core, for a frontend which implements ui_hooks.h, the
    # translation functions of i18n.h and the parts of the automation
    # interface which show things (see headless/ for an example)
    core_args = conf_defines + ['-DTYPEMOTION_NO_WX']
    core_inc = [libaegisub_inc, libresrc_inc, version_inc, include_directories('src')]

    libtypemotion_core = static_library('typemotion_core', core_src, version_h,
                                        c_args: core_args,
                                        cpp_args: core_args,
                                        include_directories: core_inc,
                                        dependencies: deps)

    typemotion_core_dep = declare_dependency(compile_args: core_args,
                                             include_directories: core_inc,
                                             link_with: [libtypemotion_core, libresrc, libaegisub, libytt],
                                             dependencies: deps)

    # Command line tool to use the core without any interface
    executable('typemotion-headless', headless_src,
               dependencies: typemotion_core_dep,
               install: false)

    # The native library of the Android app
    if host_machine.system() == 'android' or get_option('jni_bridge')
        if host_machine.system() == 'android'
            jni_deps = [cc.find_library('log')]
        else
            # Meson finds the JDK's jni.h through its Java support
            add_languages('java', native: false)
            jni_deps = [dependency('jni', version: '>=1.6')]
        endif
        shared_library('typemotion', android_src,
                       dependencies: [typemotion_core_dep] + jni_deps,
                       install: false)
    endif

    # The engine of the iPhone app, which the CI workflow merges with the
    # libraries it uses into one for Xcode (see ios/README.md)
    if host_machine.system() == 'darwin'
        static_library('typemotion_ios', ios_src,
                       dependencies: typemotion_core_dep,
                       install: false)
    endif

    subdir_done()
endif

aegisub_cpp_pch = ['src/include/agi_pre.h']
aegisub_c_pch = ['src/include/agi_pre_c.h']

link_args = []
link_depends = []
if host_machine.system() == 'windows'
    manifest_file = configure_file(copy: true, input: 'src/res/aegisub.exe.manifest', output: 'aegisub.exe.manifest')
    link_args += ['/MANIFEST:EMBED', '/MANIFESTINPUT:@0@'.format(manifest_file)]
    link_depends += manifest_file
endif

aegisub = executable('TypeMotion', aegisub_src, version_h, resrc, aegisub_order_dep,
                     c_args: aegisub_defines,
                     cpp_args: aegisub_defines,
                     link_with: [libresrc, libaegisub, libytt],
                     link_args: link_args,
                     link_depends: link_depends,
                     include_directories: [libaegisub_inc, libresrc_inc, version_inc, include_directories('src')],
                     cpp_pch: aegisub_cpp_pch,
                     c_pch: aegisub_c_pch,
                     install: true,
                     install_dir: bindir,
                     dependencies: deps,
                     win_subsystem: 'windows')
