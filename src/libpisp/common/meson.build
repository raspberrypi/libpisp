# SPDX-License-Identifier: CC0-1.0

pisp_sources += files([
    'pisp_utils.cpp',
    'pisp_logging.cpp',
    'pisp_pwl.cpp',
])

common_headers = files([
    'pisp_common.h',
    'pisp_logging.hpp',
    'pisp_pwl.hpp',
    'pisp_types.h',
    'pisp_utils.hpp',
    'version.hpp',
])

pisp_headers += common_headers

common_include_dir = pisp_include_dir / 'common'

if install
    install_headers(common_headers, subdir: common_include_dir)
endif
