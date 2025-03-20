#!/bin/sh

# SPDX-License-Identifier: BSD-2-Clause
# Copyright (C) 2023, Raspberry Pi Ltd

cd "$MESON_SOURCE_ROOT" || return
git rev-parse HEAD > "$MESON_DIST_ROOT"/version.gen
