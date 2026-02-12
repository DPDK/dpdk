.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Intel Corporation.

Using the AF_XDP driver in Kubernetes
=====================================

Introduction
------------

Two infrastructure components are needed in order to provision a pod
that is using the AF_XDP PMD in Kubernetes:

1. AF_XDP Device Plugin (DP).
2. AF_XDP Container Network Interface (CNI) binary.

Both of these components are available through
the `AF_XDP Device Plugin for Kubernetes`_ repository.

The AF_XDP DP provisions and advertises networking interfaces to Kubernetes,
while the CNI configures and plumbs network interfaces for the Pod.

This document explains how to use the `AF_XDP Device Plugin for Kubernetes`_
with a DPDK application using the :doc:`../nics/af_xdp`.

.. _AF_XDP Device Plugin for Kubernetes: https://github.com/redhat-et/afxdp-plugins-for-kubernetes


Background
----------

The standard :doc:`../nics/af_xdp` initialization process involves loading an eBPF program
onto the kernel netdev to be used by the PMD.
This operation requires root or escalated Linux privileges
and thus prevents the PMD from working in an unprivileged container.
The AF_XDP Device Plugin handles this situation
by managing the eBPF program(s) on behalf of the Pod, outside of the pod context.

At a technical level the AF_XDP Device Plugin opens a Unix Domain Socket (UDS)
and listens for a client to make requests over that socket.
A DPDK application acting as a client connects and initiates a configuration "handshake".
After some validation on the Device Plugin side,
the client receives a file descriptor which points to the XSKMAP
associated with the loaded eBPF program.
The XSKMAP is an eBPF map of AF_XDP sockets (XSK).
The client can then proceed with creating an AF_XDP socket
and inserting that socket into the XSKMAP pointed to by the descriptor.

The EAL vdev argument ``use_cni`` is used to indicate that the user wishes
to run the PMD in unprivileged mode and to receive the XSKMAP file descriptor
from the CNI.
When this flag is set,
the ``XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD`` libbpf flag
should be used when creating the socket
to instruct libbpf not to load the default libbpf program on the netdev.
Instead the loading is handled by the AF_XDP Device Plugin.

The EAL vdev argument ``use_pinned_map`` is used indicate to the AF_XDP PMD
to retrieve the XSKMAP fd from a pinned eBPF map.
This map is expected to be pinned by an external entity like the AF_XDP Device Plugin.
This enabled unprivileged pods to create and use AF_XDP sockets.
When this flag is set, the ``XSK_LIBBPF_FLAGS__INHIBIT_PROG_LOAD`` libbpf flag
is used by the AF_XDP PMD when creating the AF_XDP socket.

The EAL vdev argument ``dp_path`` is used alongside the ``use_cni`` or ``use_pinned_map``
arguments to explicitly tell the AF_XDP PMD where to find either:

1. The UDS to interact with the AF_XDP Device Plugin. OR
2. The pinned xskmap to use when creating AF_XDP sockets.

If this argument is not passed alongside the ``use_cni`` or ``use_pinned_map`` arguments
then the AF_XDP PMD configures it internally to the `AF_XDP Device Plugin for Kubernetes`_.

.. note::

   DPDK AF_XDP PMD <= v23.11 will only work with
   the AF_XDP Device Plugin <= commit id `38317c2`_.

.. note::

   DPDK AF_XDP PMD > v23.11 will work with latest version of the AF_XDP Device Plugin
   through a combination of the ``dp_path`` and/or the ``use_cni`` parameter.
   In these versions of the PMD if a user doesn't explicitly set the ``dp_path`` parameter
   when using ``use_cni`` then that path is transparently configured in the AF_XDP PMD
   to the default `AF_XDP Device Plugin for Kubernetes`_ mount point path.
   The path can be overridden by explicitly setting the ``dp_path`` param.

.. note::

   DPDK AF_XDP PMD > v23.11 is backwards compatible
   with (older) versions of the AF_XDP DP <= commit id `38317c2`_
   by explicitly setting ``dp_path`` to ``/tmp/afxdp.sock``.

.. _38317c2: https://github.com/redhat-et/afxdp-plugins-for-kubernetes/commit/38317c256b5c7dfb39e013a0f76010c2ded03669

Prerequisites
-------------

Device Plugin and DPDK container prerequisites:

* Create a DPDK container image.

* Set up the device plugin and prepare the Pod Spec as described in
  the instructions for `AF_XDP Device Plugin for Kubernetes`_.

* The Docker image should contain the libbpf and libxdp libraries,
  which are dependencies for AF_XDP,
  and should include support for the ``ethtool`` command.

* The Pod should have enabled the capabilities
  ``CAP_NET_RAW`` for AF_XDP socket creation,
  ``IPC_LOCK`` for umem creation and
  ``CAP_BPF`` (for Kernel < 5.19) along with support for hugepages.

  .. note::

     For Kernel versions < 5.19, all BPF sys calls required CAP_BPF,
     to access maps shared between the eBFP program and the userspace program.
     Kernels >= 5.19, only requires CAP_BPF for map creation (BPF_MAP_CREATE)
     and loading programs (BPF_PROG_LOAD).

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

Build a DPDK container image (using Docker)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. Create a Dockerfile (should be placed in top level DPDK directory):

   .. code-block:: console

      FROM fedora:38

      # Setup container to build DPDK applications
      RUN dnf -y upgrade && dnf -y install \
          libbsd-devel \
          numactl-libs \
          libbpf-devel \
          libbpf \
          meson \
          ninja-build \
          libxdp-devel \
          libxdp \
          numactl-devel \
          python3-pyelftools \
          python38 \
          iproute
      RUN dnf group install -y development-tools

      # Create DPDK dir and copy over sources
      # Create DPDK dir and copy over sources
      COPY ./ /dpdk
      WORKDIR /dpdk

      # Build DPDK
      RUN meson setup build
      RUN ninja -C build

2. Build a DPDK container image (using Docker)

   .. code-block:: console

      # docker build -t dpdk -f Dockerfile

Run dpdk-testpmd with the AF_XDP Device Plugin + CNI
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Clone the AF_XDP Device plugin and CNI

  .. code-block:: console

     # git clone https://github.com/redhat-et/afxdp-plugins-for-kubernetes.git

  .. note::

     Ensure you have the AF_XDP Device Plugin + CNI prerequisites installed.

* Build the AF_XDP Device plugin and CNI

  .. code-block:: console

     # cd afxdp-plugins-for-kubernetes/
     # make image

* Make sure to modify the image used by the `daemonset.yml`_ file
  in the deployments directory with the following configuration:

  .. _daemonset.yml : https://github.com/redhat-et/afxdp-plugins-for-kubernetes/blob/main/deployments/daemonset.yml

  .. code-block:: yaml

     image: afxdp-device-plugin:latest

  .. note::

     This will select the AF_XDP DP image that was built locally.
     Detailed configuration options can be found in the AF_XDP Device Plugin `readme`_ .

  .. _readme: https://github.com/redhat-et/afxdp-plugins-for-kubernetes#readme

* Deploy the AF_XDP Device Plugin and CNI

  .. code-block:: console

     # kubectl create -f deployments/daemonset.yml

* Create the Network Attachment definition

  .. code-block:: console

     # kubectl create -f nad.yaml

  Sample nad.yml

  .. code-block:: yaml

     apiVersion: "k8s.cni.cncf.io/v1"
     kind: NetworkAttachmentDefinition
     metadata:
       name: afxdp-network
       annotations:
         k8s.v1.cni.cncf.io/resourceName: afxdp/myPool
     spec:
       config: '{
           "cniVersion": "0.3.0",
           "type": "afxdp",
           "mode": "primary",
           "logFile": "afxdp-cni.log",
           "logLevel": "debug",
           "ethtoolCmds" : ["-N -device- rx-flow-hash udp4 fn",
                            "-N -device- flow-type udp4 dst-port 2152 action 22"
                         ],
           "ipam": {
             "type": "host-local",
             "subnet": "192.168.1.0/24",
             "rangeStart": "192.168.1.200",
             "rangeEnd": "192.168.1.220",
             "routes": [
               { "dst": "0.0.0.0/0" }
             ],
             "gateway": "192.168.1.1"
           }
         }'

  For further reference please use the example provided by the AF_XDP DP `nad.yaml`_

  .. _nad.yaml: https://github.com/redhat-et/afxdp-plugins-for-kubernetes/blob/main/examples/network-attachment-definition.yaml

* Run the Pod

  .. code-block:: console

     # kubectl create -f pod.yaml

  Sample pod.yaml:

  .. code-block:: yaml

     apiVersion: v1
     kind: Pod
     metadata:
      name: dpdk
      annotations:
        k8s.v1.cni.cncf.io/networks: afxdp-network
     spec:
       containers:
       - name: testpmd
         image: dpdk:latest
         command: ["tail", "-f", "/dev/null"]
         securityContext:
           capabilities:
             add:
               - NET_RAW
               - IPC_LOCK
         resources:
           requests:
             afxdp/myPool: '1'
           limits:
             hugepages-1Gi: 2Gi
             cpu: 2
             memory: 256Mi
             afxdp/myPool: '1'
         volumeMounts:
         - name: hugepages
           mountPath: /dev/hugepages
       volumes:
       - name: hugepages
         emptyDir:
           medium: HugePages

  For further reference please see the `pod.yaml`_

  .. _pod.yaml: https://github.com/redhat-et/afxdp-plugins-for-kubernetes/blob/main/examples/pod-spec.yaml

* Run DPDK with a command like the following:

  .. code-block:: console

     kubectl exec -i <Pod name> --container <containers name> -- \
           /<Path>/dpdk-testpmd -l 0,1 --no-pci \
           --vdev=net_af_xdp0,use_cni=1,iface=<interface name> \
           --no-mlockall --in-memory \
           -- -i --a --nb-cores=2 --rxq=1 --txq=1 --forward-mode=macswap;

  Or

  .. code-block:: console

     kubectl exec -i <Pod name> --container <containers name> -- \
           /<Path>/dpdk-testpmd -l 0,1 --no-pci \
           --vdev=net_af_xdp0,use_cni=1,iface=<interface name>,dp_path="/tmp/afxdp_dp/<interface name>/afxdp.sock" \
           --no-mlockall --in-memory \
           -- -i --a --nb-cores=2 --rxq=1 --txq=1 --forward-mode=macswap;

  Or

  .. code-block:: console

     kubectl exec -i <Pod name> --container <containers name> -- \
           /<Path>/dpdk-testpmd -l 0,1 --no-pci \
           --vdev=net_af_xdp0,use_pinned_map=1,iface=<interface name>,dp_path="/tmp/afxdp_dp/<interface name>/xsks_map" \
           --no-mlockall --in-memory \
           -- -i --a --nb-cores=2 --rxq=1 --txq=1 --forward-mode=macswap;

.. note::

   If the ``dp_path`` parameter isn't explicitly set with ``use_cni`` or ``use_pinned_map``
   the AF_XDP PMD will set the parameter values to the `AF_XDP Device Plugin for Kubernetes`_ defaults.
