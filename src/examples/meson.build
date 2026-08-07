# SPDX-License-Identifier: BSD-2-Clause
# Copyright (C) 2025, Raspberry Pi Ltd

opts_dep = dependency('cxxopts', fallback : ['cxxopts', 'cxxopts_dep'])

libpisp_convert = executable('convert', files('convert.cpp'),
                             dependencies: [libpisp_dep, opts_dep],
                             install : false)
