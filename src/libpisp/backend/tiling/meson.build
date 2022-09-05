# SPDX-License-Identifier: CC0-1.0

backend_sources += files([
    'context_stage.cpp',
    'crop_stage.cpp',
    'input_stage.cpp',
    'output_stage.cpp',
    'pipeline.cpp',
    'pisp_tiling.cpp',
    'rescale_stage.cpp',
    'split_stage.cpp',
    'stages.cpp',
])

tiling_headers = files([
    'context_stage.h',
    'crop_stage.h',
    'input_stage.h',
    'output_stage.h',
    'pipeline.h',
    'pisp_tiling.h',
    'rescale_stage.h',
    'split_stage.h',
    'stages.h',
    'tiling.h',
    'types.h'
])

tiling_include_dir = backend_include_dir / 'tiling'

if install
    install_headers(tiling_headers, subdir: tiling_include_dir)
endif
