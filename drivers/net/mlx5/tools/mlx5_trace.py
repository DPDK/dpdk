#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2023 NVIDIA Corporation & Affiliates

"""
Analyzing the mlx5 PMD datapath tracings
"""
import sys
import argparse
import bt2

PFX_TX = "pmd.net.mlx5.tx."
PFX_TX_LEN = len(PFX_TX)


class MlxQueue:
    """Queue container object"""

    def __init__(self):
        self.done_burst = []  # completed bursts
        self.wait_burst = []  # waiting for completion
        self.pq_id = 0

    def log(self):
        """Log all queue bursts"""
        for txb in self.done_burst:
            txb.log()


class MlxMbuf:
    """Packet mbufs container object"""

    def __init__(self):
        self.wqe = 0     # wqe id
        self.ptr = None  # first packet mbuf pointer
        self.len = 0     # packet data length
        self.nseg = 0    # number of segments

    def log(self):
        """Log mbuf"""
        out_txt = "    %X: %u" % (self.ptr, self.len)
        if self.nseg != 1:
            out_txt += " (%d segs)" % self.nseg
        print(out_txt)


class MlxWqe:
    """WQE container object"""

    def __init__(self):
        self.mbuf = []    # list of mbufs in WQE
        self.wait_ts = 0  # preceding wait/push timestamp
        self.comp_ts = 0  # send/recv completion timestamp
        self.opcode = 0

    def log(self):
        """Log WQE"""
        wqe_id = (self.opcode >> 8) & 0xFFFF
        wqe_op = self.opcode & 0xFF
        out_txt = "  %04X: " % wqe_id
        if wqe_op == 0xF:
            out_txt += "WAIT"
        elif wqe_op == 0x29:
            out_txt += "EMPW"
        elif wqe_op == 0xE:
            out_txt += "TSO "
        elif wqe_op == 0xA:
            out_txt += "SEND"
        else:
            out_txt += "0x%02X" % wqe_op
        if self.comp_ts != 0:
            out_txt += " (%d, %d)" % (self.wait_ts, self.comp_ts - self.wait_ts)
        else:
            out_txt += " (%d)" % self.wait_ts
        print(out_txt)
        for mbuf in self.mbuf:
            mbuf.log()

    def comp(self, wqe_id, wqe_ts):
        """Return 0 if WQE in not completedLog WQE"""
        if self.comp_ts != 0:
            return 1
        cur_id = (self.opcode >> 8) & 0xFFFF
        if cur_id > wqe_id:
            cur_id -= wqe_id
            if cur_id <= 0x8000:
                return 0
        else:
            cur_id = wqe_id - cur_id
            if cur_id >= 0x8000:
                return 0
        self.comp_ts = wqe_ts
        return 1


class MlxBurst:
    """Packet burst container object"""

    def __init__(self):
        self.wqes = []    # issued burst WQEs
        self.done = 0     # number of sent/recv packets
        self.req = 0      # requested number of packets
        self.call_ts = 0  # burst routine invocation
        self.done_ts = 0  # burst routine done
        self.queue = None

    def log(self):
        """Log burst"""
        port = self.queue.pq_id >> 16
        queue = self.queue.pq_id & 0xFFFF
        if self.req == 0:
            print(
                "%u: tx(p=%u, q=%u, %u/%u pkts (incomplete)"
                % (self.call_ts, port, queue, self.done, self.req)
            )
        else:
            print(
                "%u: tx(p=%u, q=%u, %u/%u pkts in %u"
                % (
                    self.call_ts,
                    port,
                    queue,
                    self.done,
                    self.req,
                    self.done_ts - self.call_ts,
                )
            )
        for wqe in self.wqes:
            wqe.log()

    def comp(self, wqe_id, wqe_ts):
        """Return 0 if not all of WQEs in burst completed"""
        wlen = len(self.wqes)
        if wlen == 0:
            return 0
        for wqe in self.wqes:
            if wqe.comp(wqe_id, wqe_ts) == 0:
                return 0
        return 1


class MlxTrace:
    """Trace representing object"""

    def __init__(self):
        self.tx_blst = {}  # current Tx bursts per CPU
        self.tx_qlst = {}  # active Tx queues per port/queue
        self.tx_wlst = {}  # wait timestamp list per CPU

    def run(self, msg_it):
        """Run over gathered tracing data and build database"""
        for msg in msg_it:
            if not isinstance(msg, bt2._EventMessageConst):
                continue
            event = msg.event
            if event.name.startswith(PFX_TX):
                do_tx(msg, self)
            # Handling of other log event cathegories can be added here

    def log(self):
        """Log gathered trace database"""
        for pq_id in self.tx_qlst:
            queue = self.tx_qlst.get(pq_id)
            queue.log()


def do_tx_entry(msg, trace):
    """Handle entry Tx busrt"""
    event = msg.event
    cpu_id = event["cpu_id"]
    burst = trace.tx_blst.get(cpu_id)
    if burst is not None:
        # continue existing burst after WAIT
        return
    # allocate the new burst and append to the queue
    burst = MlxBurst()
    burst.call_ts = msg.default_clock_snapshot.ns_from_origin
    trace.tx_blst[cpu_id] = burst
    pq_id = event["port_id"] << 16 | event["queue_id"]
    queue = trace.tx_qlst.get(pq_id)
    if queue is None:
        # queue does not exist - allocate the new one
        queue = MlxQueue()
        queue.pq_id = pq_id
        trace.tx_qlst[pq_id] = queue
    burst.queue = queue
    queue.wait_burst.append(burst)


def do_tx_exit(msg, trace):
    """Handle exit Tx busrt"""
    event = msg.event
    cpu_id = event["cpu_id"]
    burst = trace.tx_blst.get(cpu_id)
    if burst is None:
        return
    burst.done_ts = msg.default_clock_snapshot.ns_from_origin
    burst.req = event["nb_req"]
    burst.done = event["nb_sent"]
    trace.tx_blst.pop(cpu_id)


def do_tx_wqe(msg, trace):
    """Handle WQE record"""
    event = msg.event
    cpu_id = event["cpu_id"]
    burst = trace.tx_blst.get(cpu_id)
    if burst is None:
        return
    wqe = MlxWqe()
    wqe.wait_ts = trace.tx_wlst.get(cpu_id)
    if wqe.wait_ts is None:
        wqe.wait_ts = msg.default_clock_snapshot.ns_from_origin
    wqe.opcode = event["opcode"]
    burst.wqes.append(wqe)


def do_tx_wait(msg, trace):
    """Handle WAIT record"""
    event = msg.event
    cpu_id = event["cpu_id"]
    trace.tx_wlst[cpu_id] = event["ts"]


def do_tx_push(msg, trace):
    """Handle WQE push event"""
    event = msg.event
    cpu_id = event["cpu_id"]
    burst = trace.tx_blst.get(cpu_id)
    if burst is None:
        return
    if not burst.wqes:
        return
    wqe = burst.wqes[-1]
    mbuf = MlxMbuf()
    mbuf.wqe = event["wqe_id"]
    mbuf.ptr = event["mbuf"]
    mbuf.len = event["mbuf_pkt_len"]
    mbuf.nseg = event["mbuf_nb_segs"]
    wqe.mbuf.append(mbuf)


def do_tx_complete(msg, trace):
    """Handle send completion event"""
    event = msg.event
    pq_id = event["port_id"] << 16 | event["queue_id"]
    queue = trace.tx_qlst.get(pq_id)
    if queue is None:
        return
    qlen = len(queue.wait_burst)
    if qlen == 0:
        return
    wqe_id = event["wqe_id"]
    wqe_ts = event["ts"]
    rmv = 0
    while rmv < qlen:
        burst = queue.wait_burst[rmv]
        if burst.comp(wqe_id, wqe_ts) == 0:
            break
        rmv += 1
    # mode completed burst to done list
    if rmv != 0:
        idx = 0
        while idx < rmv:
            queue.done_burst.append(burst)
            idx += 1
        del queue.wait_burst[0:rmv]


def do_tx(msg, trace):
    """Handle Tx related records"""
    name = msg.event.name[PFX_TX_LEN:]
    if name == "entry":
        do_tx_entry(msg, trace)
    elif name == "exit":
        do_tx_exit(msg, trace)
    elif name == "wqe":
        do_tx_wqe(msg, trace)
    elif name == "wait":
        do_tx_wait(msg, trace)
    elif name == "push":
        do_tx_push(msg, trace)
    elif name == "complete":
        do_tx_complete(msg, trace)
    else:
        print("Error: unrecognized Tx event name: %s" % msg.event.name, file=sys.stderr)
        raise ValueError()


def main() -> int:
    """Script entry point"""
    try:
        parser = argparse.ArgumentParser()
        parser.add_argument("path", nargs=1, type=str, help="input trace folder")
        args = parser.parse_args()

        mlx_tr = MlxTrace()
        msg_it = bt2.TraceCollectionMessageIterator(args.path)
        mlx_tr.run(msg_it)
        mlx_tr.log()
        return 0
    except ValueError:
        return -1


if __name__ == "__main__":
    sys.exit(main())
