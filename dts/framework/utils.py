# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire


def GREEN(text: str) -> str:
    return f"\u001B[32;1m{str(text)}\u001B[0m"


def RED(text: str) -> str:
    return f"\u001B[31;1m{str(text)}\u001B[0m"
