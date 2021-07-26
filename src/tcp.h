#pragma once
#include <atomic>
#include <cstdint>
#include <netinet/tcp.h>

#include "net.h"

class SynAck {
  unsigned char *buf;
	uint16_t psize;
	u_int32_t source_address;
	u_int32_t dest_address;
  public:
		struct tcphdr *tcph;

	SynAck(uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport);
  ~SynAck();
  void finalize(uint16_t len);
  int send();
  unsigned char* opts();
};

uint16_t mkOptions(unsigned char* buf, uint16_t mss, uint32_t ts_value, uint32_t ts_echo, uint8_t shift_count, bool sack_permitted);

int sendIPv4(iphdr* iph, size_t psize);

void relay_packet(iphdr* iph, size_t len, session_data* session);

void parse_tcp_options(tcphdr* tcp_info, session_data* session);
