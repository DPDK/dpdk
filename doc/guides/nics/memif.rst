..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Cisco Systems, Inc.

======================
Memif Poll Mode Driver
======================

Shared memory packet interface (memif) PMD allows for DPDK and any other client
using memif (DPDK, VPP, libmemif) to communicate using shared memory. Memif is
Linux only.

The created device transmits packets in a raw format. It can be used with
Ethernet mode, IP mode, or Punt/Inject. At this moment, only Ethernet mode is
supported in DPDK memif implementation.

Memif works in two roles: server and client. Client connects to server over an
existing socket. It is also a producer of shared memory file and initializes
the shared memory. Each interface can be connected to one peer interface
at same time. The peer interface is identified by id parameter. Server
creates the socket and listens for any client connection requests. The socket
may already exist on the system. Be sure to remove any such sockets, if you
are creating a server interface, or you will see an "Address already in use"
error. Function ``rte_pmd_memif_remove()``, which removes memif interface,
will also remove a listener socket, if it is not being used by any other
interface.

The method to enable one or more interfaces is to use the
``--vdev=net_memif0`` option on the DPDK application command line. Each
``--vdev=net_memif1`` option given will create an interface named net_memif0,
net_memif1, and so on. Memif uses unix domain socket to transmit control
messages. Each memif has a unique id per socket. This id is used to identify
peer interface. If you are connecting multiple
interfaces using same socket, be sure to specify unique ids ``id=0``, ``id=1``,
etc. Note that if you assign a socket to a server interface it becomes a
listener socket. Listener socket can not be used by a client interface on same
client.

.. csv-table:: **Memif configuration options**
   :header: "Option", "Description", "Default", "Valid value"

   "id=0", "Used to identify peer interface", "0", "uint32_t"
   "role=server", "Set memif role", "client", "server|client"
   "bsize=1024", "Size of single packet buffer", "2048", "uint16_t"
   "rsize=11", "Log2 of ring size. If rsize is 10, actual ring size is 1024", "10", "1-14"
   "socket=/tmp/memif.sock", "Socket filename", "/tmp/memif.sock", "string len 108"
   "socket-abstract=no", "Set usage of abstract socket address", "yes", "yes|no"
   "owner-uid=1000", "Set socket listener owner uid. Only relevant to server with socket-abstract=no", "unchanged", "uid_t"
   "owner-gid=1000", "Set socket listener owner gid. Only relevant to server with socket-abstract=no", "unchanged", "gid_t"
   "mac=01:23:45:ab:cd:ef", "Mac address", "01:ab:23:cd:45:ef", ""
   "secret=abc123", "Secret is an optional security option, which if specified, must be matched by peer", "", "string len 24"
   "zero-copy=yes", "Enable/disable zero-copy client mode. Only relevant to client, requires '--single-file-segments' eal argument", "no", "yes|no"

**Connection establishment**

In order to create memif connection, two memif interfaces, each in separate
process, are needed. One interface in ``server`` role and other in
``client`` role. It is not possible to connect two interfaces in a single
process. Each interface can be connected to one interface at same time,
identified by matching id parameter.

Memif driver uses unix domain socket to exchange required information between
memif interfaces. Socket file path is specified at interface creation see
*Memif configuration options* table above. If socket is used by ``server``
interface, it's marked as listener socket (in scope of current process) and
listens to connection requests from other processes. One socket can be used by
multiple interfaces. One process can have ``client`` and ``server`` interfaces
at the same time, provided each role is assigned unique socket.

For detailed information on memif control messages, see: net/memif/memif.h.

Client interface attempts to make a connection on assigned socket. Process
listening on this socket will extract the connection request and create a new
connected socket (control channel). Then it sends the 'hello' message
(``MEMIF_MSG_TYPE_HELLO``), containing configuration boundaries. Client interface
adjusts its configuration accordingly, and sends 'init' message
(``MEMIF_MSG_TYPE_INIT``). This message among others contains interface id. Driver
uses this id to find server interface, and assigns the control channel to this
interface. If such interface is found, 'ack' message (``MEMIF_MSG_TYPE_ACK``) is
sent. Client interface sends 'add region' message (``MEMIF_MSG_TYPE_ADD_REGION``) for
every region allocated. Server responds to each of these messages with 'ack'
message. Same behavior applies to rings. Client sends 'add ring' message
(``MEMIF_MSG_TYPE_ADD_RING``) for every initialized ring. Server again responds to
each message with 'ack' message. To finalize the connection, client interface
sends 'connect' message (``MEMIF_MSG_TYPE_CONNECT``). Upon receiving this message
server maps regions to its address space, initializes rings and responds with
'connected' message (``MEMIF_MSG_TYPE_CONNECTED``). Disconnect
(``MEMIF_MSG_TYPE_DISCONNECT``) can be sent by both server and client interfaces at
any time, due to driver error or if the interface is being deleted.

Files

- net/memif/memif.h *- control messages definitions*
- net/memif/memif_socket.h
- net/memif/memif_socket.c

Shared memory
~~~~~~~~~~~~~

**Shared memory format**

Client is producer and server is consumer. Memory regions, are mapped shared memory files,
created by memif client and provided to server at connection establishment.
Regions contain rings and buffers. Rings and buffers can also be separated into multiple
regions. For no-zero-copy, rings and buffers are stored inside single memory
region to reduce the number of opened files.

region n (no-zero-copy):

+-----------------------+-------------------------------------------------------------------------+
| Rings                 | Buffers                                                                 |
+-----------+-----------+-----------------+---+---------------------------------------------------+
| C2S rings | S2C rings | packet buffer 0 | . | pb ((1 << pmd->run.log2_ring_size)*(c2s + s2c))-1 |
+-----------+-----------+-----------------+---+---------------------------------------------------+

C2S OR S2C Rings:

+--------+--------+-----------------------+
| ring 0 | ring 1 | ring num_c2s_rings - 1|
+--------+--------+-----------------------+

ring 0:

+-------------+---------------------------------------+
| ring header | (1 << pmd->run.log2_ring_size) * desc |
+-------------+---------------------------------------+

Descriptors are assigned packet buffers in order of rings creation. If we have one ring
in each direction and ring size is 1024, then first 1024 buffers will belong to C2S ring and
last 1024 will belong to S2C ring. In case of zero-copy, buffers are dequeued and
enqueued as needed.

**Descriptor format**

+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Quad|6| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |3|3| | | | | | | | | | | | | | |1|1| | | | | | | | | | | | | | | |
|    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|Word|3| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |2|1| | | | | | | | | | | | | | |6|5| | | | | | | | | | | | | | |0|
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0   |length                                                         |region                         |flags                          |
+----+---------------------------------------------------------------+-------------------------------+-------------------------------+
|1   |metadata                                                       |offset                                                         |
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    |6| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |3|3| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |
|    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    |3| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |2|1| | | | | | | | | | | | | | | | | | | | | | | | | | | | | | |0|
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

**Flags field - flags (Quad Word 0, bits 0:15)**

+-----+--------------------+------------------------------------------------------------------------------------------------+
|Bits |Name                |Functionality                                                                                   |
+=====+====================+================================================================================================+
|0    |MEMIF_DESC_FLAG_NEXT|Is chained buffer. When set, the packet is divided into multiple buffers. May not be contiguous.|
+-----+--------------------+------------------------------------------------------------------------------------------------+

**Region index - region (Quad Word 0, 16:31)**

Index of memory region, the buffer is located in.

**Data length - length (Quad Word 0, 32:63)**

Length of transmitted/received data.

**Data Offset - offset (Quad Word 1, 0:31)**

Data start offset from memory region address. *.regions[desc->region].addr + desc->offset*

**Metadata - metadata (Quad Word 1, 32:63)**

Buffer metadata.

Files

- net/memif/memif.h *- descriptor and ring definitions*
- net/memif/rte_eth_memif.c *- eth_memif_rx() eth_memif_tx()*

Zero-copy client
~~~~~~~~~~~~~~~~

Zero-copy client can be enabled with memif configuration option 'zero-copy=yes'. This option
is only relevant to client and requires eal argument '--single-file-segments'.
This limitation is in place, because it is too expensive to identify memseg
for each packet buffer, resulting in worse performance than with zero-copy disabled.
With single file segments we can calculate offset from the beginning of the file
for each packet buffer.

**Shared memory format**

Region 0 is created by memif driver and contains rings. Client interface exposes DPDK memory (memseg).
Instead of using memfd_create() to create new shared file, existing memsegs are used.
Server interface functions the same as with zero-copy disabled.

region 0:

+-----------------------+
| Rings                 |
+-----------+-----------+
| C2S rings | S2C rings |
+-----------+-----------+

region n:

+-----------------+
| Buffers         |
+-----------------+
|memseg           |
+-----------------+

Buffers are dequeued and enqueued as needed. Offset descriptor field is calculated at tx.
Only single file segments mode (EAL option --single-file-segments) is supported, as calculating
offset from multiple segments is too expensive.

Example: testpmd
----------------------------
In this example we run two instances of testpmd application and transmit packets over memif.

First create ``server`` interface::

    # ./<build_dir>/app/dpdk-testpmd -l 0-1 --proc-type=primary --file-prefix=pmd1 --vdev=net_memif,role=server -- -i

Now create ``client`` interface (server must be already running so the client will connect)::

    # ./<build_dir>/app/dpdk-testpmd -l 2-3 --proc-type=primary --file-prefix=pmd2 --vdev=net_memif -- -i

You can also enable ``zero-copy`` on ``client`` interface::

    # ./<build_dir>/app/dpdk-testpmd -l 2-3 --proc-type=primary --file-prefix=pmd2 --vdev=net_memif,zero-copy=yes --single-file-segments -- -i

Start forwarding packets::

    Client:
        testpmd> start

    Server:
        testpmd> start tx_first

Show status::

    testpmd> show port stats 0

For more details on testpmd please refer to :doc:`../testpmd_app_ug/index`.

Example: testpmd and VPP
------------------------
For information on how to get and run VPP please see `<https://wiki.fd.io/view/VPP>`_.

Start VPP in interactive mode (should be by default). Create memif server interface in VPP::

    vpp# create interface memif id 0 server no-zero-copy
    vpp# set interface state memif0/0 up
    vpp# set interface ip address memif0/0 192.168.1.1/24

To see socket filename use show memif command::

    vpp# show memif
    sockets
     id  listener    filename
      0   yes (1)     /run/vpp/memif.sock
    ...

Now create memif interface by running testpmd with these command line options::

    # ./dpdk-testpmd --vdev=net_memif,socket=/run/vpp/memif.sock -- -i

Testpmd should now create memif client interface and try to connect to server.
In testpmd set forward option to icmpecho and start forwarding::

    testpmd> set fwd icmpecho
    testpmd> start

Send ping from VPP::

    vpp# ping 192.168.1.2
    64 bytes from 192.168.1.2: icmp_seq=2 ttl=254 time=36.2918 ms
    64 bytes from 192.168.1.2: icmp_seq=3 ttl=254 time=23.3927 ms
    64 bytes from 192.168.1.2: icmp_seq=4 ttl=254 time=24.2975 ms
    64 bytes from 192.168.1.2: icmp_seq=5 ttl=254 time=17.7049 ms

Example: testpmd memif loopback
-------------------------------
In this example we will create 2 memif ports connected into loopback.
The situation is analogous to cross connecting 2 ports of the NIC by cable.

To set the loopback, just use the same socket and id with different roles::

    # ./dpdk-testpmd --vdev=net_memif0,role=server,id=0 --vdev=net_memif1,role=client,id=0 -- -i

Then start the communication::

    testpmd> start tx_first

Finally we can check port stats to see the traffic::

    testpmd> show port stats all
