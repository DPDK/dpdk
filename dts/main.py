#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

"""
A test framework for testing DPDK.
"""

import logging

from framework import dts


def main() -> None:
    dts.run_all()


# Main program begins here
if __name__ == "__main__":
    logging.raiseExceptions = True
    main()
