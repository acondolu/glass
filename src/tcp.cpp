#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>

#include "tcp.h"
#include "net.h"

/**
 * Counter for IPv4 Identification numbers
 */
std::atomic<uint32_t> ipv4_id;

/**
 * Obtain a fresh IPv4 ID
 */
#define IPHID (htons(ipv4_id = (ipv4_id + 1) % UINT16_MAX))

// 
constexpr struct sockaddr_in sin_ = {
	AF_INET, // sin_family
	0, // sin_port
	{ INADDR_ANY } // sin.sin_addr.s_addr = htonl(INADDR_ANY) // INADDR_ANY = 0
};

/**
 * 96 bit (12 bytes) pseudo header needed for tcp header checksum calculation 
 */
struct pseudo_header
{
	u_int32_t source_address;
	u_int32_t dest_address;
	u_int8_t placeholder;
	u_int8_t protocol;
	u_int16_t tcp_length;
};

struct pseudo_header psh = {0, 0, 0, IPPROTO_TCP, 0};

/**
 * Sum of TCP pseudo header
 */
unsigned short icsum(u_int32_t source_address, u_int32_t dest_address, u_int16_t tcp_length) {
	unsigned short *ptr = (unsigned short *) &psh;
	int nbytes = 12;
	register long sum = 0;

	psh.source_address = source_address;
	psh.dest_address = dest_address;
	psh.tcp_length = htons(tcp_length);

	while (nbytes>1) {
		sum += *ptr++;
		nbytes -= 2;
	}
	return sum;
}

/**
 * Generic checksum calculation function
 */
unsigned short csum(unsigned short *ptr, int nbytes, long init=0) {
	register long sum;
	unsigned short oddbyte;

	sum=init;
	while(nbytes>1) {
		sum+=*ptr++;
		nbytes-=2;
	}
	if(nbytes==1) {
		oddbyte=0;
		*((u_char*)&oddbyte)=*(u_char*)ptr;
		sum+=oddbyte;
	}

	sum = (sum>>16)+(sum & 0xffff);
	sum = sum + (sum>>16);
	return (short)~sum;
}

/**
 * TCP SYN-ACK segment
 */
SynAck::SynAck(uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport) {
	constexpr uint16_t maxlen = sizeof(struct iphdr) + sizeof(struct tcphdr) + /* max TCP options length */ 40;

	// allocate buffer
	buf = (unsigned char*) calloc(maxlen, sizeof(unsigned char*));
	tcph = (struct tcphdr*) (buf + sizeof(struct iphdr));
	
	// TCP Header
	tcph->th_sport = sport;
	tcph->th_dport = dport;
	tcph->th_flags = TH_SYN | TH_ACK;
	
	// Pseudoheader for TCP checksum
	source_address = saddr;
	dest_address = daddr;
}

void SynAck::finalize(uint16_t len) {
	uint16_t size = sizeof(struct tcphdr) + len; // size of TCP segment
	psize = sizeof(struct iphdr) + size; // size of IP packet
	tcph->th_off = size / 4; // size in 32-bit words

	// TCP checksum
	int init_csum = icsum(source_address, dest_address, size);
	tcph->check = csum((unsigned short*)tcph, size, init_csum);

	uint32_t source_address = psh.source_address;
	uint32_t dest_address = psh.dest_address;
	
	// zero out IP header
	// memset(buf, 0, sizeof(struct iphdr));

	struct iphdr *iph = (struct iphdr*) buf;
	
	// fill in the IP Header
	iph->ihl = 5;
	iph->version = 4;
	// iph->tos = 0;
	iph->tot_len = psize;
	iph->id = IPHID;	// id of this packet
	// iph->frag_off = IP_DF;
	// iph-> IP_DF
	iph->ttl = 64; // 225
	iph->protocol = IPPROTO_TCP;
	// iph->check = 0;		// set to 0 before calculating checksum
	iph->saddr = source_address;
	iph->daddr = dest_address;

	// IP checksum
	iph->check = csum((unsigned short*) buf, psize, 0);
}

int SynAck::send() {
	if (sendto(raw_socket, buf, psize, 0, (struct sockaddr *) &sin_, sizeof(sin_)) < 0) {
		perror("sendto failed");
		return -1;
	}
	return 0;
}

SynAck::~SynAck() {
	std::free(buf);
}

unsigned char* SynAck::opts() {
	return buf + sizeof(struct iphdr) + sizeof(struct tcphdr);
}

uint16_t mkOptions(unsigned char* buf, uint16_t mss, uint32_t ts_value, uint32_t ts_echo, uint8_t shift_count, bool sack_permitted) {
	uint16_t s = 0;
	if (mss != 0) s += 4;
	if (ts_value != 0 || ts_echo != 0) s += 10;
	if (shift_count != 0) s += 3;
	if (sack_permitted) s += 2;
	if (s == 0) return 0;
	uint16_t size_padded = s + (4 - (s % 4)) % 4;
	uint8_t* p = (uint8_t*) buf;
	if (mss != 0) {
		p[0] = 2;
		p[1] = 4;
		*(uint16_t*)(p + 2) = mss;
		p += 4;
	}
	if (ts_value != 0 || ts_echo != 0) {
		p[0] = 8;
		p[1] = 10;
		*(uint32_t*)(p + 2) = ts_value;
		*(uint32_t*)(p + 6) = ts_echo;
		p += 10;
	}
	if (shift_count != 0) {
		p[0] = 3;
		p[1] = 3;
		p[2] = shift_count; 
		p += 3;
	}
	if (sack_permitted) {
		p[0] = 4;
		p[1] = 2;
		p += 2;
	}
	printf("Size %ud, padded %ud\n", s, size_padded);
	return size_padded;
}

int sendIPv4(iphdr* iph, size_t psize) {
	if (sendto(raw_socket, reinterpret_cast<unsigned char*>(iph), psize, 0, (struct sockaddr *) &sin_, sizeof(sin_)) < 0) {
		perror("sendto failed");
		return -1;
	}
	return 0;
}

void relay_packet(iphdr* iph, size_t len, session_data* session) {
	tcphdr* tcph = reinterpret_cast<tcphdr*>(reinterpret_cast<unsigned char*>(iph) + iph->ihl * 4);
	uint16_t size = len - iph->ihl * 4;
	// Pseudoheader for TCP checksum
	uint16_t psh_sum = icsum(iph->daddr, session->peer->addr.addr, size);
	// TCP header
	tcph->th_sport = session->peer->dest_port;
	tcph->th_dport = session->peer->addr.port;
	tcph->check = 0;
	tcph->check = csum(reinterpret_cast<unsigned short*>(tcph), size, psh_sum);
	// IP header
	iph->id = IPHID;
	iph->saddr = iph->daddr;
	iph->daddr = session->peer->addr.addr;
	iph->check = 0;
	iph->check = csum(reinterpret_cast<unsigned short*>(iph), len, 0);
	sendIPv4(iph, len);
}


void parse_tcp_options(tcphdr* tcp_info, session_data* session) {
	uint8_t* p = (uint8_t*) tcp_info + 20;
	uint8_t* end = (uint8_t*) tcp_info + tcp_info->doff * 4;
	while (p < end) {
		switch (*p) {
			case TCPOPT_EOL: break;
			case TCPOPT_NOP: { p++; continue; }
			case TCPOPT_MAXSEG: {
				session->remote_mss = *(uint16_t *)(p+2);
				p += TCPOLEN_MAXSEG;
				continue;
			}
			case TCPOPT_WINDOW: {
				session->shift_count = p[2];
				p += TCPOLEN_WINDOW;
				continue;
			}
			case TCPOPT_TIMESTAMP: {
				session->ts_value = *(uint32_t *)(p+2);
				p += TCPOLEN_TIMESTAMP;
				continue;
			}
			case TCPOPT_SACK_PERMITTED: {
				session->sack_permitted = true;
				p += TCPOLEN_SACK_PERMITTED;
				continue;
			}
			default: {
				// unknown option kind: ignore
				printf("Unknown TCP option: kind %ud\n", p[0]);
				p += (uint8_t) *(p+1); // size
				continue;
			}
		}
	}
}
