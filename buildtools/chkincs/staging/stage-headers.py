#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025 Red Hat, Inc.

"""
Headers staging script for DPDK build system.
"""

import sys
import os
import shutil
from pathlib import Path

def main():
    """
    Stage header files into a staging directory or clean the staging directory.

    When invoked with "--cleanup <staging_dir> <meson_stamp>", removes and recreates the staging directory.
    Otherwise, creates the staging directory (if needed) and copies the listed header files into it (preserving filenames and metadata).

    Usage:
        stage-headers.py [--cleanup] <staging_dir> <meson_stamp> [headers...]
    """

    if len(sys.argv) < 4:
        print("Usage: stage-headers.py [--cleanup] <staging_dir> <meson_stamp> [headers...]")
        sys.exit(1)

    if len(sys.argv) == 4 and sys.argv[1] == '--cleanup':
        staging_dir = Path(sys.argv[2])
        meson_stamp = Path(sys.argv[3])

        shutil.rmtree(staging_dir)
        staging_dir.mkdir(parents=True, exist_ok=True)

    else:
        staging_dir = Path(sys.argv[1])
        meson_stamp = Path(sys.argv[2])
        headers = sys.argv[3:]

        staging_dir.mkdir(parents=True, exist_ok=True)

        for header in headers:
            file = Path(header)
            shutil.copy2(file, staging_dir / file.name)

    meson_stamp.touch()

if __name__ == "__main__":
    main()
