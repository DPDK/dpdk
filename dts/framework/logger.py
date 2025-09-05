# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2025 Arm Limited

"""DTS logger module.

The module provides several additional features:

    * The storage of DTS execution stages,
    * Logging to console, a human-readable log artifact and a machine-readable log artifact,
    * Optional log artifacts for specific stages.
"""

import logging
from logging import StreamHandler
from typing import TYPE_CHECKING, Any, ClassVar, NamedTuple

if TYPE_CHECKING:
    from framework.testbed_model.artifact import Artifact

date_fmt = "%Y/%m/%d %H:%M:%S"
stream_fmt = "%(asctime)s - %(stage)s - %(name)s - %(levelname)s - %(message)s"
dts_root_logger_name = "dts"


class ArtifactHandler(NamedTuple):
    """A logger handler with an associated artifact."""

    artifact: "Artifact"
    handler: StreamHandler


class DTSLogger(logging.Logger):
    """The DTS logger class.

    The class extends the :class:`~logging.Logger` class to add the DTS execution stage information
    to log records. The stage is common to all loggers, so it's stored in a class variable.

    Any time we switch to a new stage, we have the ability to log to an additional log artifact
    along with a supplementary log artifact with machine-readable format. These two log artifacts
    are used until a new stage switch occurs. This is useful mainly for logging per test suite.
    """

    _stage: ClassVar[str] = "pre_run"
    _root_artifact_handlers: list[ArtifactHandler] = []
    _extra_artifact_handlers: list[ArtifactHandler] = []

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        """Extend the constructor with extra artifact handlers."""
        self._extra_artifact_handlers = []
        super().__init__(*args, **kwargs)

    def makeRecord(self, *args: Any, **kwargs: Any) -> logging.LogRecord:
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

    def add_dts_root_logger_handlers(self, verbose: bool) -> None:
        """Add logger handlers to the DTS root logger.

        This method should be called only on the DTS root logger.
        The log records from child loggers will propagate to these handlers.

        Three handlers are added:

            * A console handler,
            * An artifact handler,
            * A supplementary artifact handler with machine-readable logs
              containing more debug information.

        All log messages will be logged to artifacts. The log level of the console handler
        is configurable with `verbose`.

        Args:
            verbose: If :data:`True`, log all messages to the console.
                If :data:`False`, log to console with the :data:`logging.INFO` level.
        """
        self.setLevel(1)

        sh = StreamHandler()
        sh.setFormatter(logging.Formatter(stream_fmt, date_fmt))
        if not verbose:
            sh.setLevel(logging.INFO)
        self.addHandler(sh)

        self._root_artifact_handlers = self._add_artifact_handlers(self.name)

    def set_stage(self, stage: str) -> None:
        """Set the DTS execution stage.

        Args:
            stage: The DTS stage to set.
        """
        if DTSLogger._stage != stage:
            self.info(f"Moving from stage '{DTSLogger._stage}' to stage '{stage}'.")
            DTSLogger._stage = stage

    def set_custom_log_file(self, log_file_name: str | None) -> None:
        """Set a custom log file.

        Add artifact handlers to the instance if the log artifact file name is provided. Otherwise,
        stop logging to any custom log file.

        The artifact handlers log all messages. One is a regular human-readable log artifact and
        the other one is a machine-readable log artifact with extra debug information.

        Args:
            log_file_name: An optional name of the log artifact file to use. This should be without
                suffix (which will be appended).
        """
        self._remove_extra_artifact_handlers()

        if log_file_name:
            self._extra_artifact_handlers.extend(self._add_artifact_handlers(log_file_name))

    def _add_artifact_handlers(self, log_file_name: str) -> list[ArtifactHandler]:
        """Add artifact handlers to the DTS root logger.

        Add two type of artifact handlers:

            * A regular artifact handler with suffix ".log",
            * A machine-readable artifact handler with suffix ".verbose.log".
              This format provides extensive information for debugging and detailed analysis.

        Args:
            log_file_name: The name of the artifact log file without suffix.

        Returns:
            The newly created artifact handlers.
        """
        from framework.testbed_model.artifact import Artifact

        log_artifact = Artifact("local", f"{log_file_name}.log")
        handler = StreamHandler(log_artifact.open("w"))
        handler.setFormatter(logging.Formatter(stream_fmt, date_fmt))
        self.addHandler(handler)

        verbose_log_artifact = Artifact("local", f"{log_file_name}.verbose.log")
        verbose_handler = StreamHandler(verbose_log_artifact.open("w"))
        verbose_handler.setFormatter(
            logging.Formatter(
                "%(asctime)s|%(stage)s|%(name)s|%(levelname)s|%(pathname)s|%(lineno)d|"
                "%(funcName)s|%(process)d|%(thread)d|%(threadName)s|%(message)s",
                datefmt=date_fmt,
            )
        )
        self.addHandler(verbose_handler)

        return [
            ArtifactHandler(log_artifact, handler),
            ArtifactHandler(verbose_log_artifact, verbose_handler),
        ]

    def _remove_extra_artifact_handlers(self) -> None:
        """Remove any extra artifact handlers that have been added to the logger."""
        if self._extra_artifact_handlers:
            for extra_artifact_handler in self._extra_artifact_handlers:
                self.removeHandler(extra_artifact_handler.handler)

            self._extra_artifact_handlers = []


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
