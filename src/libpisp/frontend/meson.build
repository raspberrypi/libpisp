# SPDX-License-Identifier: CC0-1.0

frontend_sources = files([
    'frontend.cpp'
])

frontend_headers = files([
    'frontend.hpp',
    'pisp_fe_config.h',
    'pisp_statistics.h'
])

frontend_include_dir = join_paths(pisp_include_dir, 'frontend')
install_headers(frontend_headers, subdir: frontend_include_dir)
