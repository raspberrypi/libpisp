pisp_sources = []

pisp_include_dir = 'libpisp'

subdir('common')
subdir('variants')
subdir('frontend')
subdir('backend')

pisp_sources += frontend_sources
pisp_sources += backend_sources

add_project_arguments('-DBOOST_LOG_DYN_LINK', language : 'cpp')
add_project_link_arguments('-DBOOST_LOG_DYN_LINK', language : 'cpp')

boost_dep = dependency('boost', modules : ['log', 'log_setup', 'thread'])

pisp_deps = [
    boost_dep
]


libpisp = shared_library('libpisp',
                           pisp_sources,
                           name_prefix : '',
                           install : true,
                           dependencies : pisp_deps)

pkg_mod = import('pkgconfig')
pkg_mod.generate(libpisp,
                 description : 'Pisp Library',
                 subdirs : 'libpisp')

meson.add_install_script('ldconfig')