#
# project definition
#
project('spice', 'c', 'cpp',
        version : run_command('build-aux/git-version-gen', meson.source_root() + '/.tarball-version', check : true).stdout().strip(),
        license : 'LGPLv2.1',
        meson_version : '>= 0.49',
        default_options : ['buildtype=debugoptimized',
                           'warning_level=2'])

meson.add_dist_script('build-aux/meson-dist', meson.project_version(), meson.source_root())

message('Updating submodules')
run_command('build-aux/meson/check-spice-common', check : true)

#
# soversion
# The versioning is defined by the forumla (CURRENT-AGE.AGE.REVISION)
#
# XXX: KEEP IN SYNC WITH configure.ac file
#
# Follow the libtool manual for the so version:
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
# - If the library source code has changed at all since the last update,
#   then increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# - If any interfaces have been added, removed, or changed since the last update,
#   increment current, and set revision to 0.
# - If any interfaces have been added since the last public release,
#   then increment age.
# - If any interfaces have been removed or changed since the last public release,
#   then set age to 0.
#
#
spice_server_current = 15
spice_server_revision = 3
spice_server_age = 14
spice_server_so_version = '@0@.@1@.@2@'.format(spice_server_current - spice_server_age,
                                               spice_server_age,
                                               spice_server_revision)
message('libspice.so version: ' + spice_server_so_version)

# some global vars
spice_server_global_cflags = ['-DSPICE_SERVER_INTERNAL',
                              '-DG_LOG_DOMAIN="Spice"',
                              '-Wno-sign-compare',
                              '-Wno-unused-parameter']

compiler = meson.get_compiler('c')
cxx_compiler = meson.get_compiler('cpp')
spice_server_config_data = configuration_data()
spice_server_include = [include_directories('.')]
spice_server_deps = [dependency('threads')]
spice_server_link_args = []
spice_server_requires = ''
spice_protocol_version='0.14.3'

#
# Spice common subproject
#
spice_common = subproject('spice-common',
                          default_options : [
                            'generate-code=server',
                            'spice-protocol-version=@0@'.format(spice_protocol_version),
                          ])
spice_server_config_data.merge_from(spice_common.get_variable('spice_common_config_data'))
spice_server_deps += spice_common.get_variable('spice_common_server_dep')
spice_protocol_version = spice_common.get_variable('spice_protocol_version')

#
# check for system headers
#
headers = ['sys/time.h',
           'execinfo.h',
           'linux/sockios.h',
           'pthread_np.h']

foreach header : headers
  if compiler.has_header(header)
    spice_server_config_data.set('HAVE_@0@'.format(header.underscorify().to_upper()), '1')
  endif
endforeach

# TCP_KEEPIDLE definition in netinet/tcp.h
if compiler.has_header_symbol('netinet/tcp.h', 'TCP_KEEPIDLE')
  spice_server_config_data.set('HAVE_TCP_KEEPIDLE', '1')
endif

#
# check for mandatory dependencies
#
glib_version = '2.38'
glib_version_info = '>= @0@'.format(glib_version)
pixman_version = '>= 0.17.7'

deps = {'glib-2.0'       : glib_version_info,
        'pixman-1'       : pixman_version,
        'openssl'        : '>= 1.0.0'}

foreach dep, version : deps
  spice_server_deps += dependency(dep, version : version)
endforeach

# TODO: specify minimum version for jpeg and zlib?
foreach dep : ['libjpeg', 'zlib']
  spice_server_deps += dependency(dep)
endforeach

if host_machine.system() != 'windows'
  foreach dep : ['rt', 'm']
    spice_server_deps += compiler.find_library(dep)
  endforeach
else
  foreach dep : ['ws2_32', 'shlwapi']
    spice_server_deps += compiler.find_library(dep)
  endforeach
endif

#
# Non-mandatory/optional dependencies
#
optional_deps = {'opus' : '>= 0.9.14'}
foreach dep, version : optional_deps
  d = dependency(dep, required : get_option(dep), version : version)
  if d.found()
    spice_server_deps += d
    spice_server_config_data.set('HAVE_@0@'.format(dep.underscorify().to_upper()), '1')
  endif
endforeach

# gstreamer
spice_server_has_gstreamer = false
spice_server_gst_version = get_option('gstreamer')
if spice_server_gst_version != 'no'
  gst_deps = ['gstreamer', 'gstreamer-base', 'gstreamer-app', 'gstreamer-video']
  foreach dep : gst_deps
    dep = '@0@-@1@'.format(dep, spice_server_gst_version)
    spice_server_deps += dependency(dep)
  endforeach
  spice_server_deps += dependency('orc-0.4')

  gst_def = 'HAVE_GSTREAMER'
  if spice_server_gst_version == '1.0'
    gst_def = 'HAVE_GSTREAMER_1_0'
  endif

  spice_server_config_data.set(gst_def, '1')
  spice_server_has_gstreamer = true
endif

# lz4
spice_server_has_lz4 = false
if get_option('lz4')
  lz4_dep = dependency('liblz4', required : false, version : '>= 129')
  if not lz4_dep.found()
    lz4_dep = dependency('liblz4', version : '>= 1.7.3')
  endif

  if compiler.has_function('LZ4_compress_fast_continue', dependencies : lz4_dep)
    spice_server_config_data.set('HAVE_LZ4_COMPRESS_FAST_CONTINUE', '1')
  endif

  spice_server_deps += lz4_dep
  spice_server_config_data.set('USE_LZ4', '1')
  spice_server_has_lz4 = true
endif

# sasl
spice_server_has_sasl = false
if get_option('sasl')
  spice_server_deps += dependency('libsasl2')
  spice_server_config_data.set('HAVE_SASL', '1')
  spice_server_has_sasl = true
endif

# smartcard check
spice_server_has_smartcard = false
smartcard_dep = dependency('libcacard', required : get_option('smartcard'), version : '>= 2.5.1')
if smartcard_dep.found()
  spice_server_deps += smartcard_dep
  spice_server_config_data.set('USE_SMARTCARD', '1')
  spice_server_has_smartcard = true
  spice_server_requires += 'libcacard >= 2.5.1 '
endif

#
# global C defines
#
glib_encoded_version = 'GLIB_VERSION_@0@'.format(glib_version.underscorify())
spice_server_global_cflags += ['-DGLIB_VERSION_MIN_REQUIRED=@0@'.format(glib_encoded_version),
                               '-DGLIB_VERSION_MAX_ALLOWED=@0@'.format(glib_encoded_version)]

add_project_arguments(compiler.get_supported_arguments(spice_server_global_cflags),
                      language : 'c')

spice_server_global_cxxflags = spice_server_global_cflags
spice_server_global_cxxflags += [
  '-fno-exceptions',
  '-Wno-suggest-final-methods',
  '-Wno-suggest-final-types',
  '-Wno-array-bounds',
  '-Wno-stringop-overflow',
  '-Wno-narrowing',
  '-Wno-missing-field-initializers',
  '-Wno-deprecated-declarations',
  '-Wshadow',
]
add_project_arguments(cxx_compiler.get_supported_arguments(spice_server_global_cxxflags),
                      language : 'cpp')

#
# Subdirectories
#
subdir('server')
subdir('tools')
subdir('docs')

#
# write config.h
#
spice_server_config_data.set_quoted('VERSION', meson.project_version())
spice_server_config_data.set('SPICE_USE_SAFER_CONTAINEROF', '1')

if get_option('statistics')
  spice_server_config_data.set('RED_STATISTICS', '1')
endif

# Minimal Win32 version
if host_machine.system() == 'windows'
  spice_server_config_data.set('_WIN32_WINNT', '0x600')
endif

configure_file(output : 'config.h',
               configuration : spice_server_config_data)

run_target('doxy', command : './doxygen.sh')
