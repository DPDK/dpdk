# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2021 Intel Corporation
# Copyright(c) 2022-2023 University of New Hampshire
# Copyright(c) 2023 PANTHEON.tech s.r.o.
# Copyright(c) 2024 Arm Limited

"""Testbed configuration and test suite specification.

This package offers classes that hold real-time information about the testbed, hold test run
configuration describing the tested testbed and a loader function, :func:`load_config`, which loads
the YAML configuration files and validates them against the :class:`Configuration` Pydantic
model, which fields are directly mapped.

The configuration files are split in:

    * A list of test run which are represented by :class:`~.test_run.TestRunConfiguration`
      defining what tests are going to be run and how DPDK will be built. It also references
      the testbed where these tests and DPDK are going to be run,
    * A list of the nodes of the testbed which ar represented by :class:`~.node.NodeConfiguration`.

The real-time information about testbed is supposed to be gathered at runtime.

The classes defined in this package make heavy use of :mod:`pydantic`.
Nearly all of them are frozen:

    * Frozen makes the object immutable. This enables further optimizations,
      and makes it thread safe should we ever want to move in that direction.
"""

from functools import cached_property
from pathlib import Path
from typing import Annotated, Any, Literal, NamedTuple, TypeVar, cast

import yaml
from pydantic import Field, TypeAdapter, ValidationError, model_validator
from typing_extensions import Self

from framework.exception import ConfigurationError

from .common import FrozenModel, ValidationContext
from .node import (
    NodeConfigurationTypes,
    SutNodeConfiguration,
    TGNodeConfiguration,
)
from .test_run import TestRunConfiguration


class TestRunWithNodesConfiguration(NamedTuple):
    """Tuple containing the configuration of the test run and its associated nodes."""

    #:
    test_run_config: TestRunConfiguration
    #:
    sut_node_config: SutNodeConfiguration
    #:
    tg_node_config: TGNodeConfiguration


TestRunsConfig = Annotated[list[TestRunConfiguration], Field(min_length=1)]

NodesConfig = Annotated[list[NodeConfigurationTypes], Field(min_length=1)]


class Configuration(FrozenModel):
    """DTS testbed and test configuration."""

    #: Test run configurations.
    test_runs: TestRunsConfig
    #: Node configurations.
    nodes: NodesConfig

    @cached_property
    def test_runs_with_nodes(self) -> list[TestRunWithNodesConfiguration]:
        """List of test runs with the associated nodes."""
        test_runs_with_nodes = []

        for test_run_no, test_run in enumerate(self.test_runs):
            sut_node_name = test_run.system_under_test_node
            sut_node = next(filter(lambda n: n.name == sut_node_name, self.nodes), None)

            assert sut_node is not None, (
                f"test_runs.{test_run_no}.sut_node_config.node_name "
                f"({test_run.system_under_test_node}) is not a valid node name"
            )
            assert isinstance(sut_node, SutNodeConfiguration), (
                f"test_runs.{test_run_no}.sut_node_config.node_name is a valid node name, "
                "but it is not a valid SUT node"
            )

            tg_node_name = test_run.traffic_generator_node
            tg_node = next(filter(lambda n: n.name == tg_node_name, self.nodes), None)

            assert tg_node is not None, (
                f"test_runs.{test_run_no}.tg_node_name "
                f"({test_run.traffic_generator_node}) is not a valid node name"
            )
            assert isinstance(tg_node, TGNodeConfiguration), (
                f"test_runs.{test_run_no}.tg_node_name is a valid node name, "
                "but it is not a valid TG node"
            )

            test_runs_with_nodes.append(TestRunWithNodesConfiguration(test_run, sut_node, tg_node))

        return test_runs_with_nodes

    @model_validator(mode="after")
    def validate_node_names(self) -> Self:
        """Validate that the node names are unique."""
        nodes_by_name: dict[str, int] = {}
        for node_no, node in enumerate(self.nodes):
            assert node.name not in nodes_by_name, (
                f"node {node_no} cannot have the same name as node {nodes_by_name[node.name]} "
                f"({node.name})"
            )
            nodes_by_name[node.name] = node_no

        return self

    @model_validator(mode="after")
    def validate_ports(self) -> Self:
        """Validate that the ports are all linked to valid ones."""
        port_links: dict[tuple[str, str], Literal[False] | tuple[int, int]] = {
            (node.name, port.pci): False for node in self.nodes for port in node.ports
        }

        for node_no, node in enumerate(self.nodes):
            for port_no, port in enumerate(node.ports):
                peer_port_identifier = (port.peer_node, port.peer_pci)
                peer_port = port_links.get(peer_port_identifier, None)
                assert (
                    peer_port is not None
                ), f"invalid peer port specified for nodes.{node_no}.ports.{port_no}"
                assert peer_port is False, (
                    f"the peer port specified for nodes.{node_no}.ports.{port_no} "
                    f"is already linked to nodes.{peer_port[0]}.ports.{peer_port[1]}"
                )
                port_links[peer_port_identifier] = (node_no, port_no)

        return self

    @model_validator(mode="after")
    def validate_test_runs_with_nodes(self) -> Self:
        """Validate the test runs to nodes associations.

        This validator relies on the cached property `test_runs_with_nodes` to run for the first
        time in this call, therefore triggering the assertions if needed.
        """
        if self.test_runs_with_nodes:
            pass
        return self


T = TypeVar("T")


def _load_and_parse_model(file_path: Path, model_type: T, ctx: ValidationContext) -> T:
    with open(file_path) as f:
        try:
            data = yaml.safe_load(f)
            return TypeAdapter(model_type).validate_python(data, context=cast(dict[str, Any], ctx))
        except ValidationError as e:
            msg = f"failed to load the configuration file {file_path}"
            raise ConfigurationError(msg) from e


def load_config(ctx: ValidationContext) -> Configuration:
    """Load the DTS configuration from files.

    Load the YAML configuration files, validate them, and create a configuration object.

    Args:
        ctx: The context required for validation.

    Returns:
        The parsed test run configuration.

    Raises:
        ConfigurationError: If the supplied configuration files are invalid.
    """
    test_runs = _load_and_parse_model(ctx["settings"].test_runs_config_path, TestRunsConfig, ctx)
    nodes = _load_and_parse_model(ctx["settings"].nodes_config_path, NodesConfig, ctx)

    try:
        return Configuration.model_validate({"test_runs": test_runs, "nodes": nodes}, context=ctx)
    except ValidationError as e:
        raise ConfigurationError("the configurations supplied are invalid") from e
