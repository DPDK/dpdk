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
import readline

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


readline.parse_and_bind('tab: complete')
readline.set_completer(readline_complete)
readline.set_completer_delims(readline.get_completer_delims().replace('/', ''))

# Path to sockets for processes run as a root user
for f in glob.glob('/var/run/dpdk/*/dpdk_telemetry.%s' % TELEMETRY_VERSION):
    handle_socket(f)
# Path to sockets for processes run as a regular user
for f in glob.glob('%s/dpdk/*/dpdk_telemetry.%s' %
                   (os.environ.get('XDG_RUNTIME_DIR', '/tmp'), TELEMETRY_VERSION)):
    handle_socket(f)
