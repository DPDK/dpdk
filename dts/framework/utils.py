# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

import sys


def check_dts_python_version() -> None:
    if sys.version_info.major < 3 or (
        sys.version_info.major == 3 and sys.version_info.minor < 10
    ):
        print(
            RED(
                (
                    "WARNING: DTS execution node's python version is lower than"
                    "python 3.10, is deprecated and will not work in future releases."
                )
            ),
            file=sys.stderr,
        )
        print(RED("Please use Python >= 3.10 instead"), file=sys.stderr)


def GREEN(text: str) -> str:
    return f"\u001B[32;1m{str(text)}\u001B[0m"


def RED(text: str) -> str:
    return f"\u001B[31;1m{str(text)}\u001B[0m"
