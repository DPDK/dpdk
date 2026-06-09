#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

"""
Script to be used with V2 Telemetry.
Allows the user input commands and read the Telemetry response.
"""

import socket
import os
import sys
import glob
import json
import errno
import readline
import argparse

# global vars
TELEMETRY_VERSION = "v2"
SOCKET_NAME = "dpdk_telemetry.{}".format(TELEMETRY_VERSION)
DEFAULT_PREFIX = "rte"
CMDS = []


def send_command(sock, cmd, output_buf_len, echo=False, pretty=False):
    """Send a telemetry command and return the parsed JSON reply"""
    sock.send(cmd.encode())
    return read_socket(sock, output_buf_len, echo, pretty)


def get_cmd_payload(reply, cmd):
    """Return the payload for a command response if present"""
    if isinstance(reply, dict) and len(reply) == 1:
        return next(iter(reply.values()))
    return None


def get_path_value(payload, path):
    """Resolve a dotted path (e.g. '.name' or '.a.b') from a JSON payload"""
    if not path:
        return payload

    keys = [k for k in path.lstrip(".").split(".") if k]
    val = payload
    for key in keys:
        if not isinstance(val, dict) or key not in val:
            return None
        val = val[key]
    return val


def parse_selectors(selector_text):
    """Parse whitespace-separated dotted selectors"""
    selectors = selector_text.split()
    if not selectors:
        print("Invalid FOREACH syntax: missing selector")
        return None
    if any(not selector.startswith(".") for selector in selectors):
        print("Invalid FOREACH syntax: selector must start with '.'")
        return None
    return selectors


def parse_foreach(text):
    """Parse FOREACH [<var>] /<cmd> /<parameterized cmd> .<value> [.<value> ...]"""
    try:
        tokens = text.split(None, 3)
    except ValueError:
        print("Invalid FOREACH syntax")
        return None

    if len(tokens) != 4:
        print("Invalid FOREACH syntax")
        return None

    _, arg1, arg2, arg3 = tokens
    if arg1.startswith("/"):
        var_name = None
        list_cmd = arg1
        iter_cmd = arg2
        selector_text = arg3
    else:
        var_name = arg1
        list_cmd = arg2
        try:
            iter_cmd, selector_text = arg3.split(None, 1)
        except ValueError:
            print("Invalid FOREACH syntax")
            return None

    if not list_cmd.startswith("/") or not iter_cmd.startswith("/"):
        print("Invalid FOREACH syntax: commands must start with '/'")
        return None

    selectors = parse_selectors(selector_text)
    if selectors is None:
        return None

    return var_name, list_cmd, iter_cmd, selectors


def build_foreach_result(item, var_name, payload, selectors):
    """Build one FOREACH result entry based on selector count and index mode"""
    values = {selector.lstrip("."): get_path_value(payload, selector) for selector in selectors}

    if var_name is None and len(selectors) == 1:
        return next(iter(values.values()))
    if var_name is None:
        return values

    return {var_name: item, **values}


def handle_foreach(sock, output_buf_len, text, pretty=False):
    """Handle FOREACH queries and print telemetry-like JSON array output"""
    parsed = parse_foreach(text)
    if parsed is None:
        return
    var_name, list_cmd, iter_cmd, selectors = parsed

    list_reply = send_command(sock, list_cmd, output_buf_len)
    values = get_cmd_payload(list_reply, list_cmd)
    if not isinstance(values, list):
        print("FOREACH source command did not return a JSON array")
        return

    output = []
    for item in values:
        if var_name is None:
            cmd = "{},{}".format(iter_cmd, item)
        else:
            cmd = iter_cmd.replace("$" + var_name, str(item))
        item_reply = send_command(sock, cmd, output_buf_len)
        item_payload = get_cmd_payload(item_reply, cmd)
        output.append(build_foreach_result(item, var_name, item_payload, selectors))

    indent = 2 if pretty else None
    print(json.dumps(output, indent=indent))


def handle_command(sock, output_buf_len, text, pretty=False):
    """Execute a user command if recognized"""
    if text.startswith("/"):
        send_command(sock, text, output_buf_len, echo=True, pretty=pretty)
    elif text.startswith("FOREACH "):
        handle_foreach(sock, output_buf_len, text, pretty)


def read_socket(sock, buf_len, echo=True, pretty=False):
    """Read data from socket and return it in JSON format"""
    reply = sock.recv(buf_len).decode()
    try:
        ret = json.loads(reply)
    except json.JSONDecodeError:
        print("Error in reply: ", reply)
        sock.close()
        raise
    if echo:
        indent = 2 if pretty else None
        print(json.dumps(ret, indent=indent))
    return ret


def get_app_name(pid):
    """return the app name for a given PID, for printing"""
    proc_cmdline = os.path.join("/proc", str(pid), "cmdline")
    try:
        with open(proc_cmdline) as f:
            argv0 = f.read(1024).split("\0")[0]
            return os.path.basename(argv0)
    except IOError as e:
        # ignore file not found errors
        if e.errno != errno.ENOENT:
            raise
    return None


def find_sockets(path):
    """Find any possible sockets to connect to and return them"""
    return glob.glob(os.path.join(path, SOCKET_NAME + "*"))


def print_socket_options(prefix, paths):
    """Given a set of socket paths, give the commands needed to connect"""
    cmd = sys.argv[0]
    if prefix != DEFAULT_PREFIX:
        cmd += " -f " + prefix
    for s in sorted(paths):
        sock_name = os.path.basename(s)
        if sock_name.endswith(TELEMETRY_VERSION):
            print("- {}  # Connect with '{}'".format(os.path.basename(s), cmd))
        else:
            print(
                "- {}  # Connect with '{} -i {}'".format(os.path.basename(s), cmd, s.split(":")[-1])
            )


def get_dpdk_runtime_dir(fp):
    """Using the same logic as in DPDK's EAL, get the DPDK runtime directory
    based on the file-prefix and user"""
    run_dir = os.environ.get("RUNTIME_DIRECTORY")
    if not run_dir:
        if os.getuid() == 0:
            run_dir = "/var/run"
        else:
            run_dir = os.environ.get("XDG_RUNTIME_DIR", "/tmp")
    return os.path.join(run_dir, "dpdk", fp)


def list_fp():
    """List all available file-prefixes to user"""
    path = get_dpdk_runtime_dir("")
    sockets = glob.glob(os.path.join(path, "*", SOCKET_NAME + "*"))
    prefixes = []
    if not sockets:
        print("No DPDK apps with telemetry enabled available")
    else:
        print("Valid file-prefixes:\n")
    for s in sockets:
        prefixes.append(os.path.relpath(os.path.dirname(s), start=path))
    for p in sorted(set(prefixes)):
        print(p)
        print_socket_options(p, glob.glob(os.path.join(path, p, SOCKET_NAME + "*")))


def handle_socket(args, path):
    """Connect to socket and handle user input"""
    prompt = ""  # this evaluates to false in conditions
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    global CMDS

    if os.isatty(sys.stdin.fileno()):
        prompt = "--> "
        print("Connecting to " + path)
    try:
        sock.connect(path)
    except OSError:
        print("Error connecting to " + path)
        sock.close()
        # if socket exists but is bad, or if non-interactive just return
        if os.path.exists(path) or not prompt:
            return
        # if user didn't give a valid socket path, but there are
        # some sockets, help the user out by printing how to connect
        socks = find_sockets(os.path.dirname(path))
        if socks:
            print("\nOther DPDK telemetry sockets found:")
            print_socket_options(args.file_prefix, socks)
        else:
            list_fp()
        return
    json_reply = read_socket(sock, 1024, prompt, prompt)
    output_buf_len = json_reply["max_output_len"]
    app_name = get_app_name(json_reply["pid"])
    if app_name and prompt:
        print('Connected to application: "%s"' % app_name)

    # get list of commands for readline completion
    sock.send("/".encode())
    CMDS = read_socket(sock, output_buf_len, False)["/"]

    # interactive prompt
    try:
        text = input(prompt).strip()
        while text != "quit":
            handle_command(sock, output_buf_len, text, pretty=prompt)
            text = input(prompt).strip()
    except EOFError:
        pass
    finally:
        sock.close()


def readline_complete(text, state):
    """Find any matching commands from the list based on user input"""
    all_cmds = ["quit"] + CMDS
    if text:
        matches = [c for c in all_cmds if c.startswith(text)]
    else:
        matches = all_cmds
    return matches[state]


readline.parse_and_bind("tab: complete")
readline.set_completer(readline_complete)
readline.set_completer_delims(readline.get_completer_delims().replace("/", ""))

parser = argparse.ArgumentParser()
parser.add_argument(
    "-f",
    "--file-prefix",
    default=DEFAULT_PREFIX,
    help="Provide file-prefix for DPDK runtime directory",
)
parser.add_argument(
    "-i", "--instance", default="0", type=int, help="Provide instance number for DPDK application"
)
parser.add_argument(
    "-l",
    "--list",
    action="store_true",
    default=False,
    help="List all possible file-prefixes and exit",
)
args = parser.parse_args()
if args.list:
    list_fp()
    sys.exit(0)
sock_path = os.path.join(get_dpdk_runtime_dir(args.file_prefix), SOCKET_NAME)
if args.instance > 0:
    sock_path += ":{}".format(args.instance)
handle_socket(args, sock_path)
