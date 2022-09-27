; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

;
; Pipeline input ports.
;
; Syntax:
;
;    port in <port_id> ethdev <ethdev_name> rxq <queue_id> bsz <burst_size>
;    port in <port_id> ring <ring_name> bsz <burst_size>
;    port in <port_id> source mempool <mempool_name> file <file_name> loop <n_loops> packets <n_pkts_max>
;    port in <port_id> fd <file_descriptor> mtu <mtu> mempool <mempool_name> bsz <burst_size>
;
; Note: Customize the parameters below to match your setup.
;
port in 0 ethdev 0000:18:00.0 rxq 0 bsz 32

;
; Pipeline output ports.
;
; Syntax:
;
;    port out <port_id> ethdev <ethdev_name> txq <queue_id> bsz <burst_size>
;    port out <port_id> ring <ring_name> bsz <burst_size>
;    port out <port_id> sink file <file_name> | none
;    port out <port_id> fd <file_descriptor> bsz <burst_size>
;
; Note: Customize the parameters below to match your setup.
;
port out 0 ring RXQ0 bsz 32
