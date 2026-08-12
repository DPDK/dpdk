#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Red Hat, Inc.

"""Exercise every telemetry command exported by an application.

Spawns the DPDK test binary (passed as arguments), waits for its telemetry
socket to appear, then walks every command reported by "/". Every reply is
parsed as JSON and checked, so a malformed, empty or missing response fails
the test immediately and names the offending command.

Commands are called with dummy parameters ("", "0" and "z"). In addition,
any node of the form /*dev/list is queried first and the identifiers it
returns are passed to the other nodes in the same namespace (/*dev/info,
/*dev/stats, /*dev/xstats, ...), so those are exercised with real arguments
rather than only rejecting garbage. The dummy parameters remain the fallback
for anything not matching that pattern.

A single connection is reused for the whole walk: the previous shell version
spawned a fresh dpdk-telemetry.py (Python interpreter + new connection) per
command, which scaled with process-startup cost and timed out under load.
"""

import json
import os
import socket
import subprocess
import sys
import time

SOCKET_NAME = "dpdk_telemetry.v2"
DUMMY_ARGS = ("", ",0", ",z")
LIST_SUFFIX = "/list"


def runtime_dir():
    """DPDK runtime dir for the default 'rte' file-prefix, matching EAL."""
    run = os.environ.get("RUNTIME_DIRECTORY")
    if not run:
        run = (
            "/var/run"
            if os.getuid() == 0
            else os.environ.get("XDG_RUNTIME_DIR", "/tmp")
        )
    return os.path.join(run, "dpdk", "rte")


def wait_for_socket(path, proc, timeout=10):
    """Wait for the telemetry socket, failing fast if the app dies first."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        if os.path.exists(path):
            return
        if proc.poll() is not None:
            raise RuntimeError(
                "application exited (code %d) before telemetry socket appeared"
                % proc.returncode
            )
        time.sleep(0.05)
    raise RuntimeError("timed out waiting for telemetry socket %s" % path)


class TelemetryClient:
    def __init__(self, path):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.sock.connect(path)
        info = json.loads(self.sock.recv(1024))
        self.buf_len = info["max_output_len"]

    def command(self, cmd):
        self.sock.send(cmd.encode())
        reply = self.sock.recv(self.buf_len).decode()
        try:
            return json.loads(reply)
        except json.JSONDecodeError as e:
            raise AssertionError(
                "invalid JSON reply for %r: %s (raw: %r)" % (cmd, e, reply)
            )

    def close(self):
        self.sock.close()


def check_reply(cmd, reply):
    """A telemetry reply must be a dict keyed by the command name."""
    if not isinstance(reply, dict) or list(reply.keys()) != [cmd.split(",")[0]]:
        raise AssertionError("unexpected reply for %r: %r" % (cmd, reply))


def device_ids(client, listing):
    """Map namespace to identifiers, from the /*dev/list nodes.

    /ethdev/list, /cryptodev/list and friends take no parameter and return
    the identifiers accepted by the other nodes under the same namespace.
    """
    devices = {}
    for cmd in listing:
        if not cmd.endswith(LIST_SUFFIX):
            continue
        namespace = cmd[: -len(LIST_SUFFIX)]
        if not namespace.endswith("dev"):
            continue
        reply = client.command(cmd)
        check_reply(cmd, reply)
        ids = reply[cmd]
        if isinstance(ids, list):
            devices[namespace] = ids
    return devices


def args_for(cmd, devices):
    """Parameters to try for a command: real identifiers where known."""
    if cmd.endswith(LIST_SUFFIX):
        return DUMMY_ARGS
    for namespace, ids in devices.items():
        if cmd.startswith(namespace + "/"):
            return DUMMY_ARGS + tuple("," + str(i) for i in ids)
    return DUMMY_ARGS


def walk(client):
    listing = client.command("/")
    check_reply("/", listing)
    devices = device_ids(client, listing["/"])
    count = 0
    for cmd in listing["/"]:
        for arg in args_for(cmd, devices):
            full = cmd + arg
            reply = client.command(full)
            check_reply(full, reply)
            count += 1
    return count


def main():
    if len(sys.argv) < 2:
        print("usage: %s <dpdk-app> [eal args...]" % sys.argv[0], file=sys.stderr)
        return 1

    sock_path = os.path.join(runtime_dir(), SOCKET_NAME)
    proc = subprocess.Popen(sys.argv[1:], stdin=subprocess.PIPE)
    try:
        wait_for_socket(sock_path, proc)
        client = TelemetryClient(sock_path)
        try:
            count = walk(client)
        finally:
            client.close()
        print("telemetry: walked %d commands" % count)
    finally:
        # tell the interactive prompt to exit, then ensure the app is gone
        try:
            proc.stdin.write(b"quit\n")
            proc.stdin.flush()
            proc.stdin.close()
        except (BrokenPipeError, OSError):
            pass
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.terminate()
            proc.wait()

    return 0 if proc.returncode == 0 else proc.returncode


if __name__ == "__main__":
    sys.exit(main())
