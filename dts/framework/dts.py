# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2019 Intel Corporation
# Copyright(c) 2022 PANTHEON.tech s.r.o.
# Copyright(c) 2022 University of New Hampshire

import sys
import traceback
from collections.abc import Iterable

from framework.testbed_model.node import Node

from .config import CONFIGURATION
from .logger import DTSLOG, getLogger
from .utils import check_dts_python_version

dts_logger: DTSLOG | None = None


def run_all() -> None:
    """
    Main process of DTS, it will run all test suites in the config file.
    """

    global dts_logger

    # check the python version of the server that run dts
    check_dts_python_version()

    dts_logger = getLogger("dts")

    nodes = {}
    # This try/finally block means "Run the try block, if there is an exception,
    # run the finally block before passing it upward. If there is not an exception,
    # run the finally block after the try block is finished." This helps avoid the
    # problem of python's interpreter exit context, which essentially prevents you
    # from making certain system calls. This makes cleaning up resources difficult,
    # since most of the resources in DTS are network-based, which is restricted.
    try:
        # for all Execution sections
        for execution in CONFIGURATION.executions:
            sut_config = execution.system_under_test
            if sut_config.name not in nodes:
                node = Node(sut_config)
                nodes[sut_config.name] = node
                node.send_command("echo Hello World")

    except Exception as e:
        # sys.exit() doesn't produce a stack trace, need to print it explicitly
        traceback.print_exc()
        raise e

    finally:
        quit_execution(nodes.values())


def quit_execution(sut_nodes: Iterable[Node]) -> None:
    """
    Close session to SUT and TG before quit.
    Return exit status when failure occurred.
    """
    for sut_node in sut_nodes:
        # close all session
        sut_node.node_exit()

    if dts_logger is not None:
        dts_logger.info("DTS execution has ended.")
    sys.exit(0)
