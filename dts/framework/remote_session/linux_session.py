# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from .posix_session import PosixSession


class LinuxSession(PosixSession):
    """
    The implementation of non-Posix compliant parts of Linux remote sessions.
    """
