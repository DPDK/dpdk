#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2020 Intel Corporation

"""
Script to be used with V2 Telemetry.
Allows the user input commands and read the Telemetry response.
"""

import socket
import os
import glob
import json
import errno
import readline
import argparse

# global vars
TELEMETRY_VERSION = "v2"
CMDS = []


def read_socket(sock, buf_len, echo=True):
    """ Read data from socket and return it in JSON format """
    reply = sock.recv(buf_len).decode()
    try:
        ret = json.loads(reply)
    except json.JSONDecodeError:
        print("Error in reply: ", reply)
        sock.close()
        raise
    if echo:
        print(json.dumps(ret))
    return ret


def get_app_name(pid):
    """ return the app name for a given PID, for printing """
    proc_cmdline = os.path.join('/proc', str(pid), 'cmdline')
    try:
        with open(proc_cmdline) as f:
            argv0 = f.read(1024).split('\0')[0]
            return os.path.basename(argv0)
    except IOError as e:
        # ignore file not found errors
        if e.errno != errno.ENOENT:
            raise
    return None


def handle_socket(path):
    """ Connect to socket and handle user input """
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    global CMDS
    print("Connecting to " + path)
    try:
        sock.connect(path)
    except OSError:
        print("Error connecting to " + path)
        sock.close()
        return
    json_reply = read_socket(sock, 1024)
    output_buf_len = json_reply["max_output_len"]
    app_name = get_app_name(json_reply["pid"])
    if app_name:
        print('Connected to application: "%s"' % app_name)

    # get list of commands for readline completion
    sock.send("/".encode())
    CMDS = read_socket(sock, output_buf_len, False)["/"]

    # interactive prompt
    text = input('--> ').strip()
    while text != "quit":
        if text.startswith('/'):
            sock.send(text.encode())
            read_socket(sock, output_buf_len)
        text = input('--> ').strip()
    sock.close()


def readline_complete(text, state):
    """ Find any matching commands from the list based on user input """
    all_cmds = ['quit'] + CMDS
    if text:
        matches = [c for c in all_cmds if c.startswith(text)]
    else:
        matches = all_cmds
    return matches[state]


def get_dpdk_runtime_dir(fp):
    """ Using the same logic as in DPDK's EAL, get the DPDK runtime directory
    based on the file-prefix and user """
    if (os.getuid() == 0):
        return os.path.join('/var/run/dpdk', fp)
    return os.path.join(os.environ.get('XDG_RUNTIME_DIR', '/tmp'), 'dpdk', fp)


readline.parse_and_bind('tab: complete')
readline.set_completer(readline_complete)
readline.set_completer_delims(readline.get_completer_delims().replace('/', ''))

parser = argparse.ArgumentParser()
parser.add_argument('-f', '--file-prefix', \
        help='Provide file-prefix for DPDK runtime directory', default='rte')
args = parser.parse_args()
rdir = get_dpdk_runtime_dir(args.file_prefix)
handle_socket(os.path.join(rdir, 'dpdk_telemetry.{}'.format(TELEMETRY_VERSION)))
