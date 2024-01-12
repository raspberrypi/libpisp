# SPDX-License-Identifier: CC0-1.0
# Copyright (C) 2023, Raspberry Pi Ltd

pisp_sources = []
inc_dirs = ['.', 'libpisp']

pisp_include_dir = 'libpisp'

subdir('libpisp')

pisp_sources += frontend_sources
pisp_sources += backend_sources

pisp_deps = [
    dependency('nlohmann_json', fallback : ['nlohmann_json', 'nlohmann_json_dep']),
    dependency('threads')
]

# Meson version >= 0.64 can simply use dependency('dl') for this, but we don't want to bump up the min version just yet.
dl_dep = meson.get_compiler('c').find_library('dl', required : true)
pisp_deps += dl_dep

logging_dep = dependency('boost', modules : ['log', 'log_setup', 'thread', 'system'], required : get_option('logging'))
if logging_dep.found()
    logging_args = ['-DPISP_LOGGING_ENABLE=1', '-DBOOST_BIND_GLOBAL_PLACEHOLDERS', '-DBOOST_LOG_DYN_LINK=1']
    pisp_deps += logging_dep
else
    logging_args = '-DPISP_LOGGING_ENABLE=0'
endif

add_project_arguments(logging_args, language : 'cpp')

meson.add_dist_script(meson.project_source_root() / 'utils' / 'gen-dist.sh')

# Generate a version string:
version_cmd = [meson.project_source_root() / 'utils' / 'version.py', meson.project_version()]

# Check if a version.gen file is present.
# This would have been generated from the meson dist command.
fs = import('fs')
dist_version_file = meson.project_source_root() / 'version.gen'
if fs.is_file(dist_version_file)
    version_cmd += fs.read(dist_version_file)
endif

version_template = meson.project_source_root() / 'utils' / 'version.cpp.in'
version_cpp = vcs_tag(command : version_cmd,
                      input : version_template,
                      output : 'version.cpp',
                      fallback : meson.project_version())

pisp_sources += version_cpp

libpisp = library(
    meson.project_name(),
    pisp_sources,
    version : meson.project_version(),
    include_directories : include_directories(inc_dirs),
    name_prefix : '',
    install : true,
    build_rpath : meson.project_source_root(),
    dependencies : pisp_deps,
)

libpisp_dep = declare_dependency(
    include_directories : include_directories(inc_dirs),
    link_with : libpisp,
    sources : [pisp_build_config]
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(libpisp,
                 description : 'PiSP Library',
                 subdirs : 'libpisp')
