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
import time
import locale


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


def validate_stats(process, stat_specs):
    """Validate stat specifications and check that fields are numeric.

    Args:
        process: The subprocess.Popen handle to the telemetry process
        stat_specs: List of stat specifications in format "command.field"

    Returns:
        Tuple of (parsed_specs, initial_values) where:
            parsed_specs: List of tuples (spec, command, field) for valid specs
            initial_values: List of initial values for each stat
        Returns (None, None) on error
    """
    parsed_specs = []
    initial_values = []
    for spec in stat_specs:
        # Parse the stat specification
        if "." not in spec:
            print(f"Error: Invalid stat specification '{spec}'", file=sys.stderr)
            print(
                "Expected format: 'command.field' (e.g., /ethdev/stats,0.ipackets)",
                file=sys.stderr,
            )
            return None, None

        command, field = spec.rsplit(".", 1)
        if not command or not field:
            print(f"Error: Invalid stat specification '{spec}'", file=sys.stderr)
            print(
                "Expected format: 'command.field' (e.g., /ethdev/stats,0.ipackets)",
                file=sys.stderr,
            )
            return None, None

        # Query the stat once to validate it exists and is numeric
        data = query_telemetry(process, command)
        if not isinstance(data, dict):
            print(f"Error: Command '{command}' did not return a dictionary", file=sys.stderr)
            return None, None
        if field not in data:
            print(f"Error: Field '{field}' not found in '{command}' response", file=sys.stderr)
            return None, None
        value = data[field]
        if not isinstance(value, (int, float)):
            print(
                f"Error: Field '{field}' in '{command}' is not numeric (got {type(value).__name__})",
                file=sys.stderr,
            )
            return None, None

        parsed_specs.append((spec, command, field))
        initial_values.append(value)

    return parsed_specs, initial_values


def monitor_stats(process, args):
    """Monitor and display statistics in columns.

    Args:
        process: The subprocess.Popen handle to the telemetry process
        args: Parsed command line arguments
    """
    # Validate all stat specifications and get initial values
    parsed_specs, prev_values = validate_stats(process, args.stats)
    if not parsed_specs:
        return

    # Print header
    header = "Time".ljust(10)
    for spec, _, _ in parsed_specs:
        header += spec.rjust(25)
    if args.total:
        header += "Total".rjust(25)
    print(header)

    # Monitor loop - once per second
    count = 0
    line_ending = "\r" if args.single_line else "\n"
    try:
        while args.timeout is None or count < args.timeout:
            time.sleep(1)
            count += 1

            timestamp = time.strftime("%H:%M:%S")
            row = timestamp.ljust(10)

            current_values = []
            total = 0
            for i, (spec, command, field) in enumerate(parsed_specs):
                data = query_telemetry(process, command)
                current_value = data[field]
                current_values.append(current_value)

                if args.delta:
                    display_value = current_value - prev_values[i]
                else:
                    display_value = current_value

                total += display_value
                row += f"{display_value:n}".rjust(25)

            if args.total:
                row += f"{total:n}".rjust(25)

            print(row, end=line_ending, flush=True)
            prev_values = current_values
    except KeyboardInterrupt:
        if args.single_line:
            print()  # Add newline before exit message
        print("\nMonitoring stopped")


def main():
    """Main function to parse arguments and run dpdk-telemetry.py with a pipe"""

    # Set locale for number formatting
    locale.setlocale(locale.LC_ALL, "")

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
    parser.add_argument(
        "-t",
        "--timeout",
        type=int,
        default=None,
        help="Number of iterations to run before stopping (default: run indefinitely)",
    )
    parser.add_argument(
        "-d",
        "--delta",
        action="store_true",
        default=False,
        help="Display delta values instead of absolute values",
    )
    parser.add_argument(
        "-T",
        "--total",
        action="store_true",
        default=False,
        help="Display a total column at the end of each row",
    )
    parser.add_argument(
        "-1",
        "--single-line",
        action="store_true",
        default=False,
        dest="single_line",
        help="Display output on a single line, replacing the previous output",
    )
    parser.add_argument(
        "stats",
        nargs="*",
        help="Statistics to monitor in format 'command.field' (e.g., /ethdev/stats,0.ipackets)",
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

    # Check if stats were provided
    if not args.stats:
        print("Error: No statistics to monitor specified", file=sys.stderr)
        print("Usage: dpdk-telemetry-watcher.py [options] stat1 stat2 ...", file=sys.stderr)
        print("Example: dpdk-telemetry-watcher.py /ethdev/stats,0.ipackets", file=sys.stderr)
        return 1

    # Run dpdk-telemetry.py with pipes for stdin and stdout
    process = create_telemetry_process(telemetry_script, args_list)

    # Get and display the connected application name
    print_connected_app(process)

    # Monitor the requested statistics
    monitor_stats(process, args)

    # Clean up
    process.stdin.close()
    process.stdout.close()
    process.stderr.close()
    process.wait()

    return 0


if __name__ == "__main__":
    sys.exit(main())
