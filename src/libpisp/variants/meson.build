# SPDX-License-Identifier: CC0-1.0

pisp_sources += files([
    'bcm2712.cpp',
])

variants_headers = files([
    'pisp_variant.hpp'
])

pisp_headers += variants_headers

variants_include_dir = pisp_include_dir / 'variants'

if install
    install_headers(variants_headers, subdir: variants_include_dir)
endif
