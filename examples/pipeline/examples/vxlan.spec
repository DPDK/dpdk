; SPDX-License-Identifier: BSD-3-Clause
; Copyright(c) 2020 Intel Corporation

//
// Headers
//
struct ethernet_h {
	bit<48> dst_addr
	bit<48> src_addr
	bit<16> ethertype
}

struct ipv4_h {
	bit<8> ver_ihl
	bit<8> diffserv
	bit<16> total_len
	bit<16> identification
	bit<16> flags_offset
	bit<8> ttl
	bit<8> protocol
	bit<16> hdr_checksum
	bit<32> src_addr
	bit<32> dst_addr
}

struct udp_h {
	bit<16> src_port
	bit<16> dst_port
	bit<16> length
	bit<16> checksum
}

struct vxlan_h {
	bit<8> flags
	bit<24> reserved
	bit<24> vni
	bit<8> reserved2
}

header ethernet instanceof ethernet_h
header ipv4 instanceof ipv4_h
header outer_ethernet instanceof ethernet_h
header outer_ipv4 instanceof ipv4_h
header outer_udp instanceof udp_h
header outer_vxlan instanceof vxlan_h

//
// Meta-data
//
struct metadata_t {
	bit<32> port_in
	bit<32> port_out
}

metadata instanceof metadata_t

//
// Actions
//
struct vxlan_encap_args_t {
	bit<48> ethernet_dst_addr
	bit<48> ethernet_src_addr
	bit<16> ethernet_ether_type
	bit<8> ipv4_ver_ihl
	bit<8> ipv4_diffserv
	bit<16> ipv4_total_len
	bit<16> ipv4_identification
	bit<16> ipv4_flags_offset
	bit<8> ipv4_ttl
	bit<8> ipv4_protocol
	bit<16> ipv4_hdr_checksum
	bit<32> ipv4_src_addr
	bit<32> ipv4_dst_addr
	bit<16> udp_src_port
	bit<16> udp_dst_port
	bit<16> udp_length
	bit<16> udp_checksum
	bit<8> vxlan_flags
	bit<24> vxlan_reserved
	bit<24> vxlan_vni
	bit<8> vxlan_reserved2
	bit<32> port_out
}

// Input frame:
//    Ethernet (14) | IPv4 (total_len)
//
// Output frame:
//    Ethernet (14) | IPv4 (20) | UDP (8) | VXLAN (8) | Input frame | Ethernet FCS (4)
//
// Note: The input frame has its FCS removed before encapsulation in the output
// frame.
//
// Assumption: When read from the table, the outer IPv4 and UDP headers contain
// the following fields:
//    - t.ipv4_total_len: Set to 50, which covers the length of:
//         - The outer IPv4 header (20 bytes);
//         - The outer UDP header (8 bytes);
//         - The outer VXLAN header (8 bytes);
//         - The inner Ethernet header (14 bytes);
//    - t.ipv4_hdr_checksum: Includes the above total length.
//    - t.udp_length: Set to 30, which covers the length of:
//         - The outer UDP header (8 bytes);
//         - The outer VXLAN header (8 bytes);
//         - The inner Ethernet header (14 bytes);
//    - t.udp_checksum: Set to 0.
//
// Once the total length of the inner IPv4 packet (h.ipv4.total_len) is known,
// the outer IPv4 and UDP headers are updated as follows:
//    - h.outer_ipv4.total_len = t.ipv4_total_len + h.ipv4.total_len
//    - h.outer_ipv4.hdr_checksum = t.ipv4_hdr_checksum + h.ipv4.total_len
//    - h.outer_udp.length = t.udp_length + h.ipv4.total_len
//    - h.outer_udp.checksum: No change.
//

action vxlan_encap args instanceof vxlan_encap_args_t {
	//Copy from table entry to headers and metadata.
	dma h.outer_ethernet t.ethernet_dst_addr
	dma h.outer_ipv4 t.ipv4_ver_ihl
	dma h.outer_udp t.udp_src_port
	dma h.outer_vxlan t.vxlan_flags
	mov m.port_out t.port_out

	//Update h.outer_ipv4.total_len field.
	add h.outer_ipv4.total_len h.ipv4.total_len

	//Update h.outer_ipv4.hdr_checksum field.
	ckadd h.outer_ipv4.hdr_checksum h.ipv4.total_len

	//Update h.outer_udp.length field.
	add h.outer_udp.length h.ipv4.total_len

	return
}

action drop args none {
	mov m.port_out 4
	tx m.port_out
}

//
// Tables.
//
table vxlan_table {
	key {
		h.ethernet.dst_addr exact
	}

	actions {
		vxlan_encap
		drop
	}

	default_action drop args none
	size 1048576
}

//
// Pipeline.
//
apply {
	rx m.port_in
	extract h.ethernet
	extract h.ipv4
	table vxlan_table
	emit h.outer_ethernet
	emit h.outer_ipv4
	emit h.outer_udp
	emit h.outer_vxlan
	emit h.ethernet
	emit h.ipv4
	tx m.port_out
}
