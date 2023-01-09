#! /usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

import socket
import os
import time
import argparse

BUFFER_SIZE = 200000

METRICS_REQ = "{\"action\":0,\"command\":\"ports_all_stat_values\",\"data\":null}"
API_REG = "{\"action\":1,\"command\":\"clients\",\"data\":{\"client_path\":\""
API_UNREG = "{\"action\":2,\"command\":\"clients\",\"data\":{\"client_path\":\""
GLOBAL_METRICS_REQ = "{\"action\":0,\"command\":\"global_stat_values\",\"data\":null}"
DEFAULT_FP = "/var/run/dpdk/default_client"
DEFAULT_PREFIX = 'rte'
RUNTIME_SOCKET_NAME = 'telemetry'


class Socket:

    def __init__(self):
        self.send_fd = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.recv_fd = socket.socket(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        self.client_fd = None

    def __del__(self):
        try:
            self.send_fd.close()
            self.recv_fd.close()
            self.client_fd.close()
        except:
            print("Error - Sockets could not be closed")


class Client:

    def __init__(self):
        # Creates a client instance
        self.socket = Socket()
        self.file_path = None
        self.run_path = None
        self.choice = None
        self.unregistered = 0

    def __del__(self):
        try:
            if self.unregistered == 0:
                self.unregister()
        except:
            print("Error - Client could not be destroyed")

    def getFilepath(self, file_path):
        # Gets arguments from Command-Line and assigns to instance of client
        self.file_path = file_path

    def setRunpath(self, file_path):
        self.run_path = os.path.join(get_dpdk_runtime_dir(args.file_prefix),
                                     RUNTIME_SOCKET_NAME)

    def register(self):
        # Connects a client to DPDK-instance
        if os.path.exists(self.file_path):
            os.unlink(self.file_path)
        try:
            self.socket.recv_fd.bind(self.file_path)
        except socket.error as msg:
            print("Error - Socket binding error: " + str(msg) + "\n")
        self.socket.recv_fd.settimeout(2)
        self.socket.send_fd.connect(self.run_path)
        JSON = (API_REG + self.file_path + "\"}}")
        self.socket.send_fd.sendall(JSON.encode())

        self.socket.recv_fd.listen(1)
        self.socket.client_fd = self.socket.recv_fd.accept()[0]

    def unregister(self):
        # Unregister a given client
        self.socket.client_fd.send((API_UNREG + self.file_path + "\"}}").encode())
        self.socket.client_fd.close()

    def requestMetrics(self):
        # Requests metrics for given client
        self.socket.client_fd.send(METRICS_REQ.encode())
        data = self.socket.client_fd.recv(BUFFER_SIZE).decode()
        print("\nResponse: \n", data)

    def repeatedlyRequestMetrics(self, sleep_time):
        # Recursively requests metrics for given client
        print("\nPlease enter the number of times you'd like to continuously request Metrics:")
        n_requests = int(input("\n:"))
        # Removes the user input from screen, cleans it up
        print("\033[F")
        print("\033[K")
        for i in range(n_requests):
            self.requestMetrics()
            time.sleep(sleep_time)

    def requestGlobalMetrics(self):
        # Requests global metrics for given client
        self.socket.client_fd.send(GLOBAL_METRICS_REQ.encode())
        data = self.socket.client_fd.recv(BUFFER_SIZE).decode()
        print("\nResponse: \n", data)

    def interactiveMenu(self, sleep_time):
        # Creates Interactive menu within the script
        while self.choice != 4:
            print("\nOptions Menu")
            print("[1] Send for Metrics for all ports")
            print("[2] Send for Metrics for all ports recursively")
            print("[3] Send for global Metrics")
            print("[4] Unregister client")

            try:
                self.choice = int(input("\n:"))
                # Removes the user input for screen, cleans it up
                print("\033[F")
                print("\033[K")
                if self.choice == 1:
                    self.requestMetrics()
                elif self.choice == 2:
                    self.repeatedlyRequestMetrics(sleep_time)
                elif self.choice == 3:
                    self.requestGlobalMetrics()
                elif self.choice == 4:
                    self.unregister()
                    self.unregistered = 1
                else:
                    print("Error - Invalid request choice")
            except:
                pass


def get_dpdk_runtime_dir(fp):
    """ Using the same logic as in DPDK's EAL, get the DPDK runtime directory
    based on the file-prefix and user """
    run_dir = os.environ.get('RUNTIME_DIRECTORY')
    if not run_dir:
        if (os.getuid() == 0):
            run_dir = '/var/run'
        else:
            run_dir = os.environ.get('XDG_RUNTIME_DIR', '/tmp')
    return os.path.join(run_dir, 'dpdk', fp)


if __name__ == "__main__":

    sleep_time = 1
    parser = argparse.ArgumentParser()
    parser.add_argument('-f', '--file-prefix', default=DEFAULT_PREFIX,
                        help='Provide file-prefix for DPDK runtime directory')
    parser.add_argument('sock_path', nargs='?', default=DEFAULT_FP,
                        help='Provide socket file path connected by legacy client')
    args = parser.parse_args()

    client = Client()
    client.getFilepath(args.sock_path)
    client.setRunpath(args.file_prefix)
    client.register()
    client.interactiveMenu(sleep_time)
