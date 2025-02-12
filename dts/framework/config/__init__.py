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

from pathlib import Path
from typing import Annotated, Any, Literal, TypeVar, cast

import yaml
from pydantic import Field, TypeAdapter, ValidationError, model_validator
from typing_extensions import Self

from framework.exception import ConfigurationError

from .common import FrozenModel, ValidationContext
from .node import NodeConfiguration
from .test_run import TestRunConfiguration

TestRunsConfig = Annotated[list[TestRunConfiguration], Field(min_length=1)]

NodesConfig = Annotated[list[NodeConfiguration], Field(min_length=1)]


class Configuration(FrozenModel):
    """DTS testbed and test configuration."""

    #: Test run configurations.
    test_runs: TestRunsConfig
    #: Node configurations.
    nodes: NodesConfig

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
    def validate_port_links(self) -> Self:
        """Validate that all the test runs' port links are valid."""
        existing_port_links: dict[tuple[str, str], Literal[False] | tuple[str, str]] = {
            (node.name, port.name): False for node in self.nodes for port in node.ports
        }

        defined_port_links = [
            (test_run_idx, test_run, link_idx, link)
            for test_run_idx, test_run in enumerate(self.test_runs)
            for link_idx, link in enumerate(test_run.port_topology)
        ]
        for test_run_idx, test_run, link_idx, link in defined_port_links:
            sut_node_port_peer = existing_port_links.get(
                (test_run.system_under_test_node, link.sut_port), None
            )
            assert sut_node_port_peer is not None, (
                "Invalid SUT node port specified for link "
                f"test_runs.{test_run_idx}.port_topology.{link_idx}."
            )

            assert sut_node_port_peer is False or sut_node_port_peer == link.right, (
                f"The SUT node port for link test_runs.{test_run_idx}.port_topology.{link_idx} is "
                f"already linked to port {sut_node_port_peer[0]}.{sut_node_port_peer[1]}."
            )

            tg_node_port_peer = existing_port_links.get(
                (test_run.traffic_generator_node, link.tg_port), None
            )
            assert tg_node_port_peer is not None, (
                "Invalid TG node port specified for link "
                f"test_runs.{test_run_idx}.port_topology.{link_idx}."
            )

            assert tg_node_port_peer is False or sut_node_port_peer == link.left, (
                f"The TG node port for link test_runs.{test_run_idx}.port_topology.{link_idx} is "
                f"already linked to port {tg_node_port_peer[0]}.{tg_node_port_peer[1]}."
            )

            existing_port_links[link.left] = link.right
            existing_port_links[link.right] = link.left

        return self

    @model_validator(mode="after")
    def validate_test_runs_against_nodes(self) -> Self:
        """Validate the test runs to nodes associations."""
        for test_run_no, test_run in enumerate(self.test_runs):
            sut_node_name = test_run.system_under_test_node
            sut_node = next((n for n in self.nodes if n.name == sut_node_name), None)

            assert sut_node is not None, (
                f"Test run {test_run_no}.system_under_test_node "
                f"({sut_node_name}) is not a valid node name."
            )

            tg_node_name = test_run.traffic_generator_node
            tg_node = next((n for n in self.nodes if n.name == tg_node_name), None)

            assert tg_node is not None, (
                f"Test run {test_run_no}.traffic_generator_name "
                f"({tg_node_name}) is not a valid node name."
            )

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
