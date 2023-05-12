# SPDX-License-Identifier: CC0-1.0

config_install_dir = get_option('prefix') / get_option('datadir') / 'libpisp'

conf_data = configuration_data()
conf_data.set('PISP_BE_CONFIG_DIR', '"' + config_install_dir + '"')
configure_file(output : 'pisp_build_config.h', configuration : conf_data)

backend_sources = files([
    'backend.cpp',
    'backend_default_config.cpp',
    'backend_prepare.cpp',
])

backend_headers = files([
    'backend.hpp',
    'pisp_be_config.h',
])

backend_include_dir = pisp_include_dir / 'backend'
install_headers(backend_headers, subdir: backend_include_dir)

install_data('backend_default_config.json',
             install_dir : config_install_dir)

subdir('tiling')
