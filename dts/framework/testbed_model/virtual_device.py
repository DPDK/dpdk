# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.


class VirtualDevice(object):
    """
    Base class for virtual devices used by DPDK.
    """

    name: str

    def __init__(self, name: str):
        self.name = name

    def __str__(self) -> str:
        return self.name
