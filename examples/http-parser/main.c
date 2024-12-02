#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/ethernet.h>  // Correct header for macOS

// Function to handle the captured packet
void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    // Ethernet header
    struct ether_header *eth_header = (struct ether_header*) packet;

    // Check if the packet is an IP packet (Ethernet Type is 0x0800 for IP)
    if (ntohs(eth_header->ether_type) != ETHERTYPE_IP) {
        printf("Not an IP packet\n");
        return;
    }

    // IP header
    struct ip *ip_header = (struct ip*)(packet + sizeof(struct ether_header));

    // Check if the packet is a TCP packet (IP protocol number 6 for TCP)
    if (ip_header->ip_p != IPPROTO_TCP) {
        printf("Not a TCP packet\n");
        return;
    }

    // TCP header
    struct tcphdr *tcp_header = (struct tcphdr*)(packet + sizeof(struct ether_header) + sizeof(struct ip));

    // Source and destination IP addresses
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip_header->ip_src), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_header->ip_dst), dst_ip, INET_ADDRSTRLEN);

    // Source and destination TCP ports
    uint16_t src_port = ntohs(tcp_header->th_sport);
    uint16_t dst_port = ntohs(tcp_header->th_dport);

    // Display basic info about the packet
    printf("Captured packet from IP: %s to IP: %s\n", src_ip, dst_ip);
    printf("TCP Src Port: %d, Dst Port: %d\n", src_port, dst_port);

    // If it's an HTTP packet (we assume port 80 for HTTP)
    if (dst_port == 80 || src_port == 80) {
        printf("This is an HTTP packet!\n");

        // Optionally, parse the TCP payload for HTTP content
        const u_char *http_payload = packet + sizeof(struct ether_header) + sizeof(struct ip) + sizeof(struct tcphdr);
        int payload_len = header->len - (http_payload - packet);
        
        // Try to print first few bytes of the HTTP message (for example)
        printf("HTTP Payload (First 50 bytes):\n");
        for (int i = 0; i < 50 && i < payload_len; i++) {
            printf("%c", http_payload[i]);
        }
        printf("\n\n");
    }
}

// Main function
int main(int argc, char *argv[]) {
    pcap_if_t *alldevs;
    pcap_if_t *device;
    pcap_t *handle;
    char errbuf[PCAP_ERRBUF_SIZE];

    // Get the list of all devices
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        fprintf(stderr, "Error finding devices: %s\n", errbuf);
        return 1;
    }

    // Choose the first device for simplicity
    device = alldevs;
    printf("Using device: %s\n", device->name);

    // Open the device for capturing
    handle = pcap_open_live(device->name, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", device->name, errbuf);
        return 2;
    }

    // Set a filter to capture only TCP packets on port 80 or 443 (HTTP/HTTPS)
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, "tcp port 80 or tcp port 443", 0, PCAP_NETMASK_UNKNOWN) == -1) {
        fprintf(stderr, "Could not parse filter\n");
        return 2;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Could not install filter\n");
        return 2;
    }

    // Start capturing packets (capturing 10 packets for demonstration)
    pcap_loop(handle, 10, packet_handler, NULL);

    // Cleanup
    pcap_close(handle);
    pcap_freealldevs(alldevs);
    return 0;
}
