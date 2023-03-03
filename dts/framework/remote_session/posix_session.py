# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from .os_session import OSSession


class PosixSession(OSSession):
    """
    An intermediary class implementing the Posix compliant parts of
    Linux and other OS remote sessions.
    """
