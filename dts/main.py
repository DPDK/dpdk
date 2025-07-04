#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire
# Copyright(c) 2025 Arm Limited

"""The DTS executable."""

import os

from framework import settings


def main() -> None:
    """Set DTS settings, then run DTS.

    The DTS settings are taken from the command line arguments and the environment variables.
    The settings object is stored in the module-level variable settings.SETTINGS which the entire
    framework uses. After importing the module (or the variable), any changes to the variable are
    not going to be reflected without a re-import. This means that the SETTINGS variable must
    be modified before the settings module is imported anywhere else in the framework.
    """
    settings.SETTINGS = settings.get_settings()

    from framework.runner import DTSRunner

    dts = DTSRunner()

    # After loading up, make the dts folder the current working directory.
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    dts.run()


# Main program begins here
if __name__ == "__main__":
    main()
