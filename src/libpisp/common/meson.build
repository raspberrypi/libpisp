# SPDX-License-Identifier: CC0-1.0

pisp_sources += files([
    'pisp_utils.cpp',
    'pisp_logging.cpp',
    'pisp_pwl.cpp',
])

common_headers = files([
    'pisp_common.h',
    'logging.hpp',
    'pisp_types.h',
    'utils.hpp',
    'version.hpp',
])

common_include_dir = join_paths(pisp_include_dir, 'common')
install_headers(common_headers, subdir: common_include_dir)
