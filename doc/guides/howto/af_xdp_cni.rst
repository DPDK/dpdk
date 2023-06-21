.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Intel Corporation.

Using a CNI with the AF_XDP driver
==================================

Introduction
------------

CNI, the Container Network Interface, is a technology for configuring
container network interfaces
and which can be used to setup Kubernetes networking.
AF_XDP is a Linux socket Address Family that enables an XDP program
to redirect packets to a memory buffer in userspace.

This document explains how to enable the `AF_XDP Plugin for Kubernetes`_ within
a DPDK application using the :doc:`../nics/af_xdp` to connect and use these technologies.

.. _AF_XDP Plugin for Kubernetes: https://github.com/intel/afxdp-plugins-for-kubernetes


Background
----------

The standard :doc:`../nics/af_xdp` initialization process involves loading an eBPF program
onto the kernel netdev to be used by the PMD.
This operation requires root or escalated Linux privileges
and thus prevents the PMD from working in an unprivileged container.
The AF_XDP CNI plugin handles this situation
by providing a device plugin that performs the program loading.

At a technical level the CNI opens a Unix Domain Socket and listens for a client
to make requests over that socket.
A DPDK application acting as a client connects and initiates a configuration "handshake".
The client then receives a file descriptor which points to the XSKMAP
associated with the loaded eBPF program.
The XSKMAP is a BPF map of AF_XDP sockets (XSK).
The client can then proceed with creating an AF_XDP socket
and inserting that socket into the XSKMAP pointed to by the descriptor.

The EAL vdev argument ``use_cni`` is used to indicate that the user wishes
to run the PMD in unprivileged mode and to receive the XSKMAP file descriptor
from the CNI.
When this flag is set,
the ``XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD`` libbpf flag
should be used when creating the socket
to instruct libbpf not to load the default libbpf program on the netdev.
Instead the loading is handled by the CNI.

.. note::

   The Unix Domain Socket file path appear in the end user is "/tmp/afxdp.sock".


Prerequisites
-------------

Docker and container prerequisites:

* Set up the device plugin
  as described in the instructions for `AF_XDP Plugin for Kubernetes`_.

* The Docker image should contain the libbpf and libxdp libraries,
  which are dependencies for AF_XDP,
  and should include support for the ``ethtool`` command.

* The Pod should have enabled the capabilities ``CAP_NET_RAW`` and ``CAP_BPF``
  for AF_XDP along with support for hugepages.

* Increase locked memory limit so containers have enough memory for packet buffers.
  For example:

  .. code-block:: console

     cat << EOF | sudo tee /etc/systemd/system/containerd.service.d/limits.conf
     [Service]
     LimitMEMLOCK=infinity
     EOF

* dpdk-testpmd application should have AF_XDP feature enabled.

  For further information see the docs for the: :doc:`../../nics/af_xdp`.


Example
-------

Howto run dpdk-testpmd with CNI plugin:

* Clone the CNI plugin

  .. code-block:: console

     # git clone https://github.com/intel/afxdp-plugins-for-kubernetes.git

* Build the CNI plugin

  .. code-block:: console

     # cd afxdp-plugins-for-kubernetes/
     # make build

  .. note::

     CNI plugin has a dependence on the config.json.

  Sample Config.json

  .. code-block:: json

     {
        "logLevel":"debug",
        "logFile":"afxdp-dp-e2e.log",
        "pools":[
           {
              "name":"e2e",
              "mode":"primary",
              "timeout":30,
              "ethtoolCmds" : ["-L -device- combined 1"],
              "devices":[
                 {
                    "name":"ens785f0"
                 }
              ]
           }
        ]
     }

  For further reference please use the `config.json`_

  .. _config.json: https://github.com/intel/afxdp-plugins-for-kubernetes/blob/v0.0.2/test/e2e/config.json

* Create the Network Attachment definition

  .. code-block:: console

     # kubectl create -f nad.yaml

  Sample nad.yml

  .. code-block:: yaml

      apiVersion: "k8s.cni.cncf.io/v1"
      kind: NetworkAttachmentDefinition
      metadata:
        name: afxdp-e2e-test
        annotations:
          k8s.v1.cni.cncf.io/resourceName: afxdp/e2e
      spec:
        config: '{
            "cniVersion": "0.3.0",
            "type": "afxdp",
            "mode": "cdq",
            "logFile": "afxdp-cni-e2e.log",
            "logLevel": "debug",
            "ipam": {
              "type": "host-local",
              "subnet": "192.168.1.0/24",
              "rangeStart": "192.168.1.200",
              "rangeEnd": "192.168.1.216",
              "routes": [
                { "dst": "0.0.0.0/0" }
              ],
              "gateway": "192.168.1.1"
            }
          }'

  For further reference please use the `nad.yaml`_

  .. _nad.yaml: https://github.com/intel/afxdp-plugins-for-kubernetes/blob/v0.0.2/test/e2e/nad.yaml

* Build the Docker image

  .. code-block:: console

     # docker build -t afxdp-e2e-test -f Dockerfile .

  Sample Dockerfile:

  .. code-block:: console

     FROM ubuntu:20.04
     RUN apt-get update -y
     RUN apt install build-essential libelf-dev -y
     RUN apt-get install iproute2  acl -y
     RUN apt install python3-pyelftools ethtool -y
     RUN apt install libnuma-dev libjansson-dev libpcap-dev net-tools -y
     RUN apt-get install clang llvm -y
     COPY ./libbpf<version>.tar.gz /tmp
     RUN cd /tmp && tar -xvmf libbpf<version>.tar.gz && cd libbpf/src && make install
     COPY ./libxdp<version>.tar.gz /tmp
     RUN cd /tmp && tar -xvmf libxdp<version>.tar.gz && cd libxdp && make install

  .. note::

     All the files that need to COPY-ed should be in the same directory as the Dockerfile

* Run the Pod

  .. code-block:: console

     # kubectl create -f pod.yaml

  Sample pod.yaml:

  .. code-block:: yaml

     apiVersion: v1
     kind: Pod
     metadata:
       name: afxdp-e2e-test
       annotations:
         k8s.v1.cni.cncf.io/networks: afxdp-e2e-test
     spec:
       containers:
       - name: afxdp
         image: afxdp-e2e-test:latest
         imagePullPolicy: Never
         env:
         - name: LD_LIBRARY_PATH
           value: /usr/lib64/:/usr/local/lib/
         command: ["tail", "-f", "/dev/null"]
         securityContext:
          capabilities:
             add:
               - CAP_NET_RAW
               - CAP_BPF
         resources:
           requests:
             hugepages-2Mi: 2Gi
             memory: 2Gi
             afxdp/e2e: '1'
           limits:
             hugepages-2Mi: 2Gi
             memory: 2Gi
             afxdp/e2e: '1'

  For further reference please use the `pod.yaml`_

  .. _pod.yaml: https://github.com/intel/afxdp-plugins-for-kubernetes/blob/v0.0.2/test/e2e/pod-1c1d.yaml

* Run DPDK with a command like the following:

  .. code-block:: console

     kubectl exec -i <Pod name> --container <containers name> -- \
           /<Path>/dpdk-testpmd -l 0,1 --no-pci \
           --vdev=net_af_xdp0,use_cni=1,iface=<interface name> \
           -- --no-mlockall --in-memory

For further reference please use the `e2e`_ test case in `AF_XDP Plugin for Kubernetes`_

  .. _e2e: https://github.com/intel/afxdp-plugins-for-kubernetes/tree/v0.0.2/test/e2e
