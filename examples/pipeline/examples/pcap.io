; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2022 Intel Corporation

;
; Pipeline packet mirroring.
;
mirroring slots 4 sessions 64

;
; Pipeline input ports.
;
; Note: Customize the parameters below to match your setup.
;
port in 0 source mempool MEMPOOL0 file ./examples/pipeline/examples/packet.pcap loop 1 packets 0
port in 1 source mempool MEMPOOL0 file ./examples/pipeline/examples/packet.pcap loop 1 packets 0
port in 2 source mempool MEMPOOL0 file ./examples/pipeline/examples/packet.pcap loop 1 packets 0
port in 3 source mempool MEMPOOL0 file ./examples/pipeline/examples/packet.pcap loop 1 packets 0

;
; Pipeline output ports.
;
; Note: Customize the parameters below to match your setup.
;
port out 0 sink file none
port out 1 sink file none
port out 2 sink file none
port out 3 sink file none
