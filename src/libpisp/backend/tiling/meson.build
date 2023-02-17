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
    'context_stage.hpp',
    'crop_stage.hpp',
    'input_stage.hpp',
    'output_stage.hpp',
    'pipeline.hpp',
    'pisp_tiling.hpp',
    'rescale_stage.hpp',
    'split_stage.hpp',
    'stages.hpp',
    'tiling.hpp',
    'types.hpp'
])

tiling_include_dir = backend_include_dir / 'tiling'

if install
    install_headers(tiling_headers, subdir: tiling_include_dir)
endif
