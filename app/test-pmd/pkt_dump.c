#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_arp.h>
#include <rte_tcp.h>
#include <rte_udp.h>
#include <rte_icmp.h>
#include <rte_vxlan.h>

int log_type;

#define ETHER_ADDR_PRT_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define ETHER_ADDR_BYTES(mac_addrs) \
	(mac_addrs[0]), \
	(mac_addrs[1]), \
	(mac_addrs[2]), \
	(mac_addrs[3]), \
	(mac_addrs[4]), \
	(mac_addrs[5])
#define IPv4_UINT32_BYTES_FMT "%u.%u.%u.%u"
#define IPv4_UINT32_BYTES(addr) \
	(uint8_t) (((addr) >> 24) & 0xFF),\
	(uint8_t) (((addr) >> 16) & 0xFF),\
	(uint8_t) (((addr) >> 8) & 0xFF),\
	(uint8_t) ((addr) & 0xFF)

int pkt_may_pull(struct rte_mbuf *mbuf, uint16_t hdr_len);
int mrg_mbuf(struct rte_mbuf *first_mbuf, uint16_t expct_len);

#define PKT_DUMP_LOG(level, ...) \
	rte_log(RTE_LOG_ ## level, log_type, __VA_ARGS__)

#define GET_PKT_HDR_OFFSET(mbuf, hdr_type, offset, hdr) 				\
	do {																\
		(hdr) = NULL;													\
		if (likely(pkt_may_pull(mbuf, (offset) + sizeof(hdr_type)))) {	\
			(hdr) = rte_pktmbuf_mtod_offset(mbuf, hdr_type *, offset); 	\
		}																\
	} while(0)

int pkt_log_init(void) {
    FILE *f;
    log_type = rte_log_register("pkt_dump"); 
    if (log_type >= 0) {
        rte_log_set_global_level(RTE_LOG_DEBUG);
        f = fopen("/home/jun/vm/pkt.log", "a+");
        if (f == NULL) {
            printf("open file failed\n");
            return -1;
        } else {
            rte_openlog_stream(f);
        }
    } else {
        printf("log register failed %d\n", log_type);
    }
}

static int mrg_mbuf(struct rte_mbuf *first_mbuf, uint16_t expct_len)
{
    uint16_t cpy_len, need_len, mrg_sge;
    void *first_mbuf_tail;
    void *second_mbuf_head;
    struct rte_mbuf *second_mbuf;

    mrg_sge = 17;  // avoid too many sge to quit while loop

    while (first_mbuf->data_len < expct_len && first_mbuf->next && mrg_sge) {
        second_mbuf = first_mbuf->next;
        need_len = expct_len - first_mbuf->data_len;
        cpy_len = need_len < second_mbuf->data_len ? need_len : second_mbuf->data_len;
        first_mbuf_tail = rte_pktmbuf_mtod_offset(first_mbuf, void *, first_mbuf->data_len);
        second_mbuf_head = rte_pktmbuf_mtod(second_mbuf, void *);

        /* Copy part of the second mbuf to the first one. */
        rte_memcpy(first_mbuf_tail, second_mbuf_head, cpy_len);
        first_mbuf->data_len += cpy_len;
        if (cpy_len == second_mbuf->data_len) {
            /* Remove the second mbuf when the whole of it has copied to the first mbuf. */
            first_mbuf->nb_segs--;
            first_mbuf->next = second_mbuf->next;
            rte_pktmbuf_free_seg(second_mbuf);
            second_mbuf = NULL;
        } else {
            second_mbuf->data_off += cpy_len;
            second_mbuf->data_len -= cpy_len;
        }
        mrg_sge--;
    }
    return (first_mbuf->data_len >= expct_len) ? 1 : 0;
}

int pkt_may_pull(struct rte_mbuf *mbuf, uint16_t hdr_len)
{
    if (likely(hdr_len <= mbuf->data_len))
        return 1;
    if (unlikely(hdr_len > mbuf->buf_len - rte_pktmbuf_headroom(mbuf)))
        return 0;
    /* multi segment */
    return mrg_mbuf(mbuf, hdr_len);
}

static int first_fragment(const struct rte_ipv4_hdr * hdr)
{
	uint16_t flag_offset, ip_ofs;

	flag_offset = rte_be_to_cpu_16(hdr->fragment_offset);
	ip_ofs = (uint16_t)(flag_offset & RTE_IPV4_HDR_OFFSET_MASK);
	if (ip_ofs == 0)
		return 1;
	return 0;
}
static void show_meta(struct rte_mbuf *m)
{
    PKT_DUMP_LOG(INFO, "meta:\t\t mbuf:%p next:%p pkt_len:%u data_len:%u nb_segs:%u \n",
        m, m->next, m->pkt_len, m->data_len, m->nb_segs);
}

static void show_eth_hdr(struct rte_ether_hdr *eth_hdr)
{
	PKT_DUMP_LOG(INFO, "eth_hdr:\t "ETHER_ADDR_PRT_FMT" > "ETHER_ADDR_PRT_FMT"  type:%x\n",
		ETHER_ADDR_BYTES(eth_hdr->s_addr.addr_bytes),
		ETHER_ADDR_BYTES(eth_hdr->d_addr.addr_bytes),
		ntohs(eth_hdr->ether_type));
}

static void show_ipv4_hdr(struct rte_ipv4_hdr *ipv4_hdr)
{
	uint16_t frag_off;
	uint32_t ipv4_saddr, ipv4_daddr;
	frag_off = rte_be_to_cpu_16(ipv4_hdr->fragment_offset);
	ipv4_saddr = rte_be_to_cpu_32(ipv4_hdr->src_addr);
	ipv4_daddr = rte_be_to_cpu_32(ipv4_hdr->dst_addr);
	PKT_DUMP_LOG(INFO, "ipv4_hdr:\t "IPv4_UINT32_BYTES_FMT" > "IPv4_UINT32_BYTES_FMT"  flags:%s%s  offset:%u  next_proto_id:%u  packet_id:%u\n",
		IPv4_UINT32_BYTES(ipv4_saddr),
		IPv4_UINT32_BYTES(ipv4_daddr),
		(frag_off & RTE_IPV4_HDR_DF_FLAG) ? "DF" : ".",
		(frag_off & RTE_IPV4_HDR_MF_FLAG) ? "MF" : ".",
		frag_off & 0x1FFF, ipv4_hdr->next_proto_id,
		rte_be_to_cpu_16(ipv4_hdr->packet_id));
}

static void show_ipv6_hdr(struct rte_ipv6_hdr *ipv6_hdr)
{
    PKT_DUMP_LOG(INFO, "ipv6_hdr:\t proto:%u\n", ipv6_hdr->proto);
}

static void show_arp_hdr(struct rte_arp_hdr *arp_hdr)
{
	uint32_t ipv4_sip = rte_be_to_cpu_32(arp_hdr->arp_data.arp_sip);
	uint32_t ipv4_tip = rte_be_to_cpu_32(arp_hdr->arp_data.arp_tip);
	PKT_DUMP_LOG(INFO, "arp_hdr:\t arp_hardware:%u  arp_protocol:%u  "
			"arp_sha:"ETHER_ADDR_PRT_FMT" arp_sip:"IPv4_UINT32_BYTES_FMT"  "
			"arp_tha:"ETHER_ADDR_PRT_FMT" arp_tip:"IPv4_UINT32_BYTES_FMT"\n",
		arp_hdr->arp_hardware, arp_hdr->arp_protocol,
		ETHER_ADDR_BYTES(arp_hdr->arp_data.arp_sha.addr_bytes),
		IPv4_UINT32_BYTES(ipv4_sip),
		ETHER_ADDR_BYTES(arp_hdr->arp_data.arp_tha.addr_bytes),
		IPv4_UINT32_BYTES(ipv4_tip));
}

static void show_udp_hdr(struct rte_udp_hdr *udp_hdr)
{
	PKT_DUMP_LOG(INFO, "udp_hdr:\t %u > %u\n",
		rte_be_to_cpu_16(udp_hdr->src_port),
		rte_be_to_cpu_16(udp_hdr->dst_port));
}

static void show_tcp_hdr(struct rte_tcp_hdr *tcp_hdr)
{
	PKT_DUMP_LOG(INFO, "tcp_hdr:\t %u > %u  tcp_flags:%s %s %s %s %s %s %s %s\n",
		rte_be_to_cpu_16(tcp_hdr->src_port),
		rte_be_to_cpu_16(tcp_hdr->dst_port),
		tcp_hdr->tcp_flags & RTE_TCP_CWR_FLAG ? "CWR" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_ECE_FLAG ? "ECE" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_URG_FLAG ? "URG" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_ACK_FLAG ? "ACK" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_PSH_FLAG ? "PSH" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_RST_FLAG ? "RST" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_SYN_FLAG ? "SYN" : ".",
		tcp_hdr->tcp_flags & RTE_TCP_FIN_FLAG ? "FIN" : ".");
}

static void show_icmp_hdr(struct rte_icmp_hdr *icmp_hdr)
{
	PKT_DUMP_LOG(INFO, "icmp_hdr:\t icmp_type:%u  icmp_code:%u\n", 
		icmp_hdr->icmp_type, icmp_hdr->icmp_code);   
}

static void show_vxlan_hdr(struct rte_vxlan_hdr *vxlan_hdr)
{
    PKT_DUMP_LOG(INFO, "vxlan_hdr:\t flags:%u  vni:%u\n", vxlan_hdr->vx_flags, vxlan_hdr->vx_vni);
}


static void show_binary_stream(char *data, uint32_t len) {
	uint32_t idx = 0;
	uint32_t rlen = len > 128 ? 128 : len;
	uint8_t *cur;
	char *outbase;
	char *out_cur;

	outbase = (char *)malloc(512);
	if (outbase == NULL) {
		PKT_DUMP_LOG(ERR, "fail to malloc buffer");
        return;
	}
	out_cur = outbase;

	cur = (uint8_t *)data;
	for (idx = 0; idx < rlen; idx ++) {
		if (*cur < 0x10) {
			out_cur += sprintf(out_cur, "0%x", *cur);
		} else {
			out_cur += sprintf(out_cur, "%x", *cur);
		}
	
		if ((idx + 1) % 8 != 0) {
			out_cur += sprintf(out_cur, " ");
		} else {
			out_cur += sprintf(out_cur, "\n");
		}
		cur ++;
	}

	out_cur += sprintf(out_cur, "\n");
	
	PKT_DUMP_LOG(INFO, "%s", outbase);
	free(outbase);
}

void show_mbuf(struct rte_mbuf *m) {
    uint16_t offset = 0;
    uint16_t l3_proto = 0;
    uint8_t l4_proto = 0;
    struct rte_ether_hdr *eth_hdr;
    struct rte_ipv4_hdr *ipv4_hdr;
    struct rte_ipv6_hdr *ipv6_hdr;
    struct rte_arp_hdr *arp_hdr;
    struct rte_udp_hdr *udp_hdr;
    struct rte_tcp_hdr *tcp_hdr;
    struct rte_icmp_hdr *icmp_hdr;
    struct rte_vxlan_hdr *vxlan_hdr;

    /* meta */
    show_meta(m);

	ipv4_hdr = NULL;

    /* eth */
    GET_PKT_HDR_OFFSET(m, struct rte_ether_hdr, offset, eth_hdr);
    if (unlikely(!eth_hdr))
        goto out;
    l3_proto = rte_cpu_to_be_16(eth_hdr->ether_type);
    offset += sizeof(struct rte_ether_hdr);

    show_eth_hdr(eth_hdr);

    switch (l3_proto) {
    /* ipv4 */
        case RTE_ETHER_TYPE_IPV4:
		    GET_PKT_HDR_OFFSET(m, struct rte_ipv4_hdr, offset, ipv4_hdr);
		    if (unlikely(!ipv4_hdr))
			    goto out;
		    offset += rte_ipv4_hdr_len(ipv4_hdr);
            l4_proto = ipv4_hdr->next_proto_id;
            show_ipv4_hdr(ipv4_hdr);
            break;
        case RTE_ETHER_TYPE_IPV6:
    /* ipv6 */
		    GET_PKT_HDR_OFFSET(m, struct rte_ipv6_hdr, offset, ipv6_hdr);
		    if (unlikely(!ipv6_hdr))
			    goto out;
		    offset += sizeof(struct rte_ipv6_hdr);
            l4_proto = ipv6_hdr->proto;
            show_ipv6_hdr(ipv6_hdr);
            break;
    /* arp */
        case RTE_ETHER_TYPE_ARP:
            GET_PKT_HDR_OFFSET(m, struct rte_arp_hdr, offset, arp_hdr);
            if (unlikely(!arp_hdr))
                goto out;
            offset += sizeof(struct rte_arp_hdr);
            show_arp_hdr(arp_hdr);
			l4_proto = 0xFF;
            break;
        default:
		    goto out;
	}

    switch (l4_proto) {
    /* udp */
        case IPPROTO_UDP:
			if (!ipv4_hdr || !first_fragment(ipv4_hdr)) {
				PKT_DUMP_LOG(INFO, "udp_data:\t this is data\n");
				goto out;
			}
            GET_PKT_HDR_OFFSET(m, struct rte_udp_hdr, offset, udp_hdr);
            if (unlikely(!udp_hdr))
                goto out;
            offset += sizeof(struct rte_udp_hdr);
            show_udp_hdr(udp_hdr);
            break;
    /* tcp */
        case IPPROTO_TCP:
			if (!ipv4_hdr || !first_fragment(ipv4_hdr)) {
				PKT_DUMP_LOG(INFO, "tcp_data:\t this is data\n");
				goto out;
			}
            GET_PKT_HDR_OFFSET(m, struct rte_tcp_hdr, offset, tcp_hdr);
            if (unlikely(!tcp_hdr))
                goto out;
            offset += sizeof(struct rte_tcp_hdr);
            show_tcp_hdr(tcp_hdr);
            break;
    /* icmp */
        case IPPROTO_ICMP:
			if (!ipv4_hdr || !first_fragment(ipv4_hdr)) {
				PKT_DUMP_LOG(INFO, "icmp_data:\t this is data\n");
				goto out;
			}
            GET_PKT_HDR_OFFSET(m, struct rte_icmp_hdr, offset, icmp_hdr);
            if (unlikely(!icmp_hdr))
                goto out;
            offset += sizeof(struct rte_icmp_hdr);
            show_icmp_hdr(icmp_hdr);
            break;
        default:
            break;
    }
    show_binary_stream((char *)eth_hdr, m->pkt_len);
out:
	PKT_DUMP_LOG(INFO, "\n");
    return;
}

