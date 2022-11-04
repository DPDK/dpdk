# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

import argparse
import os
from collections.abc import Callable, Iterable, Sequence
from dataclasses import dataclass
from typing import Any, TypeVar

_T = TypeVar("_T")


def _env_arg(env_var: str) -> Any:
    class _EnvironmentArgument(argparse.Action):
        def __init__(
            self,
            option_strings: Sequence[str],
            dest: str,
            nargs: str | int | None = None,
            const: str | None = None,
            default: str = None,
            type: Callable[[str], _T | argparse.FileType | None] = None,
            choices: Iterable[_T] | None = None,
            required: bool = True,
            help: str | None = None,
            metavar: str | tuple[str, ...] | None = None,
        ) -> None:
            env_var_value = os.environ.get(env_var)
            default = env_var_value or default
            super(_EnvironmentArgument, self).__init__(
                option_strings,
                dest,
                nargs=nargs,
                const=const,
                default=default,
                type=type,
                choices=choices,
                required=required,
                help=help,
                metavar=metavar,
            )

        def __call__(
            self,
            parser: argparse.ArgumentParser,
            namespace: argparse.Namespace,
            values: Any,
            option_string: str = None,
        ) -> None:
            setattr(namespace, self.dest, values)

    return _EnvironmentArgument


@dataclass(slots=True, frozen=True)
class _Settings:
    config_file_path: str
    output_dir: str
    timeout: float
    verbose: bool


def _get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="DPDK test framework.")

    parser.add_argument(
        "--config-file",
        action=_env_arg("DTS_CFG_FILE"),
        default="conf.yaml",
        required=False,
        help="[DTS_CFG_FILE] configuration file that describes the test cases, SUTs "
        "and targets.",
    )

    parser.add_argument(
        "--output-dir",
        "--output",
        action=_env_arg("DTS_OUTPUT_DIR"),
        default="output",
        required=False,
        help="[DTS_OUTPUT_DIR] Output directory where dts logs and results are saved.",
    )

    parser.add_argument(
        "-t",
        "--timeout",
        action=_env_arg("DTS_TIMEOUT"),
        default=15,
        required=False,
        help="[DTS_TIMEOUT] The default timeout for all DTS operations except for "
        "compiling DPDK.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action=_env_arg("DTS_VERBOSE"),
        default="N",
        required=False,
        help="[DTS_VERBOSE] Set to 'Y' to enable verbose output, logging all messages "
        "to the console.",
    )

    return parser


def _get_settings() -> _Settings:
    parsed_args = _get_parser().parse_args()
    return _Settings(
        config_file_path=parsed_args.config_file,
        output_dir=parsed_args.output_dir,
        timeout=float(parsed_args.timeout),
        verbose=(parsed_args.verbose == "Y"),
    )


SETTINGS: _Settings = _get_settings()
