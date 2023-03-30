# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""
DTS logger module with several log level. DTS framework and TestSuite logs
are saved in different log files.
"""

import logging
import os.path
from typing import TypedDict

from .settings import SETTINGS

date_fmt = "%Y/%m/%d %H:%M:%S"
stream_fmt = "%(asctime)s - %(name)s - %(levelname)s - %(message)s"


class LoggerDictType(TypedDict):
    logger: "DTSLOG"
    name: str
    node: str


# List for saving all using loggers
Loggers: list[LoggerDictType] = []


class DTSLOG(logging.LoggerAdapter):
    """
    DTS log class for framework and testsuite.
    """

    _logger: logging.Logger
    node: str
    sh: logging.StreamHandler
    fh: logging.FileHandler
    verbose_fh: logging.FileHandler

    def __init__(self, logger: logging.Logger, node: str = "suite"):
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
        """
        Remove stream handler and logfile handler.
        """
        for handler in (self.sh, self.fh, self.verbose_fh):
            handler.flush()
            self._logger.removeHandler(handler)


def getLogger(name: str, node: str = "suite") -> DTSLOG:
    """
    Get logger handler and if there's no handler for specified Node will create one.
    """
    global Loggers
    # return saved logger
    logger: LoggerDictType
    for logger in Loggers:
        if logger["name"] == name and logger["node"] == node:
            return logger["logger"]

    # return new logger
    dts_logger: DTSLOG = DTSLOG(logging.getLogger(name), node)
    Loggers.append({"logger": dts_logger, "name": name, "node": node})
    return dts_logger
