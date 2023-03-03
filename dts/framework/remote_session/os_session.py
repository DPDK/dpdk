# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2023 University of New Hampshire

from abc import ABC

from framework.config import NodeConfiguration
from framework.logger import DTSLOG

from .remote import RemoteSession, create_remote_session


class OSSession(ABC):
    """
    The OS classes create a DTS node remote session and implement OS specific
    behavior. There a few control methods implemented by the base class, the rest need
    to be implemented by derived classes.
    """

    _config: NodeConfiguration
    name: str
    _logger: DTSLOG
    remote_session: RemoteSession

    def __init__(
        self,
        node_config: NodeConfiguration,
        name: str,
        logger: DTSLOG,
    ):
        self._config = node_config
        self.name = name
        self._logger = logger
        self.remote_session = create_remote_session(node_config, name, logger)

    def close(self, force: bool = False) -> None:
        """
        Close the remote session.
        """
        self.remote_session.close(force)

    def is_alive(self) -> bool:
        """
        Check whether the remote session is still responding.
        """
        return self.remote_session.is_alive()
