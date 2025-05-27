//
// looper.cpp
//
// using two ports on the same machine, measure the per-pkt latency of tx/rx
//
// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: BSD-3-Clause-Clear
//

#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <x86intrin.h>

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_version.h>

#define PINGER_IP_ADDR RTE_IPV4(192, 168, 55, 15)
#define PINGEE_IP_ADDR RTE_IPV4(192, 168, 55, 10)

#define RTE_LOGTYPE_LOOPER RTE_LOGTYPE_USER1

#define MTU       (9216)
#define MBUF_SIZE (16834)
#define NB_MBUF   (8192)

#define MEMPOOL_CACHE_SIZE (256)

/* mask of enabled ports */
static uint32_t _enabled_port_mask = 0x3;

// how many packets to send 64M
static uint32_t _count = 64 * 1000 * 1000;

// how many bytes per packet
static uint32_t _size = 100;

static struct rte_mempool* _rte_pktmbuf_pool;

static struct rte_ether_addr tx_ether_addr;
static struct rte_ether_addr rx_ether_addr;

static uint32_t tx_port_id = 0;
static uint32_t rx_port_id = 1;

static const int MAX_PKT_BURST = 32;

static u_int64_t tscPerKhz = 0;
static u_int64_t tsc2nsecfactor;

static u_int64_t average = 0;
static u_int64_t min = ~0UL;
static u_int64_t max = 0;
static u_int64_t total = 0;
static const int MAX_HIST = 25;
static u_int64_t hist[ 25 ];


void init_port(uint32_t port, struct rte_ether_addr* p_eth_addr);
void inittsc2nsec(void);
void histinit(void);
void histprint(void);
void histadd(u_int64_t v);

void histinit(void)
{
   int count;
   for( count = 0; count < MAX_HIST ; count++ )
   {
      hist[ count ] = 0;
   }
}

void histprint(void)
{
   int count;
   int st;
   if ( total == 0 )
   {
      printf( "ERROR:No samples\n" );
      return;
   }
   printf( "\n" );
   printf( "Total:%ld Min:%ld Max:%ld, Ave:%ld\n", total, min, max, average / total );
   st = MAX_HIST;
   if ( max < MAX_HIST ) st = max + 1;

   for( count = 0; count < st ; count++ )
   {
      printf( "%2d uS: %ld\n", count, hist[ count ] );
   }
}

void histadd(u_int64_t v)
{
   if ( v < min ) min = v;
   if ( v > max ) max = v;
   average += v;
   total += 1;
   if ( v < MAX_HIST ) hist[ v ]++;
}


void inittsc2nsec(void)
{

   unsigned long mhz = 0, khz = 0;
   char          str[100], *p;

   // Parse out /proc/cpuinfo to determin CPU clock frequency
   FILE* f = fopen("/proc/cpuinfo", "r");
   if(!f)
   {
      perror("Failed to open /proc/cpuinfo");
      exit( 1 );
   }

   while(fgets(str, sizeof(str), f))
   {
      if(!strstr(str, "cpu MHz")) continue;
      p = strchr(str, ':');
      if(p)
      {
         mhz = atoi(p + 1);
         khz = mhz * 1000;
         p   = strchr(p + 1, '.');
         if(p) khz += atoi(p + 1);
         break;
      }
   }
   fclose(f);


   tscPerKhz = khz;

   // printf( "%ld\n", tscPerKhz );

   tsc2nsecfactor = (1000000ULL << 20) / tscPerKhz;
}

static inline void rwfence(void)
{
   asm volatile("mfence" ::: "memory");
}

static inline u_int64_t tsc2nsec(u_int64_t t)
{
   t = (t * tsc2nsecfactor) >> 20;
   return (t);
}


static int recv_arp_response(uint8_t portid)
{
   int ret = 0;

   rwfence();
   uint64_t timeout = tsc2nsec(__rdtsc()) + 3000000000;

   bool arp_done = false;
   while(arp_done == false)
   {
      struct rte_mbuf* pkts_burst[MAX_PKT_BURST];
      unsigned         nb_rx;

      do {
         nb_rx = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);

         // timeout
         {
            rwfence();
            uint64_t now = tsc2nsec(__rdtsc());
            if(now > timeout)
            {
               RTE_LOG(INFO, LOOPER, "did not rx ARP response (timeout)\n");
               return -1;
            }
         }

      } while(nb_rx <= 0);

      for(unsigned i = 0; i < nb_rx; ++i)
      {
         struct rte_mbuf* m = pkts_burst[i];
         rte_prefetch0(rte_pktmbuf_mtod(m, void*));

         struct rte_ether_hdr* rte_ether_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);

         switch(rte_cpu_to_be_16(rte_ether_hdr->ether_type))
         {
         // ignore harmless network pkts
         case 0x0027:               // netbios announcements
         case 0x0300:               // Rapid MTP
         case RTE_ETHER_TYPE_IPV4:  // 0x0800 - no ping expected yet
         case 0x88CC:               // link layer discovery protocol
         case RTE_ETHER_TYPE_IPV6:  // 0x88DD
            rte_pktmbuf_free(m);
            break;

         case RTE_ETHER_TYPE_ARP:

            if(!rte_is_same_ether_addr(&rte_ether_hdr->dst_addr, &tx_ether_addr))
            {
               // ignore other ARPs (e.g. gratuitious)
               rte_pktmbuf_free(m);
               break;
            }
            else
            {
               // is this a well-formed REQUEST?
               struct rte_arp_hdr* arp_hdr =
                     (struct
                      rte_arp_hdr*)(rte_pktmbuf_mtod(m, unsigned char*) + sizeof(struct rte_ether_hdr));

               if((RTE_ARP_HRD_ETHER == rte_cpu_to_be_16(arp_hdr->arp_hardware))
                  && (RTE_ETHER_TYPE_IPV4 == rte_cpu_to_be_16(arp_hdr->arp_protocol))
                  && (RTE_ETHER_ADDR_LEN == arp_hdr->arp_hlen)
                  && (sizeof(PINGER_IP_ADDR) == arp_hdr->arp_plen)
                  && (RTE_ARP_OP_REPLY == rte_cpu_to_be_16(arp_hdr->arp_opcode)))
               {
                  struct rte_arp_ipv4* arp_data = &arp_hdr->arp_data;
                  if(rte_is_same_ether_addr(&arp_data->arp_tha, &tx_ether_addr)
                     && (rte_cpu_to_be_32(PINGER_IP_ADDR) == arp_data->arp_tip)
                     && (rte_cpu_to_be_32(PINGEE_IP_ADDR) == arp_data->arp_sip))
                  {
                     // better get the other port's mac addr
                     assert(
                           0
                           == memcmp(
                                 rx_ether_addr.addr_bytes,
                                 arp_data->arp_sha.addr_bytes,
                                 RTE_ETHER_ADDR_LEN));
                     arp_done = true;
                     rte_pktmbuf_free(m);
                     continue;
                  }
               }
            }
            /* fall through */


         default:
            RTE_LOG(
                  INFO,
                  LOOPER,
                  "discarding bad arp response (ether_type=0x%04X, "
                  "rte_ether_addr=%02X:%02X:%02X:%02X:%02X:%02X)\n",
                  rte_cpu_to_be_16(rte_ether_hdr->ether_type),
                  rte_ether_hdr->dst_addr.addr_bytes[0],
                  rte_ether_hdr->dst_addr.addr_bytes[1],
                  rte_ether_hdr->dst_addr.addr_bytes[2],
                  rte_ether_hdr->dst_addr.addr_bytes[3],
                  rte_ether_hdr->dst_addr.addr_bytes[4],
                  rte_ether_hdr->dst_addr.addr_bytes[5]);
            rte_pktmbuf_free(m);
            ret += 1;
            break;
         }
      }
   }

   return ret;
}

static bool
      handle_arp_packet(struct rte_mbuf* m, uint8_t portid, struct rte_ether_hdr* rte_ether_hdr)
{
   // is this a well-formed REQUEST?
   struct rte_arp_hdr* arp_hdr =
         (struct rte_arp_hdr*)(rte_pktmbuf_mtod(m, unsigned char*) + sizeof(struct rte_ether_hdr));
   if((RTE_ARP_HRD_ETHER == rte_cpu_to_be_16(arp_hdr->arp_hardware))
      && (RTE_ETHER_TYPE_IPV4 == rte_cpu_to_be_16(arp_hdr->arp_protocol))
      && (RTE_ETHER_ADDR_LEN == arp_hdr->arp_hlen) && (4 == arp_hdr->arp_plen)
      && (RTE_ARP_OP_REQUEST == rte_cpu_to_be_16(arp_hdr->arp_opcode)))
   {
      // is this request for us?
      struct rte_arp_ipv4* arp_data = &arp_hdr->arp_data;
      if((rte_is_broadcast_ether_addr(&arp_data->arp_tha)
          || rte_is_zero_ether_addr(&arp_data->arp_tha))
         && rte_cpu_to_be_32(PINGEE_IP_ADDR) == arp_data->arp_tip)
      {
         // form and send the response
         arp_data->arp_tip = arp_data->arp_sip;
         rte_ether_addr_copy(&arp_data->arp_sha, &arp_data->arp_tha);
         arp_data->arp_sip = rte_cpu_to_be_32(PINGEE_IP_ADDR);
         rte_ether_addr_copy(&rx_ether_addr, &arp_data->arp_sha);
         arp_hdr->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);

         rte_ether_addr_copy(&rte_ether_hdr->src_addr, &rte_ether_hdr->dst_addr);
         rte_ether_addr_copy(&rx_ether_addr, &rte_ether_hdr->src_addr);
         int ret = rte_eth_tx_burst(portid, 0, &m, 1);
         if(1 != ret)
         {
            rte_exit(
                  EXIT_FAILURE, "Cannot send arp packet: err=%d, port=%u\n", ret, (unsigned)portid);
         }

         return true;
      }
   }

   // else discard
   RTE_LOG(INFO, LOOPER, "discarding bad arp packet\n");
   rte_pktmbuf_free(m);
   return false;
}

inline static bool  simple_forward(struct rte_mbuf* m, uint8_t portid)
{
   struct rte_ether_hdr* ether_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);

   switch(rte_cpu_to_be_16(ether_hdr->ether_type))
   {
   // ignore harmless network pkts
   case 0x0027:               // netbios announcements
   case 0x0300:               // Rapid MTP
   case 0x88CC:               // link layer discovery protocol
   case RTE_ETHER_TYPE_IPV6:  // 0x88DD
   case RTE_ETHER_TYPE_IPV4:
      RTE_LOG(
            INFO,
            LOOPER,
            "ignoring packet (ether_type=0x%04X)\n",
            rte_cpu_to_be_16(ether_hdr->ether_type));
      rte_pktmbuf_free(m);
      return false;

   // is this an ARP packet that we are interested in?
   case RTE_ETHER_TYPE_ARP:
      if((rte_is_broadcast_ether_addr(&ether_hdr->dst_addr)
          || rte_is_same_ether_addr(&ether_hdr->dst_addr, &rx_ether_addr)))
      {
         return handle_arp_packet(m, portid, ether_hdr);
      }
      return false;

   // else discard
   default:
      RTE_LOG(
            INFO,
            LOOPER,
            "discarding non-arp, non-ipv4 (ether_type=0x%04X, dest "
            "rte_ether_addr=%02X:%02X:%02X:%02X:%02X:%02X)\n",
            rte_cpu_to_be_16(ether_hdr->ether_type),
            ether_hdr->dst_addr.addr_bytes[0],
            ether_hdr->dst_addr.addr_bytes[1],
            ether_hdr->dst_addr.addr_bytes[2],
            ether_hdr->dst_addr.addr_bytes[3],
            ether_hdr->dst_addr.addr_bytes[4],
            ether_hdr->dst_addr.addr_bytes[5]);
      rte_pktmbuf_free(m);
      return false;
   }
}

static void handle_arp_request(uint8_t portid)
{
   rwfence();
   uint64_t timeout = tsc2nsec(__rdtsc()) + 3000000000;

   while(true)
   {
      struct rte_mbuf* pkts_burst[MAX_PKT_BURST];
      unsigned         nb_rx = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST);
      rwfence();
      uint64_t now = tsc2nsec(__rdtsc());
      if(now > timeout)
      {
         RTE_LOG(INFO, LOOPER, "did not handle ARP request (timeout)\n");
         return;
      }

      for(unsigned j = 0; j < nb_rx; j++)
      {
         printf("\r\nReceived packet:%d", nb_rx);

         struct rte_mbuf* m = pkts_burst[j];
         rte_prefetch0(rte_pktmbuf_mtod(m, void*));

         if(true == simple_forward(m, portid)) { return; }
      }
   }
}

static void send_arp_request(uint8_t portid)
{
   // allocate a new packet for Tx
   struct rte_mbuf* m = rte_pktmbuf_alloc(_rte_pktmbuf_pool);
   assert(NULL != m);

   // form arp request
   struct rte_arp_hdr* arp_hdr =
         (struct rte_arp_hdr*)rte_pktmbuf_append(m, sizeof(struct rte_arp_hdr));
   assert(NULL != arp_hdr);
   arp_hdr->arp_hardware = rte_cpu_to_be_16(RTE_ARP_HRD_ETHER);
   arp_hdr->arp_protocol = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
   arp_hdr->arp_hlen     = RTE_ETHER_ADDR_LEN;
   arp_hdr->arp_plen     = sizeof(PINGER_IP_ADDR);
   arp_hdr->arp_opcode   = rte_cpu_to_be_16(RTE_ARP_OP_REQUEST);

   struct rte_arp_ipv4* arp_data = &arp_hdr->arp_data;
   memcpy(arp_data->arp_sha.addr_bytes, &tx_ether_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
   arp_data->arp_sip = rte_cpu_to_be_32(PINGER_IP_ADDR);
   memset(arp_data->arp_tha.addr_bytes, 0, RTE_ETHER_ADDR_LEN);
   arp_data->arp_tip = rte_cpu_to_be_32(PINGEE_IP_ADDR);

   // form ether header
   struct rte_ether_hdr* ether_hdr =
         (struct rte_ether_hdr*)rte_pktmbuf_prepend(m, sizeof(struct rte_ether_hdr));
   assert(NULL != ether_hdr);
   memset(ether_hdr->dst_addr.addr_bytes, 0xff, RTE_ETHER_ADDR_LEN);
   memcpy(ether_hdr->src_addr.addr_bytes, &tx_ether_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
   ether_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP);

   // driver will free rte mbuf when finished
   int ret = rte_eth_tx_burst(portid, 0, &m, 1);
   if(1 != ret)
   {
      rte_exit(
            EXIT_FAILURE,
            "Cannot rte_eth_tx_burst arp packet: err=%d, port=%u\n",
            ret,
            (unsigned)portid);
   }
}

static void run_test(void)
{
   inittsc2nsec();

   // stats
   // histogram hist("0.5:10.5:20.0");

   // arp exchange to prime the switch mac table (if present)
   {
      printf("\r\nSender ARP Request port:%d", tx_port_id);
      send_arp_request(tx_port_id);
      printf("\r\nReceiver waiting for ARP request, port:%d", rx_port_id);
      handle_arp_request(rx_port_id);
      printf("\r\nSending ARP response");
      recv_arp_response(tx_port_id);
   }

   printf("looper: sending %u packets of size %u bytes\n", _count, _size);
   fflush(stdout);

   static const uint16_t ether_type = 0x1234;
   for(uint32_t i = 0; i < _count; ++i)
   {
      // tx pkt
      uint64_t tx_tsc;
      {
         // allocate a new packet for Tx
         struct rte_mbuf*      m = rte_pktmbuf_alloc(_rte_pktmbuf_pool);
         struct rte_ether_hdr* rte_ether_hdr =
               (struct rte_ether_hdr*)rte_pktmbuf_append(m, sizeof(struct rte_ether_hdr));
         memcpy(rte_ether_hdr->src_addr.addr_bytes, &tx_ether_addr.addr_bytes, 6);
         memcpy(rte_ether_hdr->dst_addr.addr_bytes, &rx_ether_addr.addr_bytes, 6);
         rte_ether_hdr->ether_type = ether_type;
         char* buf                 = (char*)rte_pktmbuf_append(m, _size);
         memset(buf, 0, _size);

         // remember tx time
         rwfence();
         tx_tsc  = __rdtsc();
         int ret = rte_eth_tx_burst(tx_port_id, 0, &m, 1);
         if(1 != ret)
         {
            rte_exit(
                  EXIT_FAILURE,
                  "Cannot rte_eth_tx_burst send packet: err=%d, port=%u\n",
                  ret,
                  (unsigned)tx_port_id);
         }

         // tx pkt
      }

      // FIXME! don't know why this helps, but else see huge (1s!) latencies (on mlx) w/o this
      // here...
      asm volatile("sfence" ::: "memory");

      // rx pkt
      uint64_t rx_tsc;
      {
         bool done = false;
         while(done == false)
         {
            struct rte_mbuf* pkts_burst[MAX_PKT_BURST];
            int              nrx;
            while(0 == (nrx = rte_eth_rx_burst(rx_port_id, 0, pkts_burst, MAX_PKT_BURST)))
               ;

            // remember rx time
            rwfence();
            rx_tsc = __rdtsc();

            for(int i = 0; i < nrx; ++i)
            {
               struct rte_ether_hdr* rte_ether_hdr =
                     rte_pktmbuf_mtod(pkts_burst[i], struct rte_ether_hdr*);
               if(ether_type == rte_ether_hdr->ether_type)
               {
                  assert(done != true);  // better not be more than one of these
                  done = true;
               }
               else
               {
                  RTE_LOG(
                        DEBUG,
                        LOOPER,
                        "ignoring packet (ether_type=0x%04X)\n",
                        rte_cpu_to_be_16(rte_ether_hdr->ether_type));
               }
               rte_pktmbuf_free(pkts_burst[i]);
            }
         }
      }

      if(_count > 100 && 0 == i % (_count / 100))
      {
         printf("looper: rxd pkt %u (%u percent)\r", i, i / (_count / 100));
         fflush(stdout);
      }

      // accumulate stats
      uint64_t rtt_tsc = rx_tsc - tx_tsc;
      rtt_tsc          = rtt_tsc;
      // printf( "%ld... ", tsc2nsec( rtt_tsc ) );
      histadd( (u_int64_t)( tsc2nsec( rtt_tsc ) / 1000 ) );
      // hist.update( tsc2nsec( rtt_tsc ) );
      // AMD - Log Data here

#ifdef FTRACE
      if(tsc2nsec(rtt_tsc) > 100000) { break; }
#endif
   }

   // all done
   // hist.dump_in_usec();
   histprint();

   return;
}

#define handle_error_en(en, msg)                                                                   \
   do {                                                                                            \
      errno = en;                                                                                  \
      perror(msg);                                                                                 \
      exit(EXIT_FAILURE);                                                                          \
   } while(0)

static int launch_one_lcore(__attribute__((unused)) void* dummy)
{
   int                policy, s;
   struct sched_param param;

   s = pthread_getschedparam(pthread_self(), &policy, &param);
   if(s != 0) handle_error_en(s, "pthread_getschedparam");

   policy               = SCHED_FIFO;
   param.sched_priority = 99;

   s = pthread_setschedparam(pthread_self(), policy, &param);
   if(s != 0) handle_error_en(s, "pthread_setschedparam");

   run_test();
   return 0;
}

/* display usage */
static void usage(const char* prgname)
{
   printf(
         "%s [EAL options] -- -p PORTMASK -t TX_PORTID -r RX_PORTID  -s PKT_SIZE\n"
         "  -p PORTMASK: hexadecimal bitmask of ports to configure\n"
         "       output of 'ifconfig' lists i-faces in ascending order of ports\n"
         "          eg: ehs0(port0)        --> set 0th bit --> -p 0x1\n"
         "              ehs1(port1)        --> set 1st bit --> -p 0x2\n"
         "              ehs0-virt00(port2) --> set 2nd bit --> -p 0x4\n"
         "              ehs1-virt00(port10)--> set 10th bit--> -p 0x400\n"
         "       Note: Looper app uses 2 ports on different interfaces\n"
         "              -p 0x3    --->(enable ehs0 and ehs1)\n"
         "              -p 0x404  --->(enable ehs0-virt00 and ehs1-virt00)\n"
         "  -t TX_PORTID\n"
         "  -r RX_PORTID\n"
         "  -s PKT_SIZE in bytes to transmit\n",
         prgname);
}

static int parse_portmask(const char* portmask)
{
   char*         end = NULL;
   unsigned long pm;

   /* parse hexadecimal string */
   pm = strtoul(portmask, &end, 16);
   if((portmask[0] == '\0') || (end == NULL) || (*end != '\0')) return -1;

   if(pm == 0) return -1;

   return pm;
}

static uint32_t parse_portid(const char* q_arg)
{
   char*         end = NULL;
   unsigned long n;

   /* parse hexadecimal string */
   n = strtoul(q_arg, &end, 10);
   if((q_arg[0] == '\0') || (end == NULL) || (*end != '\0')) return 0;

   return n;
}

static uint32_t parse_size(const char* q_arg)
{
   char*         end = NULL;
   unsigned long size;

   /* parse hexadecimal string */
   size = strtoul(q_arg, &end, 10);
   if((q_arg[0] == '\0') || (end == NULL) || (*end != '\0')) return 0;
   if(size == 0)
   {
      // Set to default size of 100 bytes
      size = 100;
   }
   return size;
}

/* Parse the argument given in the command line of the application */
static int parse_args(int argc, char** argv)
{
   int                  opt, ret;
   char**               argvopt;
   int                  option_index;
   char*                prgname  = argv[0];
   static struct option lgopts[] = {{NULL, 0, 0, 0}};
   printf(
         "\r\nStarting Test....\n"
         "........Test Config Info........\n");
   argvopt = argv;
   while((opt = getopt_long(argc, argvopt, "p:t:r:s:T", lgopts, &option_index)) != EOF)
   {
      switch(opt)
      {
      /* portmask */
      case 'p':
         _enabled_port_mask = parse_portmask(optarg);
         if(_enabled_port_mask == 0)
         {
            printf("invalid portmask\n");
            usage(prgname);
            return -1;
         }
         printf("Port mask:0x%x\n", _enabled_port_mask);
         break;
      case 't':
         tx_port_id = parse_portid(optarg);
         printf("Tx port:%u\n", tx_port_id);
         break;
      case 'r':
         rx_port_id = parse_portid(optarg);
         printf("Rx port:%u\n", rx_port_id);
         break;
      case 's':
         _size = parse_size(optarg);
         printf("Pkt size :%u bytes\n", _size);
         break;
      default: usage(prgname); return -1;
      }
   }

   if(optind >= 0) argv[optind - 1] = prgname;

   ret    = optind - 1;
   optind = 0; /* reset getopt lib */
   printf("................................\n");
   return ret;
}

/* Check the link status of all ports in up to 9s, and print them finally */
static void check_all_ports_link_status(uint8_t port_num, uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
   uint8_t             portid, count, all_ports_up, print_flag = 0;
   struct rte_eth_link link;

   printf("\nChecking link status...");
   fflush(stdout);
   for(count = 0; count <= MAX_CHECK_TIME; count++)
   {
      all_ports_up = 1;
      for(portid = 0; portid < port_num; portid++)
      {
         if((port_mask & (1 << portid)) == 0) continue;
         memset(&link, 0, sizeof(link));
         rte_eth_link_get_nowait(portid, &link);
         /* print link status if flag set */
         if(print_flag == 1)
         {
            if(link.link_status)
               printf(
                     "Port %d Link Up - speed %u "
                     "Mbps - %s\n",
                     (uint8_t)portid,
                     (unsigned)link.link_speed,
                     (link.link_duplex == RTE_ETH_LINK_FULL_DUPLEX) ? ("full-duplex")
                                                                : ("half-duplex\n"));
            else
               printf("Port %d Link Down\n", (uint8_t)portid);
            continue;
         }
         /* clear all_ports_up flag if any link down */
         if(link.link_status == 0)
         {
            all_ports_up = 0;
            break;
         }
      }
      /* after finally printing all link status, get out */
      if(print_flag == 1) break;

      if(all_ports_up == 0)
      {
         printf(".");
         fflush(stdout);
         rte_delay_ms(CHECK_INTERVAL);
      }

      /* set the print_flag if all ports up or timeout */
      if(all_ports_up == 1 || count == (MAX_CHECK_TIME - 1))
      {
         print_flag = 1;
         printf("done\n");
      }
   }
}

void init_port(uint32_t port, struct rte_ether_addr* p_eth_addr)
{
   struct rte_eth_dev_info dev_info;
   rte_eth_dev_info_get(port, &dev_info);

   /* Initialize the port/queue configuration */

   /* get the lcore_id for this port */
   unsigned lcore_id = 0;
   while(rte_lcore_is_enabled(lcore_id) == 0)
   {
      lcore_id++;
      if(lcore_id >= RTE_MAX_LCORE) rte_exit(EXIT_FAILURE, "Not enough cores\n");
   }

   printf("Lcore %u: port %u\n", lcore_id, (unsigned)port);

   /* init port */
   printf("Initializing port %u... ", (unsigned)port);
   fflush(stdout);

   struct rte_eth_conf port_conf;
   memset(&port_conf, 0, sizeof(port_conf));

   int ret = rte_eth_dev_configure(port, 1, 1, &port_conf);
   if(ret < 0)
   {
      rte_exit(EXIT_FAILURE, "Cannot configure device: err=%d, port=%u\n", ret, (unsigned)port);
   }

   /*
    * Number of RX/TX ring descriptors
    */
   static const uint16_t nb_rxd = 128;
   static const uint16_t nb_txd = 128;

   /* init one RX queue */
   ret = rte_eth_rx_queue_setup(
         port, 0, nb_rxd, rte_eth_dev_socket_id(port), NULL, _rte_pktmbuf_pool);
   if(ret < 0)
      rte_exit(EXIT_FAILURE, "rte_eth_rx_queue_setup:err=%d, port=%u\n", ret, (unsigned)port);

   /* init one TX queue */
   ret = rte_eth_tx_queue_setup(port, 0, nb_txd, rte_eth_dev_socket_id(port), NULL);
   if(ret < 0)
      rte_exit(EXIT_FAILURE, "rte_eth_tx_queue_setup:err=%d, port=%u\n", ret, (unsigned)port);

   // this fails on Intel XL710, but isn't needed
   // this is needed for Mellanox CX314A, though
   (void)rte_eth_dev_set_mtu(port, MTU);

   /* Start device */
   ret = rte_eth_dev_start(port);
   if(ret < 0) rte_exit(EXIT_FAILURE, "rte_eth_dev_start:err=%d, port=%u\n", ret, (unsigned)port);

   /* ethernet address of port */
   rte_eth_macaddr_get(port, p_eth_addr);
   rte_eth_promiscuous_enable(port);

   // Disable flow control (default in i40e)
   // RT operation does not play well with the flow control.
   // We'd rather drop packets than delay things.
   struct rte_eth_fc_conf fc_conf;
   ret = rte_eth_dev_flow_ctrl_get(port, &fc_conf);
   if(ret != -ENOTSUP)
   {  // some devices don't support this
      if(ret < 0)
         rte_exit(EXIT_FAILURE, "rte_eth_dev_flow_ctrl_get:err=%d, port=%u\n", ret, (unsigned)port);
      if(fc_conf.mode != RTE_ETH_FC_NONE)
      {
         fc_conf.mode = RTE_ETH_FC_NONE;
         ret          = rte_eth_dev_flow_ctrl_set(port, &fc_conf);
         if(ret != -ENOSYS)
         {  // some devices don't support this
            if(ret < 0)
            {
               rte_exit(
                     EXIT_FAILURE,
                     "rte_eth_dev_flow_ctrl_set:err=%d, port=%u\n",
                     ret,
                     (unsigned)port);
            }
         }
      }
   }

   printf(
         "Port %u, MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n\n",
         (unsigned)port,
         p_eth_addr->addr_bytes[0],
         p_eth_addr->addr_bytes[1],
         p_eth_addr->addr_bytes[2],
         p_eth_addr->addr_bytes[3],
         p_eth_addr->addr_bytes[4],
         p_eth_addr->addr_bytes[5]);
}

int main(int argc, char** argv)
{
   histinit();

   /* init EAL */
   int ret = rte_eal_init(argc, argv);
   if(ret < 0) rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
   argc -= ret;
   argv += ret;

   /* parse application arguments (after the EAL ones) */
   ret = parse_args(argc, argv);
   if(ret < 0) rte_exit(EXIT_FAILURE, "Invalid looper arguments\n");

   /* create the mbuf pool */
   _rte_pktmbuf_pool = rte_pktmbuf_pool_create(
         "mbuf_pool", NB_MBUF, MEMPOOL_CACHE_SIZE, 0, MBUF_SIZE, rte_socket_id());
   if(_rte_pktmbuf_pool == NULL) rte_exit(EXIT_FAILURE, "Cannot init mbuf pool (%d)\n", rte_errno);

   uint8_t nb_ports = 0;
   nb_ports         = rte_eth_dev_count_avail();

   if(nb_ports < 2) rte_exit(EXIT_FAILURE, "No Ethernet ports - bye\n");
   printf("Found %u ports..\n", nb_ports);
   // init both ports
   init_port(tx_port_id, &tx_ether_addr);
   init_port(rx_port_id, &rx_ether_addr);

   check_all_ports_link_status(nb_ports, _enabled_port_mask);

   // needed by XL710 and/or Mellanox switch
   printf("Waiting for link to stabilize");
   fflush(stdout);
   for(int i = 0; i < 3; ++i)
   {
      printf(".");
      fflush(stdout);
      sleep(1);
   }
   printf("done\n");


   /* launch per-lcore init on every lcore */
   rte_eal_mp_remote_launch(launch_one_lcore, NULL, CALL_MAIN);
   rte_eal_mp_wait_lcore();
   return 0;
}
