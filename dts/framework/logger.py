# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire

"""DTS logger module.

The module provides several additional features:

    * The storage of DTS execution stages,
    * Logging to console, a human-readable log file and a machine-readable log file,
    * Optional log files for specific stages.
"""

import logging
from logging import FileHandler, StreamHandler
from pathlib import Path
from typing import ClassVar

date_fmt = "%Y/%m/%d %H:%M:%S"
stream_fmt = "%(asctime)s - %(stage)s - %(name)s - %(levelname)s - %(message)s"
dts_root_logger_name = "dts"


class DTSLogger(logging.Logger):
    """The DTS logger class.

    The class extends the :class:`~logging.Logger` class to add the DTS execution stage information
    to log records. The stage is common to all loggers, so it's stored in a class variable.

    Any time we switch to a new stage, we have the ability to log to an additional log file along
    with a supplementary log file with machine-readable format. These two log files are used until
    a new stage switch occurs. This is useful mainly for logging per test suite.
    """

    _stage: ClassVar[str] = "pre_run"
    _extra_file_handlers: list[FileHandler] = []

    def __init__(self, *args, **kwargs):
        """Extend the constructor with extra file handlers."""
        self._extra_file_handlers = []
        super().__init__(*args, **kwargs)

    def makeRecord(self, *args, **kwargs) -> logging.LogRecord:
        """Generates a record with additional stage information.

        This is the default method for the :class:`~logging.Logger` class. We extend it
        to add stage information to the record.

        :meta private:

        Returns:
            record: The generated record with the stage information.
        """
        record = super().makeRecord(*args, **kwargs)
        record.stage = DTSLogger._stage
        return record

    def add_dts_root_logger_handlers(self, verbose: bool, output_dir: str) -> None:
        """Add logger handlers to the DTS root logger.

        This method should be called only on the DTS root logger.
        The log records from child loggers will propagate to these handlers.

        Three handlers are added:

            * A console handler,
            * A file handler,
            * A supplementary file handler with machine-readable logs
              containing more debug information.

        All log messages will be logged to files. The log level of the console handler
        is configurable with `verbose`.

        Args:
            verbose: If :data:`True`, log all messages to the console.
                If :data:`False`, log to console with the :data:`logging.INFO` level.
            output_dir: The directory where the log files will be located.
                The names of the log files correspond to the name of the logger instance.
        """
        self.setLevel(1)

        sh = StreamHandler()
        sh.setFormatter(logging.Formatter(stream_fmt, date_fmt))
        if not verbose:
            sh.setLevel(logging.INFO)
        self.addHandler(sh)

        self._add_file_handlers(Path(output_dir, self.name))

    def set_stage(self, stage: str, log_file_path: Path | None = None) -> None:
        """Set the DTS execution stage and optionally log to files.

        Set the DTS execution stage of the DTSLog class and optionally add
        file handlers to the instance if the log file name is provided.

        The file handlers log all messages. One is a regular human-readable log file and
        the other one is a machine-readable log file with extra debug information.

        Args:
            stage: The DTS stage to set.
            log_file_path: An optional path of the log file to use. This should be a full path
                (either relative or absolute) without suffix (which will be appended).
        """
        self._remove_extra_file_handlers()

        if DTSLogger._stage != stage:
            self.info(f"Moving from stage '{DTSLogger._stage}' to stage '{stage}'.")
            DTSLogger._stage = stage

        if log_file_path:
            self._extra_file_handlers.extend(self._add_file_handlers(log_file_path))

    def _add_file_handlers(self, log_file_path: Path) -> list[FileHandler]:
        """Add file handlers to the DTS root logger.

        Add two type of file handlers:

            * A regular file handler with suffix ".log",
            * A machine-readable file handler with suffix ".verbose.log".
              This format provides extensive information for debugging and detailed analysis.

        Args:
            log_file_path: The full path to the log file without suffix.

        Returns:
            The newly created file handlers.

        """
        fh = FileHandler(f"{log_file_path}.log")
        fh.setFormatter(logging.Formatter(stream_fmt, date_fmt))
        self.addHandler(fh)

        verbose_fh = FileHandler(f"{log_file_path}.verbose.log")
        verbose_fh.setFormatter(
            logging.Formatter(
                "%(asctime)s|%(stage)s|%(name)s|%(levelname)s|%(pathname)s|%(lineno)d|"
                "%(funcName)s|%(process)d|%(thread)d|%(threadName)s|%(message)s",
                datefmt=date_fmt,
            )
        )
        self.addHandler(verbose_fh)

        return [fh, verbose_fh]

    def _remove_extra_file_handlers(self) -> None:
        """Remove any extra file handlers that have been added to the logger."""
        if self._extra_file_handlers:
            for extra_file_handler in self._extra_file_handlers:
                self.removeHandler(extra_file_handler)

            self._extra_file_handlers = []


def get_dts_logger(name: str | None = None) -> DTSLogger:
    """Return a DTS logger instance identified by `name`.

    Args:
        name: If :data:`None`, return the DTS root logger.
            If specified, return a child of the DTS root logger.

    Returns:
         The DTS root logger or a child logger identified by `name`.
    """
    original_logger_class = logging.getLoggerClass()
    logging.setLoggerClass(DTSLogger)
    if name:
        name = f"{dts_root_logger_name}.{name}"
    else:
        name = dts_root_logger_name
    logger = logging.getLogger(name)
    logging.setLoggerClass(original_logger_class)
    return logger  # type: ignore[return-value]
