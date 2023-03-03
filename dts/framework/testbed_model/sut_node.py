# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2023 PANTHEON.tech s.r.o.

from .node import Node


class SutNode(Node):
    """
    A class for managing connections to the System under Test, providing
    methods that retrieve the necessary information about the node (such as
    CPU, memory and NIC details) and configuration capabilities.
    """
