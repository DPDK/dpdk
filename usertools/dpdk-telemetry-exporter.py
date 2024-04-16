#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 Robin Jarry

r'''
DPDK telemetry exporter.

It uses dynamically loaded endpoint exporters which are basic python files that
must implement two functions:

    def info() -> dict[MetricName, MetricInfo]:
        """
        Mapping of metric names to their description and type.
        """

    def metrics(sock: TelemetrySocket) -> list[MetricValue]:
        """
        Request data from sock and return it as metric values. A metric value
        is a 3-tuple: (name: str, value: any, labels: dict). Each name must be
        present in info().
        """

The sock argument passed to metrics() has a single method:

    def cmd(self, uri, arg=None) -> dict | list:
        """
        Request JSON data to the telemetry socket and parse it to python
        values.
        """

See existing endpoints for examples.

The exporter supports multiple output formats:

prometheus://ADDRESS:PORT
openmetrics://ADDRESS:PORT
  Expose the enabled endpoints via a local HTTP server listening on the
  specified address and port. GET requests on that server are served with
  text/plain responses in the prometheus/openmetrics format.

  More details:
  https://prometheus.io/docs/instrumenting/exposition_formats/#text-based-format

carbon://ADDRESS:PORT
graphite://ADDRESS:PORT
  Export all enabled endpoints to the specified TCP ADDRESS:PORT in the pickle
  carbon format.

  More details:
  https://graphite.readthedocs.io/en/latest/feeding-carbon.html#the-pickle-protocol
'''

import argparse
import importlib.util
import json
import logging
import os
import pickle
import re
import socket
import struct
import sys
import time
import typing
from http import HTTPStatus, server
from urllib.parse import urlparse

LOG = logging.getLogger(__name__)
# Use local endpoints path only when running from source
LOCAL = os.path.join(os.path.dirname(__file__), "telemetry-endpoints")
DEFAULT_LOAD_PATHS = []
if os.path.isdir(LOCAL):
    DEFAULT_LOAD_PATHS.append(LOCAL)
DEFAULT_LOAD_PATHS += [
    "/usr/local/share/dpdk/telemetry-endpoints",
    "/usr/share/dpdk/telemetry-endpoints",
]
DEFAULT_OUTPUT = "openmetrics://:9876"


def main():
    logging.basicConfig(
        stream=sys.stdout,
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "-o",
        "--output",
        metavar="FORMAT://PARAMETERS",
        default=urlparse(DEFAULT_OUTPUT),
        type=urlparse,
        help=f"""
        Output format (default: "{DEFAULT_OUTPUT}"). Depending on the format,
        URL elements have different meanings. By default, the exporter starts a
        local HTTP server on port 9876 that serves requests in the
        prometheus/openmetrics plain text format.
        """,
    )
    parser.add_argument(
        "-p",
        "--load-path",
        dest="load_paths",
        type=lambda v: v.split(os.pathsep),
        default=DEFAULT_LOAD_PATHS,
        help=f"""
        The list of paths from which to disvover endpoints.
        (default: "{os.pathsep.join(DEFAULT_LOAD_PATHS)}").
        """,
    )
    parser.add_argument(
        "-e",
        "--endpoint",
        dest="endpoints",
        metavar="ENDPOINT",
        action="append",
        help="""
        Telemetry endpoint to export (by default, all discovered endpoints are
        enabled). This option can be specified more than once.
        """,
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="""
        Only list detected endpoints and exit.
        """,
    )
    parser.add_argument(
        "-s",
        "--socket-path",
        default="/run/dpdk/rte/dpdk_telemetry.v2",
        help="""
        The DPDK telemetry socket path (default: "%(default)s").
        """,
    )
    args = parser.parse_args()
    output = OUTPUT_FORMATS.get(args.output.scheme)
    if output is None:
        parser.error(f"unsupported output format: {args.output.scheme}://")

    try:
        endpoints = load_endpoints(args.load_paths, args.endpoints)
        if args.list:
            return
    except Exception as e:
        parser.error(str(e))

    output(args, endpoints)


class TelemetrySocket:
    """
    Abstraction of the DPDK telemetry socket.
    """

    def __init__(self, path: str):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.sock.connect(path)
        data = json.loads(self.sock.recv(1024).decode())
        self.max_output_len = data["max_output_len"]

    def cmd(
        self, uri: str, arg: typing.Any = None
    ) -> typing.Optional[typing.Union[dict, list]]:
        """
        Request JSON data to the telemetry socket and parse it to python
        values.
        """
        if arg is not None:
            u = f"{uri},{arg}"
        else:
            u = uri
        self.sock.send(u.encode("utf-8"))
        data = self.sock.recv(self.max_output_len)
        return json.loads(data.decode("utf-8"))[uri]

    def __enter__(self):
        return self

    def __exit__(self, *args, **kwargs):
        self.sock.close()


MetricDescription = str
MetricType = str
MetricName = str
MetricLabels = typing.Dict[str, typing.Any]
MetricInfo = typing.Tuple[MetricDescription, MetricType]
MetricValue = typing.Tuple[MetricName, typing.Any, MetricLabels]


class TelemetryEndpoint:
    """
    Placeholder class only used for typing annotations.
    """

    @staticmethod
    def info() -> typing.Dict[MetricName, MetricInfo]:
        """
        Mapping of metric names to their description and type.
        """
        raise NotImplementedError()

    @staticmethod
    def metrics(sock: TelemetrySocket) -> typing.List[MetricValue]:
        """
        Request data from sock and return it as metric values. Each metric
        name must be present in info().
        """
        raise NotImplementedError()


def load_endpoints(
    paths: typing.List[str], names: typing.List[str]
) -> typing.List[TelemetryEndpoint]:
    """
    Load selected telemetry endpoints from the specified paths.
    """

    endpoints = {}
    dwb = sys.dont_write_bytecode
    sys.dont_write_bytecode = True  # never generate .pyc files for endpoints

    for p in paths:
        if not os.path.isdir(p):
            continue
        for fname in os.listdir(p):
            f = os.path.join(p, fname)
            if os.path.isdir(f):
                continue
            try:
                name, _ = os.path.splitext(fname)
                if names is not None and name not in names:
                    # not selected by user
                    continue
                if name in endpoints:
                    # endpoint with same name already loaded
                    continue
                spec = importlib.util.spec_from_file_location(name, f)
                module = importlib.util.module_from_spec(spec)
                spec.loader.exec_module(module)
                endpoints[name] = module
            except Exception:
                LOG.exception("parsing endpoint: %s", f)

    if not endpoints:
        raise Exception("no telemetry endpoints detected/selected")

    sys.dont_write_bytecode = dwb

    modules = []
    info = {}
    for name, module in sorted(endpoints.items()):
        LOG.info("using endpoint: %s (from %s)", name, module.__file__)
        try:
            for metric, (description, type_) in module.info().items():
                info[(name, metric)] = (description, type_)
            modules.append(module)
        except Exception:
            LOG.exception("getting endpoint info: %s", name)
    return modules


def serve_openmetrics(
    args: argparse.Namespace, endpoints: typing.List[TelemetryEndpoint]
):
    """
    Start an HTTP server and serve requests in the openmetrics/prometheus
    format.
    """
    listen = (args.output.hostname or "", int(args.output.port or 80))
    with server.HTTPServer(listen, OpenmetricsHandler) as httpd:
        httpd.dpdk_socket_path = args.socket_path
        httpd.telemetry_endpoints = endpoints
        LOG.info("listening on port %s", httpd.server_port)
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            LOG.info("shutting down")


class OpenmetricsHandler(server.BaseHTTPRequestHandler):
    """
    Basic HTTP handler that returns prometheus/openmetrics formatted responses.
    """

    CONTENT_TYPE = "text/plain; version=0.0.4; charset=utf-8"

    def escape(self, value: typing.Any) -> str:
        """
        Escape a metric label value.
        """
        value = str(value)
        value = value.replace('"', '\\"')
        value = value.replace("\\", "\\\\")
        return value.replace("\n", "\\n")

    def do_GET(self):
        """
        Called upon GET requests.
        """
        try:
            lines = []
            metrics_names = set()
            with TelemetrySocket(self.server.dpdk_socket_path) as sock:
                for e in self.server.telemetry_endpoints:
                    info = e.info()
                    metrics_lines = []
                    try:
                        metrics = e.metrics(sock)
                    except Exception:
                        LOG.exception("%s: metrics collection failed", e.__name__)
                        continue
                    for name, value, labels in metrics:
                        fullname = re.sub(r"\W", "_", f"dpdk_{e.__name__}_{name}")
                        labels = ", ".join(
                            f'{k}="{self.escape(v)}"' for k, v in labels.items()
                        )
                        if labels:
                            labels = f"{{{labels}}}"
                        metrics_lines.append(f"{fullname}{labels} {value}")
                        if fullname not in metrics_names:
                            metrics_names.add(fullname)
                            desc, metric_type = info[name]
                            lines += [
                                f"# HELP {fullname} {desc}",
                                f"# TYPE {fullname} {metric_type}",
                            ]
                    lines += metrics_lines
            if not lines:
                self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR)
                LOG.error(
                    "%s %s: no metrics collected",
                    self.address_string(),
                    self.requestline,
                )
            body = "\n".join(lines).encode("utf-8") + b"\n"
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", self.CONTENT_TYPE)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            LOG.info("%s %s", self.address_string(), self.requestline)

        except (FileNotFoundError, ConnectionRefusedError):
            self.send_error(HTTPStatus.SERVICE_UNAVAILABLE)
            LOG.exception(
                "%s %s: telemetry socket not available",
                self.address_string(),
                self.requestline,
            )
        except Exception:
            self.send_error(HTTPStatus.INTERNAL_SERVER_ERROR)
            LOG.exception("%s %s", self.address_string(), self.requestline)

    def log_message(self, fmt, *args):
        pass  # disable built-in logger


def export_carbon(args: argparse.Namespace, endpoints: typing.List[TelemetryEndpoint]):
    """
    Collect all metrics and export them to a carbon server in the pickle format.
    """
    addr = (args.output.hostname or "", int(args.output.port or 80))
    with TelemetrySocket(args.socket_path) as dpdk:
        with socket.socket() as carbon:
            carbon.connect(addr)
            all_metrics = []
            for e in endpoints:
                try:
                    metrics = e.metrics(dpdk)
                except Exception:
                    LOG.exception("%s: metrics collection failed", e.__name__)
                    continue
                for name, value, labels in metrics:
                    fullname = re.sub(r"\W", ".", f"dpdk.{e.__name__}.{name}")
                    for key, val in labels.items():
                        val = str(val).replace(";", "")
                        fullname += f";{key}={val}"
                    all_metrics.append((fullname, (time.time(), value)))
            if not all_metrics:
                raise Exception("no metrics collected")
            payload = pickle.dumps(all_metrics, protocol=2)
            header = struct.pack("!L", len(payload))
            buf = header + payload
            carbon.sendall(buf)


OUTPUT_FORMATS = {
    "openmetrics": serve_openmetrics,
    "prometheus": serve_openmetrics,
    "carbon": export_carbon,
    "graphite": export_carbon,
}


if __name__ == "__main__":
    main()
