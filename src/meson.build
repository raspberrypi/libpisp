pisp_sources = []
pisp_headers = []
inc_dirs = ['.', 'libpisp']

pisp_include_dir = 'libpisp'

install = not meson.is_subproject()

subdir('libpisp')

pisp_sources += frontend_sources
pisp_sources += backend_sources

add_project_arguments('-DBOOST_LOG_DYN_LINK', language : 'cpp')
boost_link_args = ['-DBOOST_LOG_DYN_LINK', '-lboost_system']
add_project_link_arguments(boost_link_args, language : 'cpp')

boost_dep = dependency('boost', modules : ['log', 'log_setup', 'thread'])

pisp_deps = [
    boost_dep
]

# Generate a version string:
version_cpp = vcs_tag(command : [meson.project_source_root() + '/utils/version.py', meson.project_version()],
                      input : meson.project_source_root() + '/utils/version.cpp.in',
                      output : 'version.cpp',
                      fallback : meson.project_version())

pisp_sources += version_cpp

libpisp = shared_library(
    meson.project_name(),
    pisp_sources,
    include_directories : include_directories(inc_dirs),
    name_prefix : '',
    install : install,
    dependencies : pisp_deps
)

libpisp_dep = declare_dependency(
    sources : pisp_sources,
    include_directories : include_directories(inc_dirs),
    link_with : libpisp,
    dependencies : pisp_deps,
    link_args : boost_link_args
)

if install
    pkg_mod = import('pkgconfig')
    pkg_mod.generate(libpisp,
                     description : 'PiSP Library',
                     subdirs : 'libpisp')

    meson.add_install_script('ldconfig')
endif
