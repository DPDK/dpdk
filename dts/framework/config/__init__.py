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

    * The test run which is represented by :class:`~.test_run.TestRunConfiguration`
      defining what tests are going to be run and how DPDK will be built. It also references
      the testbed where these tests and DPDK are going to be run,
    * A list of the nodes of the testbed which ar represented by :class:`~.node.NodeConfiguration`.
    * A dictionary mapping test suite names to their corresponding configurations.

The real-time information about testbed is supposed to be gathered at runtime.

The classes defined in this package make heavy use of :mod:`pydantic`.
Nearly all of them are frozen:

    * Frozen makes the object immutable. This enables further optimizations,
      and makes it thread safe should we ever want to move in that direction.
"""

from pathlib import Path
from typing import TYPE_CHECKING, Annotated, Any, Literal, TypeVar, cast

import yaml
from pydantic import Field, TypeAdapter, ValidationError, model_validator
from typing_extensions import Self

from framework.exception import ConfigurationError

from .common import FrozenModel, ValidationContext
from .node import NodeConfiguration
from .test_run import TestRunConfiguration, create_test_suites_config_model

# Import only if type checking or building docs, to prevent circular imports.
if TYPE_CHECKING:
    from framework.test_suite import BaseConfig

NodesConfig = Annotated[list[NodeConfiguration], Field(min_length=1)]


class Configuration(FrozenModel):
    """DTS testbed and test configuration."""

    #: Test run configuration.
    test_run: TestRunConfiguration
    #: Node configurations.
    nodes: NodesConfig
    #: Test suites custom configurations.
    tests_config: dict[str, "BaseConfig"]

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
        """Validate that all of the test run's port links are valid."""
        existing_port_links: dict[tuple[str, str], Literal[False] | tuple[str, str]] = {
            (node.name, port.name): False for node in self.nodes for port in node.ports
        }

        defined_port_links = [
            (link_idx, link) for link_idx, link in enumerate(self.test_run.port_topology)
        ]
        for link_idx, link in defined_port_links:
            sut_node_port_peer = existing_port_links.get(
                (self.test_run.system_under_test_node, link.sut_port), None
            )
            assert (
                sut_node_port_peer is not None
            ), f"Invalid SUT node port specified for link port_topology.{link_idx}."

            assert sut_node_port_peer is False or sut_node_port_peer == link.right, (
                f"The SUT node port for link port_topology.{link_idx} is "
                f"already linked to port {sut_node_port_peer[0]}.{sut_node_port_peer[1]}."
            )

            tg_node_port_peer = existing_port_links.get(
                (self.test_run.traffic_generator_node, link.tg_port), None
            )
            assert (
                tg_node_port_peer is not None
            ), f"Invalid TG node port specified for link port_topology.{link_idx}."

            assert tg_node_port_peer is False or sut_node_port_peer == link.left, (
                f"The TG node port for link port_topology.{link_idx} is "
                f"already linked to port {tg_node_port_peer[0]}.{tg_node_port_peer[1]}."
            )

            existing_port_links[link.left] = link.right
            existing_port_links[link.right] = link.left

        return self

    @model_validator(mode="after")
    def validate_test_run_against_nodes(self) -> Self:
        """Validate the test run against the supplied nodes."""
        sut_node_name = self.test_run.system_under_test_node
        sut_node = next((n for n in self.nodes if n.name == sut_node_name), None)

        assert (
            sut_node is not None
        ), f"The system_under_test_node {sut_node_name} is not a valid node name."

        tg_node_name = self.test_run.traffic_generator_node
        tg_node = next((n for n in self.nodes if n.name == tg_node_name), None)

        assert (
            tg_node is not None
        ), f"The traffic_generator_name {tg_node_name} is not a valid node name."

        return self


T = TypeVar("T")


def _load_and_parse_model(file_path: Path, model_type: type[T], ctx: ValidationContext) -> T:
    with open(file_path) as f:
        try:
            data = yaml.safe_load(f)
            return TypeAdapter(model_type).validate_python(data, context=cast(dict[str, Any], ctx))
        except ValidationError as e:
            msg = f"Failed to load the configuration file {file_path}."
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
    test_run = _load_and_parse_model(
        ctx["settings"].test_run_config_path, TestRunConfiguration, ctx
    )

    TestSuitesConfiguration = create_test_suites_config_model(test_run.test_suites)
    if ctx["settings"].tests_config_path:
        tests_config = _load_and_parse_model(
            ctx["settings"].tests_config_path,
            TestSuitesConfiguration,
            ctx,
        )
    else:
        try:
            tests_config = TestSuitesConfiguration()
        except ValidationError as e:
            raise ConfigurationError(
                "A test suites' configuration file is required for the given test run. "
                "The following selected test suites require manual configuration: "
                + ", ".join(str(error["loc"][0]) for error in e.errors())
            )

    nodes = _load_and_parse_model(ctx["settings"].nodes_config_path, NodesConfig, ctx)

    try:
        from framework.test_suite import BaseConfig as BaseConfig

        Configuration.model_rebuild()
        return Configuration.model_validate(
            {"test_run": test_run, "nodes": nodes, "tests_config": dict(tests_config)}, context=ctx
        )
    except ValidationError as e:
        raise ConfigurationError("The configurations supplied are invalid.") from e
