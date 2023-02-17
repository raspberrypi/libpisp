# SPDX-License-Identifier: CC0-1.0

backend_sources = files([
    'backend.cpp',
    'backend_default_config.cpp',
    'backend_prepare.cpp',
])

backend_headers = files([
    'backend.hpp',
    'pisp_be_config.h'
])

pisp_headers += backend_headers

backend_include_dir = pisp_include_dir / 'backend'

if install
    install_headers(backend_headers, subdir: backend_include_dir)
endif

install_data('backend_default_config.json',
             install_dir : '/usr/local/share/libpisp/')

subdir('tiling')
