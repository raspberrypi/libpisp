#!/usr/bin/python3

# SPDX-License-Identifier: CC0-1.0
# Copyright (C) 2021, Raspberry Pi Ltd.
#
# Generate version information for libcamera-apps

import os
import subprocess
import sys
import time
from datetime import datetime
from string import hexdigits

digits = 12


def generate_version():
    try:
        if len(sys.argv) == 2:
            # Check if this is a git directory
            r = subprocess.run(['git', 'rev-parse', '--git-dir'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, universal_newlines=True)
            if r.returncode:
                raise RuntimeError('Invalid git directory!')

            # Get commit id
            r = subprocess.run(['git', 'rev-parse', '--verify', 'HEAD'],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True)
            if r.returncode:
                raise RuntimeError('Invalid git commit!')

            commit = r.stdout.strip('\n')[0:digits]

            # Check dirty status
            r = subprocess.run(['git', 'diff-index', '--quiet', 'HEAD'],
                                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, universal_newlines=True)
            if r.returncode:
                commit = commit + '-dirty'

        elif len(sys.argv) == 3:
            commit = sys.argv[2].lower().strip()
            if any(c not in hexdigits for c in commit):
                raise RuntimeError('Invalid git sha!')

            commit = commit[0:digits]
      
        else:
            raise RuntimeError('Invalid number of command line arguments') 

        commit = f'v{sys.argv[1]} {commit}'

    except RuntimeError as e:
        print(f'ERR: {e}', file=sys.stderr)
        commit = '0' * digits + '-invalid'

    finally:
        date_str = time.strftime(
            "%d-%m-%Y (%H:%M:%S)",
            time.gmtime(int(os.environ.get('SOURCE_DATE_EPOCH', time.time())))
        )
        print(f'{commit} {date_str}', end="")


if __name__ == "__main__":
    generate_version()
