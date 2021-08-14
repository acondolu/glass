#pragma once
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <stdint.h>

#include <atomic>
#include <functional>

#define localhost 16777343

/**
 * Counter for IPv4 Identification numbers
 */
extern std::atomic<uint32_t> ipv4_id;

/**
 * Raw socket
 */
extern int raw_socket;

/**
 * An IPv4 address + port.
 * Both represented in network order.
 */
struct Addr {
  __extension__ union {
    struct {
      uint32_t addr;
      uint16_t port;
      uint16_t __pad16 = 0;
    };
    uint64_t raw;
  };
} __attribute__((packed));

enum conn_status {
  XXX,
  LISTEN,       // the peer is unknown, no segment received
  SYN_RCVD,     // the initial SYN segment has been received
  ESTABLISHED,  // handshake is over, connection established
  CLOSED        // No more data should be exhanged. Will be freed at timeout.
};

class SynAck;

struct session_data {
  Addr addr;
  session_data *peer;
  conn_status status;
  time_t last_activity;
  SynAck *syn_ack;  // != NULL only when status == SYN_RCVD
  uint16_t dest_port;
  // TCP options values
  uint16_t remote_win;  // network order
  uint16_t remote_mss;  // network order
  uint32_t ts_value;    // network order
  uint8_t shift_count;  // network order
  bool sack_permitted;
  // sequence number
  tcp_seq seq_remote;  // network order
  // tcp_seq seq_local;
  // diff seq_diff; /* sequence number diff */
  // diff ack_diff; /* acknowledgement number diff */
  // ...and more: encryption keys, ...
};

// pair, ends
struct init_data {
  session_data *me;
  session_data *peer;
};

// mixed address (source addr + dest port)
typedef Addr Addr2;

/** HASHING **/

extern bool operator==(const Addr &lhs, const Addr &rhs);

template <>
struct std::hash<Addr> {
  std::size_t operator()(const Addr &c) const { return c.raw; }
};

struct command {
  Addr a0;
  Addr a1;
};
