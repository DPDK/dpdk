#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2025 Intel Corporation

"""
Script to monitor DPDK telemetry statistics on the command line.
Wraps dpdk-telemetry.py to provide continuous monitoring capabilities.
"""

import argparse
import subprocess
import sys
import os
import shutil
import errno
import json


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


def find_telemetry_script():
    """Find the dpdk-telemetry.py script in the script directory or PATH.

    Returns:
        str: Path to the dpdk-telemetry.py script

    Exits:
        If the script cannot be found
    """
    # First, try to find it in the same directory as this script
    script_dir = os.path.dirname(os.path.abspath(__file__))
    telemetry_script = os.path.join(script_dir, "dpdk-telemetry.py")

    # If not found locally, check if it's in PATH
    if not os.path.exists(telemetry_script):
        telemetry_in_path = shutil.which("dpdk-telemetry.py")
        if telemetry_in_path:
            telemetry_script = telemetry_in_path
        else:
            print(
                "Error: dpdk-telemetry.py not found in script directory or PATH",
                file=sys.stderr,
            )
            sys.exit(1)

    return telemetry_script


def create_telemetry_process(telemetry_script, args_list):
    """Create a subprocess for dpdk-telemetry.py with pipes.

    Args:
        telemetry_script: Path to the dpdk-telemetry.py script
        args_list: List of arguments to pass to the script

    Returns:
        subprocess.Popen: Process handle with stdin/stdout/stderr pipes

    Exits:
        If the process cannot be created
    """
    # Build the command
    cmd = [sys.executable, telemetry_script] + args_list

    try:
        process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,  # Line buffered
        )
        return process
    except FileNotFoundError:
        print("Error: Python interpreter or script not found", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error running dpdk-telemetry.py: {e}", file=sys.stderr)
        sys.exit(1)


def query_telemetry(process, command):
    """Send a telemetry command and return the parsed JSON response.

    Args:
        process: The subprocess.Popen handle to the telemetry process
        command: The telemetry command to send (e.g., "/info" or "/ethdev/stats,0")

    Returns:
        dict: The parsed JSON response with the command wrapper stripped,
              or None if there was an error
    """
    # Send the command
    process.stdin.write(f"{command}\n")
    process.stdin.flush()

    # Read the JSON response
    response = process.stdout.readline()
    try:
        data = json.loads(response)
        # When run non-interactively, the response is wrapped with the command
        # e.g., {"/info": {"version": ..., "pid": ...}}
        # or {"/ethdev/stats,0": {...}}
        # The response should have exactly one key which is the command
        if len(data) == 1:
            # Extract the value, ignoring the key
            return next(iter(data.values()))
        else:
            return data
    except (json.JSONDecodeError, KeyError):
        return None


def print_connected_app(process):
    """Query and print the name of the connected DPDK application.

    Args:
        process: The subprocess.Popen handle to the telemetry process
    """
    info = query_telemetry(process, "/info")
    if info and "pid" in info:
        app_name = get_app_name(info["pid"])
        if app_name:
            print(f'Connected to application: "{app_name}"')


def main():
    """Main function to parse arguments and run dpdk-telemetry.py with a pipe"""

    # Parse command line arguments - matching dpdk-telemetry.py parameters
    parser = argparse.ArgumentParser(
        description="Monitor DPDK telemetry statistics on the command line"
    )
    parser.add_argument(
        "-f",
        "--file-prefix",
        default="rte",
        help="Provide file-prefix for DPDK runtime directory",
    )
    parser.add_argument(
        "-i",
        "--instance",
        default=0,
        type=int,
        help="Provide instance number for DPDK application",
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        default=False,
        help="List all possible file-prefixes and exit",
    )

    args = parser.parse_args()

    # Find the dpdk-telemetry.py script
    telemetry_script = find_telemetry_script()

    # Build arguments list
    args_list = ["-f", args.file_prefix, "-i", str(args.instance)]

    if args.list:
        args_list.append("-l")
        # For --list, just run the command directly without pipes
        cmd = [sys.executable, telemetry_script] + args_list
        return subprocess.run(cmd).returncode

    # Run dpdk-telemetry.py with pipes for stdin and stdout
    process = create_telemetry_process(telemetry_script, args_list)

    # Get and display the connected application name
    print_connected_app(process)

    # TODO: Add monitoring logic here

    # Clean up
    process.stdin.close()
    process.stdout.close()
    process.stderr.close()
    process.wait()

    return 0


if __name__ == "__main__":
    sys.exit(main())
