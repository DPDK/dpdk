# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

"""Remote interactive and non-interactive sessions.

This package provides modules for managing remote connections to a remote host (node).

The non-interactive sessions send commands and return their output and exit code.

The interactive sessions open an interactive shell which is continuously open,
allowing it to send and receive data within that particular shell.
"""
