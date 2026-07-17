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
ALIASES = {}
ALIAS_FILE = "telemetry_aliases"
MAX_ALIAS_EXPANSIONS = 32

BASIC_HELP_TEXT = """Basic usage:
    /<command>[,<params>]      Send a telemetry command to the app
    FOREACH ...                Run a compound query over list items
    help                       Show this help
    help /<command>            Show app-provided help for a command
    help FOREACH               Show FOREACH usage and examples
    quit                       Exit the client
"""

FOREACH_HELP_TEXT = """FOREACH usage:
    FOREACH /<list_cmd> /<iter_cmd> .<field> [.<field> ...]
    FOREACH <var> /<list_cmd> /<iter_cmd_with_$var> .<field> [.<field> ...]

Examples:
    FOREACH /ethdev/list /ethdev/stats .opackets
    FOREACH /ethdev/list /ethdev/stats .ipackets .opackets
    FOREACH i /ethdev/list /ethdev/info,$i .name
    FOREACH i /ethdev/list /ethdev/stats,$i .ipackets .opackets
"""


def get_default_alias_path():
    config_home = os.environ.get("XDG_CONFIG_HOME")
    if config_home:
        return os.path.join(config_home, "dpdk", ALIAS_FILE)

    home = os.environ.get("HOME")
    if not home:
        return None

    return os.path.join(home, ".config", "dpdk", ALIAS_FILE)


def load_aliases(alias_path=None):
    """Load aliases from the config directory or a custom path if provided"""
    aliases = {}
    if alias_path and not os.path.isfile(alias_path):
        print("Warning: alias file {} not found, skipping".format(alias_path), file=sys.stderr)
        return aliases

    if not alias_path:
        alias_path = get_default_alias_path()
        if not alias_path:
            return aliases

        if not os.path.isfile(alias_path):
            return aliases

    try:
        with open(alias_path) as alias_file:
            for line_num, line in enumerate(alias_file, start=1):
                entry = line.strip()
                if not entry or entry.startswith("#"):
                    continue
                if "=" not in entry:
                    print(
                        "Warning: ignoring malformed alias at {}:{}".format(alias_path, line_num),
                        file=sys.stderr,
                    )
                    continue
                name, command = entry.split("=", 1)
                name = name.strip()
                command = command.strip()
                if not name or not command:
                    print(
                        "Warning: ignoring malformed alias at {}:{}".format(alias_path, line_num),
                        file=sys.stderr,
                    )
                    continue
                aliases[name] = command
    except OSError as e:
        print("Warning: failed to read {}: {}".format(alias_path, e), file=sys.stderr)

    print("Loaded {} aliases from {}".format(len(aliases), alias_path))
    return aliases


def expand_aliases(text, aliases):
    """Expand aliases similarly to shell aliases on the first token"""
    expanded = text
    for _ in range(MAX_ALIAS_EXPANSIONS):
        stripped = expanded.lstrip()
        if not stripped:
            return expanded

        parts = stripped.split(None, 1)
        first = parts[0]
        rest = parts[1] if len(parts) > 1 else ""

        if first not in aliases:
            return expanded

        alias_value = aliases[first]
        expanded = "{} {}".format(alias_value, rest).strip() if rest else alias_value

    print("Warning: alias expansion limit reached", file=sys.stderr)
    return expanded


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


def command_exists(cmd):
    """Check if a telemetry command exists in the command list"""
    return cmd in CMDS


def app_help_command_for(target_cmd):
    """Build a '/help,<command>' query for app-side command help"""
    if not target_cmd:
        return None
    normalized = target_cmd.strip()
    if not normalized.startswith("/"):
        return None
    if not command_exists(normalized):
        print("Unknown command for help: {}".format(normalized))
        return None
    return "/help,{}".format(normalized)


def handle_user_help(sock, output_buf_len, text, pretty=False):
    """Handle local 'help' command and command-specific help lookup"""
    parts = text.split(None, 1)
    if len(parts) == 1:
        print(BASIC_HELP_TEXT, end="")
        return

    help_arg = parts[1].strip()
    if help_arg.upper() == "FOREACH":
        print(FOREACH_HELP_TEXT, end="")
        return
    elif help_arg.lower() == "alias" or help_arg.lower() == "aliases":
        if not ALIASES:
            print("No aliases defined")
            return
        print("Defined aliases:")
        for name, command in ALIASES.items():
            print(f"  {name}='{command}'")
        return

    cmd = app_help_command_for(help_arg)
    if cmd is None:
        print("Usage: help [FOREACH|/<command>]")
        return
    send_command(sock, cmd, output_buf_len, echo=True, pretty=pretty)


def handle_command(sock, output_buf_len, text, pretty=False):
    """Execute a user command if recognized"""
    if text.startswith("/"):
        send_command(sock, text, output_buf_len, echo=True, pretty=pretty)
    elif text == "help" or text.startswith("help "):
        handle_user_help(sock, output_buf_len, text, pretty)
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
        while True:
            text = input(prompt).strip()
            expanded = expand_aliases(text, ALIASES)
            if expanded == "quit":
                break
            handle_command(sock, output_buf_len, expanded, pretty=prompt)
    except EOFError:
        pass
    finally:
        sock.close()


def readline_complete(text, state):
    """Find any matching commands from the list based on user input"""
    all_cmds = ["quit", "help"] + list(ALIASES.keys()) + CMDS
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
    "--alias-file",
    help="Provide a custom alias file instead of the default config path",
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
ALIASES = load_aliases(args.alias_file if args.alias_file else None)
if args.list:
    list_fp()
    sys.exit(0)
sock_path = os.path.join(get_dpdk_runtime_dir(args.file_prefix), SOCKET_NAME)
if args.instance > 0:
    sock_path += ":{}".format(args.instance)
handle_socket(args, sock_path)
