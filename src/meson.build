pisp_sources = []
inc_dirs = ['.', 'libpisp']

pisp_include_dir = 'libpisp'

subdir('libpisp')

pisp_sources += frontend_sources
pisp_sources += backend_sources

boost_args = ['-DBOOST_BIND_GLOBAL_PLACEHOLDERS', '-DBOOST_LOG_DYN_LINK=1']

boost_dep = dependency('boost', modules : ['log', 'log_setup', 'thread'])
add_project_arguments(boost_args, language : 'cpp')

pisp_deps = [
    boost_dep
]

# Generate a version string:
version_script = meson.project_source_root() / 'utils' / 'version.py'
version_template = meson.project_source_root() / 'utils' / 'version.cpp.in'
version_cpp = vcs_tag(command : [version_script, meson.project_version()],
                      input : version_template,
                      output : 'version.cpp',
                      fallback : meson.project_version())

pisp_sources += version_cpp

libpisp = library(
    meson.project_name(),
    pisp_sources,
    soversion : meson.project_version(),
    include_directories : include_directories(inc_dirs),
    name_prefix : '',
    install : true,
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

meson.add_install_script('ldconfig')
