# Description

DTS is a testing framework and set of testsuites for end to end testing of DPDK and DPDK
enabled hardware. Unlike DPDK's dpdk-test application, which is used for running unit tests,
DTS is intended to be used to evaluate real DPDK workloads run over supported hardware. For instance,
DTS will control a traffic generator node which will send packets to a system under test node which
is running a DPDK application, and evaluate the resulting DPDK application behavior.

# Supported Test Node Topologies

DTS is a python application which will control a traffic generator node (TG) and system
under test node (SUT). The nodes represent a DPDK device (usually a NIC) located on a
host. The devices/NICs can be located on two separate servers, or on the same server. If you use
the same server for both NICs, install them on separate NUMA domains if possible (this is ideal
for performance testing.)

- 2 links topology: Represents a topology in which the TG node and SUT node both have two network interfaces
which form the TG <-> SUT connection. An example of this would be a dual interface NIC which is the
TG node connected to a dual interface NIC which is the SUT node. Interface 0 on TG <-> interface 0
on SUT, interface 1 on TG <-> interface 1 on SUT.
- 1 link topology: Works, but may result in skips for testsuites which are explicitly decorated with a
2 link requirement. Represents a topology in which the TG node and SUT node are connected over
a single networking link. An example of this would be two single interface
NICs directly connected to each other.

# Simple Linux Setup

1. On your TG and SUT nodes, add a dedicated user for DTS. In this example I
    will name the user "dts."
2. Grant passwordless sudo to the dts user, like so:
    2a: enter 'visudo' in your terminal
    2b: In the visudo text editor, add:
        dts   ALL=(ALL:ALL) NOPASSWD:ALL
3. DTS uses ssh key auth to control the nodes. Copy your ssh keys to the TG and SUT:
    ssh-copy-id dts@{your host}.

For additional detail, please refer to [dts.rst](../doc/guides/tools/dts.rst)

# DTS Configuration

DTS requires two yaml files to be filled out with information about your environment,
test_run.yaml and nodes.yaml, which follow the format illustrated in the example files.

1. Install Docker on the SUT, and Scapy on the TG.
2. Create and fill out a test_run.yaml and nodes.yaml file within your dts
    directory, based on the structure from the example config files.
3. Run the bash terminal commands below in order to run the DTS container
    and start the DTS execution.

```shell
docker build --target dev -t dpdk-dts .
docker run -v $(pwd)/..:/dpdk -v /home/{name of dts user}/.ssh:/root/.ssh:ro -it dpdk-dts bash
$ poetry install
$ poetry run ./main.py
```

These commands will give you a bash shell inside a docker container
with all DTS Python dependencies installed.

## Visual Studio Code

Usage of VScode devcontainers is NOT required for developing on DTS and running DTS,
but provide some small quality of life improvements for the developer. If you
want to develop from a devcontainer, see the instructions below:

VSCode has first-class support for developing with containers.
You may need to run the non-Docker setup commands in the integrated terminal.
DTS contains a .devcontainer config,
so if you open the folder in VSCode it should prompt you to use the dev container
assuming you have the plugin installed.
Please refer to
[VS Development Containers Docs](https://code.visualstudio.com/docs/remote/containers)
to set it all up.
Additionally, there is a line in `.devcontainer/devcontainer.json` that, when included,
will mount the SSH keys of the user currently running VSCode into the container for you.
The `source` on this line can be altered to mount any SSH keys
on the local machine into the container at the correct location.
