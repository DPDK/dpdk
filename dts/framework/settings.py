# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

import argparse
import os
from collections.abc import Callable, Iterable, Sequence
from dataclasses import dataclass
from pathlib import Path
from typing import Any, TypeVar

from .utils import DPDKGitTarball

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
            required: bool = False,
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
    skip_setup: bool
    dpdk_tarball_path: Path
    compile_timeout: float
    test_cases: list
    re_run: int


def _get_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run DPDK test suites. All options may be specified with "
        "the environment variables provided in brackets. "
        "Command line arguments have higher priority.",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )

    parser.add_argument(
        "--config-file",
        action=_env_arg("DTS_CFG_FILE"),
        default="conf.yaml",
        help="[DTS_CFG_FILE] configuration file that describes the test cases, SUTs "
        "and targets.",
    )

    parser.add_argument(
        "--output-dir",
        "--output",
        action=_env_arg("DTS_OUTPUT_DIR"),
        default="output",
        help="[DTS_OUTPUT_DIR] Output directory where dts logs and results are saved.",
    )

    parser.add_argument(
        "-t",
        "--timeout",
        action=_env_arg("DTS_TIMEOUT"),
        default=15,
        type=float,
        help="[DTS_TIMEOUT] The default timeout for all DTS operations except for "
        "compiling DPDK.",
    )

    parser.add_argument(
        "-v",
        "--verbose",
        action=_env_arg("DTS_VERBOSE"),
        default="N",
        help="[DTS_VERBOSE] Set to 'Y' to enable verbose output, logging all messages "
        "to the console.",
    )

    parser.add_argument(
        "-s",
        "--skip-setup",
        action=_env_arg("DTS_SKIP_SETUP"),
        default="N",
        help="[DTS_SKIP_SETUP] Set to 'Y' to skip all setup steps on SUT and TG nodes.",
    )

    parser.add_argument(
        "--tarball",
        "--snapshot",
        "--git-ref",
        action=_env_arg("DTS_DPDK_TARBALL"),
        default="dpdk.tar.xz",
        type=Path,
        help="[DTS_DPDK_TARBALL] Path to DPDK source code tarball or a git commit ID, "
        "tag ID or tree ID to test. To test local changes, first commit them, "
        "then use the commit ID with this option.",
    )

    parser.add_argument(
        "--compile-timeout",
        action=_env_arg("DTS_COMPILE_TIMEOUT"),
        default=1200,
        type=float,
        help="[DTS_COMPILE_TIMEOUT] The timeout for compiling DPDK.",
    )

    parser.add_argument(
        "--test-cases",
        action=_env_arg("DTS_TESTCASES"),
        default="",
        help="[DTS_TESTCASES] Comma-separated list of test cases to execute. "
        "Unknown test cases will be silently ignored.",
    )

    parser.add_argument(
        "--re-run",
        "--re_run",
        action=_env_arg("DTS_RERUN"),
        default=0,
        type=int,
        help="[DTS_RERUN] Re-run each test case the specified amount of times "
        "if a test failure occurs",
    )

    return parser


def _get_settings() -> _Settings:
    parsed_args = _get_parser().parse_args()
    return _Settings(
        config_file_path=parsed_args.config_file,
        output_dir=parsed_args.output_dir,
        timeout=parsed_args.timeout,
        verbose=(parsed_args.verbose == "Y"),
        skip_setup=(parsed_args.skip_setup == "Y"),
        dpdk_tarball_path=Path(
            DPDKGitTarball(parsed_args.tarball, parsed_args.output_dir)
        )
        if not os.path.exists(parsed_args.tarball)
        else Path(parsed_args.tarball),
        compile_timeout=parsed_args.compile_timeout,
        test_cases=parsed_args.test_cases.split(",") if parsed_args.test_cases else [],
        re_run=parsed_args.re_run,
    )


SETTINGS: _Settings = _get_settings()
