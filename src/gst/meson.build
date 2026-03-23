# SPDX-License-Identifier: CC0-1.0
# Copyright (C) 2026, Raspberry Pi Ltd

gst_dep = dependency('gstreamer-1.0', version : '>= 1.14', required : get_option('gstreamer'))
gst_base_dep = dependency('gstreamer-base-1.0', version : '>= 1.14', required : get_option('gstreamer'))
gst_video_dep = dependency('gstreamer-video-1.0', version : '>= 1.14', required : get_option('gstreamer'))
gst_allocators_dep = dependency('gstreamer-allocators-1.0', version : '>= 1.14', required : get_option('gstreamer'))

if gst_dep.found() and gst_base_dep.found() and gst_video_dep.found() and gst_allocators_dep.found()
    gst_sources = [
        'gstpispconvert.cpp',
    ]

    gst_deps = [
        gst_dep,
        gst_base_dep,
        gst_video_dep,
        gst_allocators_dep,
        libpisp_dep,
    ]

    library('gstpispconvert',
            gst_sources,
            dependencies : gst_deps,
            include_directories : include_directories('..'),
            install : true,
            install_dir : get_option('libdir') / 'gstreamer-1.0',
    )
endif
