#! /usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

from __future__ import print_function
from __future__ import unicode_literals

import socket
import os
import sys
import time

BUFFER_SIZE = 200000

METRICS_REQ = "{\"action\":0,\"command\":\"ports_all_stat_values\",\"data\":null}"
API_REG = "{\"action\":1,\"command\":\"clients\",\"data\":{\"client_path\":\""
API_UNREG = "{\"action\":2,\"command\":\"clients\",\"data\":{\"client_path\":\""
GLOBAL_METRICS_REQ = "{\"action\":0,\"command\":\"global_stat_values\",\"data\":null}"
DEFAULT_FP = "/var/run/dpdk/default_client"

try:
    raw_input  # Python 2
except NameError:
    raw_input = input  # Python 3

if sys.version_info.major < 3:
    print("WARNING: Python 2 is deprecated for use in DPDK, and will not work in future releases.", file=sys.stderr)
    print("Please use Python 3 instead", file=sys.stderr)

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

    def __init__(self): # Creates a client instance
        self.socket = Socket()
        self.file_path = None
        self.choice = None
        self.unregistered = 0

    def __del__(self):
        try:
            if self.unregistered == 0:
                self.unregister();
        except:
            print("Error - Client could not be destroyed")

    def getFilepath(self, file_path): # Gets arguments from Command-Line and assigns to instance of client
        self.file_path = file_path

    def register(self): # Connects a client to DPDK-instance
        if os.path.exists(self.file_path):
            os.unlink(self.file_path)
        try:
            self.socket.recv_fd.bind(self.file_path)
        except socket.error as msg:
            print ("Error - Socket binding error: " + str(msg) + "\n")
        self.socket.recv_fd.settimeout(2)
        self.socket.send_fd.connect("/var/run/dpdk/rte/telemetry")
        JSON = (API_REG + self.file_path + "\"}}")
        self.socket.send_fd.sendall(JSON.encode())

        self.socket.recv_fd.listen(1)
        self.socket.client_fd = self.socket.recv_fd.accept()[0]

    def unregister(self): # Unregister a given client
        self.socket.client_fd.send((API_UNREG + self.file_path + "\"}}").encode())
        self.socket.client_fd.close()

    def requestMetrics(self): # Requests metrics for given client
        self.socket.client_fd.send(METRICS_REQ.encode())
        data = self.socket.client_fd.recv(BUFFER_SIZE).decode()
        print("\nResponse: \n", data)

    def repeatedlyRequestMetrics(self, sleep_time): # Recursively requests metrics for given client
        print("\nPlease enter the number of times you'd like to continuously request Metrics:")
        n_requests = int(raw_input("\n:"))
        print("\033[F") #Removes the user input from screen, cleans it up
        print("\033[K")
        for i in range(n_requests):
            self.requestMetrics()
            time.sleep(sleep_time)

    def requestGlobalMetrics(self): #Requests global metrics for given client
        self.socket.client_fd.send(GLOBAL_METRICS_REQ.encode())
        data = self.socket.client_fd.recv(BUFFER_SIZE).decode()
        print("\nResponse: \n", data)

    def interactiveMenu(self, sleep_time): # Creates Interactive menu within the script
        while self.choice != 4:
            print("\nOptions Menu")
            print("[1] Send for Metrics for all ports")
            print("[2] Send for Metrics for all ports recursively")
            print("[3] Send for global Metrics")
            print("[4] Unregister client")

            try:
                self.choice = int(raw_input("\n:"))
                print("\033[F") #Removes the user input for screen, cleans it up
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

if __name__ == "__main__":

    sleep_time = 1
    file_path = ""
    if (len(sys.argv) == 2):
        file_path = sys.argv[1]
    else:
        print("Warning - No filepath passed, using default (" + DEFAULT_FP + ").")
        file_path = DEFAULT_FP
    client = Client()
    client.getFilepath(file_path)
    client.register()
    client.interactiveMenu(sleep_time)
