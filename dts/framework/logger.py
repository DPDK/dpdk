# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""DTS logger module.

DTS framework and TestSuite logs are saved in different log files.
"""

import logging
import os.path
from typing import TypedDict

from .settings import SETTINGS

date_fmt = "%Y/%m/%d %H:%M:%S"
stream_fmt = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"


class DTSLOG(logging.LoggerAdapter):
    """DTS logger adapter class for framework and testsuites.

    The :option:`--verbose` command line argument and the :envvar:`DTS_VERBOSE` environment
    variable control the verbosity of output. If enabled, all messages will be emitted to the
    console.

    The :option:`--output` command line argument and the :envvar:`DTS_OUTPUT_DIR` environment
    variable modify the directory where the logs will be stored.

    Attributes:
        node: The additional identifier. Currently unused.
        sh: The handler which emits logs to console.
        fh: The handler which emits logs to a file.
        verbose_fh: Just as fh, but logs with a different, more verbose, format.
    """

    _logger: logging.Logger
    node: str
    sh: logging.StreamHandler
    fh: logging.FileHandler
    verbose_fh: logging.FileHandler

    def __init__(self, logger: logging.Logger, node: str = "suite"):
        """Extend the constructor with additional handlers.

        One handler logs to the console, the other one to a file, with either a regular or verbose
        format.

        Args:
            logger: The logger from which to create the logger adapter.
            node: An additional identifier. Currently unused.
        """
        self._logger = logger
        # 1 means log everything, this will be used by file handlers if their level
        # is not set
        self._logger.setLevel(1)

        self.node = node

        # add handler to emit to stdout
        sh = logging.StreamHandler()
        sh.setFormatter(logging.Formatter(stream_fmt, date_fmt))
        sh.setLevel(logging.INFO)  # console handler default level

        if SETTINGS.verbose is True:
            sh.setLevel(logging.DEBUG)

        self._logger.addHandler(sh)
        self.sh = sh

        # prepare the output folder
        if not os.path.exists(SETTINGS.output_dir):
            os.mkdir(SETTINGS.output_dir)

        logging_path_prefix = os.path.join(SETTINGS.output_dir, node)

        fh = logging.FileHandler(f"{logging_path_prefix}.log")
        fh.setFormatter(
            logging.Formatter(
                fmt="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
                datefmt=date_fmt,
            )
        )

        self._logger.addHandler(fh)
        self.fh = fh

        # This outputs EVERYTHING, intended for post-mortem debugging
        # Also optimized for processing via AWK (awk -F '|' ...)
        verbose_fh = logging.FileHandler(f"{logging_path_prefix}.verbose.log")
        verbose_fh.setFormatter(
            logging.Formatter(
                fmt="%(asctime)s|%(name)s|%(levelname)s|%(pathname)s|%(lineno)d|"
                "%(funcName)s|%(process)d|%(thread)d|%(threadName)s|%(message)s",
                datefmt=date_fmt,
            )
        )

        self._logger.addHandler(verbose_fh)
        self.verbose_fh = verbose_fh

        super(DTSLOG, self).__init__(self._logger, dict(node=self.node))

    def logger_exit(self) -> None:
        """Remove the stream handler and the logfile handler."""
        for handler in (self.sh, self.fh, self.verbose_fh):
            handler.flush()
            self._logger.removeHandler(handler)


class _LoggerDictType(TypedDict):
    logger: DTSLOG
    name: str
    node: str


# List for saving all loggers in use
_Loggers: list[_LoggerDictType] = []


def getLogger(name: str, node: str = "suite") -> DTSLOG:
    """Get DTS logger adapter identified by name and node.

    An existing logger will be returned if one with the exact name and node already exists.
    A new one will be created and stored otherwise.

    Args:
        name: The name of the logger.
        node: An additional identifier for the logger.

    Returns:
        A logger uniquely identified by both name and node.
    """
    global _Loggers
    # return saved logger
    logger: _LoggerDictType
    for logger in _Loggers:
        if logger["name"] == name and logger["node"] == node:
            return logger["logger"]

    # return new logger
    dts_logger: DTSLOG = DTSLOG(logging.getLogger(name), node)
    _Loggers.append({"logger": dts_logger, "name": name, "node": node})
    return dts_logger
