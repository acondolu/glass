#include <arpa/inet.h>
#include <fcntl.h>
#include <iostream>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/netfilter.h> /* for NF_DROP */
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "netfilter.h"

#include "net.h"
#include "pipe.h"
#include "shutdown.h"
#include "socket.h"
#include "table.h"
#include "tcp.h"

struct nfq_handle *h;

struct nfq_q_handle *qh;

Pipe<command> *cmd_pipe;

UnixDomainSocket<command, int> *cmd_sock;

int raw_socket;

/**
 * Definition of callback function
 */
static int cb (
	struct nfq_q_handle *qh,
	struct nfgenmsg *nfmsg,
  struct nfq_data *nfa,
	void *_data
) {
	// send 'DROP' verdict
	nfqnl_msg_packet_hdr* ph = nfq_get_msg_packet_hdr(nfa);
	int id = ph ? ntohl(ph->packet_id) : 0;
  if (nfq_set_verdict(qh, id, NF_DROP, 0, NULL) < 0) return -1;

	// get IP packet
	struct iphdr* ip_info;
	int len = nfq_get_payload(nfa, (unsigned char**) &ip_info);
	printf("/!\\ IP Packet (payload_len=%d) from %s \n", len, inet_ntoa(*(in_addr*)&ip_info->saddr));
	if (ip_info->protocol == IPPROTO_TCP) {
			struct tcphdr* tcp_info = (struct tcphdr*) ((unsigned char*) ip_info + sizeof(struct iphdr));
			printf("      Source port: %d\n", ntohs(tcp_info->source));
			Addr h = { ip_info->saddr, tcp_info->source };
			uint8_t flags = tcp_info->th_flags;
			session_data* session = state[h];
			if (session == NULL) {
				if (flags == TH_SYN) {
					Addr h2 = { ip_info->saddr, tcp_info->dest };
					init_data* id = init_map[h2];
					if (id != NULL) {
						// Initialize session
						session = id->me;
						session->addr = h;
						session->dest_port = tcp_info->dest;
						session->status = LISTEN;
						state[h] = session;
						id->peer->peer = session;
						init_map.erase(h2);
						free(id);
						goto begin;
					}
				}
				printf("      Unknown host: ignore\n");
				return 0;
				// TODO: create a session by using the SYN payload
			}
			begin:
			session_data* peer = session->peer;
			if (flags == TH_SYN) {
				// If the segment is a SYN, check the state of this host:
				//    LISTEN --> advance. check state of peer: if SYN_RCVD, reply with a SYN-ACK (also peer); else, drop
				//    SYN_RCVD --> reply with another SYN-ACK
				//    ESTABLISHED --> drop
				//    CLOSED --> drop
				//-------------------------------------------------------
				printf(">>>>>> SYN (data len=%lud)\n", len - sizeof(struct iphdr) - tcp_info->doff * 4);
				SynAck* s;
				switch (session->status) {
					case LISTEN: {
						// store sequence numbers
						session->seq_remote = tcp_info->th_seq;
						session->remote_win = tcp_info->th_win;
						// check TCP options
						parse_tcp_options(tcp_info, session);
						// build syn-ack segment
						s = new SynAck(ip_info->daddr, tcp_info->dest, h.addr, h.port);
						s->tcph->th_ack = htonl(ntohl(session->seq_remote) + 1);
						session->syn_ack = s;
						// transition
						session->status = SYN_RCVD;
						if (peer != NULL && peer->status == SYN_RCVD) {
							s->tcph->th_seq = peer->seq_remote;
							s->tcph->th_win = peer->remote_win;
							s->finalize(mkOptions(s->opts(), peer->remote_mss, peer->ts_value, session->ts_value, peer->shift_count, peer->sack_permitted));
							s->send();

							peer->syn_ack->tcph->th_seq = session->seq_remote;
							peer->syn_ack->tcph->th_win = session->remote_win;
							peer->syn_ack->finalize(mkOptions(peer->syn_ack->opts(), session->remote_mss, session->ts_value, peer->ts_value, session->shift_count, session->sack_permitted));
							peer->syn_ack->send();
						}
						return 0;
					}
					case SYN_RCVD:
						if (peer != NULL && peer->status == SYN_RCVD)
							session->syn_ack->send();
						return 0;
					case ESTABLISHED:
					case CLOSED:
					default:
						return 0;
				}
				return 0;
			} // end: SYN
			if (session->status == SYN_RCVD && (flags & TH_ACK) != 0) {
				// A fist ACK has been received: connection is established
				session->status = ESTABLISHED;
			}
			if (session->status == ESTABLISHED && peer != nullptr && peer->status == ESTABLISHED) {
				printf(">>>> Relaying TCP segment\n");
				relay_packet(ip_info, len, session);
			}
			// RST
			if (flags == TH_RST) {
				printf(">>>> RST\n");
				state.erase(h);
				free(session);
				if (peer != NULL) {
					state.erase(peer->addr);
					free(peer);
				}
				return 0;
			}
			// TODO: FIN & Co.
			// see: https://users.cs.northwestern.edu/~agupta/cs340/project2/TCPIP_State_Transition_Diagram.pdf
	}
	return 0;
}

/**
 * Exit handler.
 * This is only called after a successful initialization, hence there
 * is not need to check whether the things to destroy have been created.
 */
void Netfilter::destroy() {
	nfq_destroy_queue(qh);
	nfq_close(h);
	close(raw_socket);
	delete cmd_pipe;
	delete cmd_sock;
}

int cb_command(command* cmd) {
  session_data* s0 = (session_data*) calloc(1, sizeof(session_data));
	session_data* s1 = (session_data*) calloc(1, sizeof(session_data));
  init_data* d0 = (init_data*) malloc(sizeof(init_data));
  d0->me = s0;
  d0->peer = s1;
  init_data* d1 = (init_data*) malloc(sizeof(init_data));
  d1->me = s1;
  d1->peer = s0;
  init_map[cmd->a0] = d0;
  init_map[cmd->a1] = d1;
	return 0;
}

int uds_cb(command* cmd) {
	return cmd_pipe->enqueue(cmd);
}

/**
 * Initialisation function (netfilter & raw socket)
 */
int Netfilter::init(uint16_t queue_num, char* socket_file) {
	h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		return -1;
	}

	printf("unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		return -1;
	}

	printf("binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		return -1;
	}

	printf("binding this socket to queue '%d'\n", queue_num);
	qh = nfq_create_queue(h,  queue_num, &cb, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		return -1;
	}

	printf("setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		return -1;
	}
	// Init raw socket
	// Create a raw socket
	raw_socket = socket(PF_INET, SOCK_RAW, IPPROTO_TCP);
	if (raw_socket == -1) {
		perror("Failed to create socket");
		return -1;
	}

	int one = 1;
	const int *val = &one;
	// tell the kernel that headers are included in the packets
	if (setsockopt (raw_socket, IPPROTO_IP, IP_HDRINCL, val, sizeof (one)) < 0) {
		perror("Error setting IP_HDRINCL");
		return -1;
	}

	// Init pipe
	cmd_pipe = new Pipe<command>(cb_command);
  if (cmd_pipe->init() < 0) {
    perror("Pipe::init()");
    return -1;
  }

	// Init Unix domain socket
	cmd_sock = new UnixDomainSocket<command, int>(uds_cb);
	if (cmd_sock->init(const_cast<char const*>(socket_file)) != 0) {
    perror("UnixDomainSocket::init()");
    return -1;
  }

	ipv4_id = std::rand() & UINT16_MAX;

  return 0;
}

/**
 * 
 */
void Netfilter::main_loop() {
	char buf[4096] __attribute__ ((aligned));
	int rv;
	int fd = nfq_fd(h);

	struct pollfd fds[4];
	fds[0].fd = fd;
	fds[1].fd = cmd_pipe->fd_in;
	fds[2].fd = cmd_sock->socket_fd;
	fds[3].fd = Shutdown::fd_in;
	fds[0].events = fds[1].events = fds[2].events = fds[3].events = POLLIN;
	while (true) {
		poll(fds, 4, -1);
		if (fds[3].revents & POLLIN) {
			std::cerr << "Shutdown requested." << std::endl;
			return;
		}
		if (fds[1].revents & POLLIN) {
			printf("Stuff available on internal pipe\n");
			if (cmd_pipe->handle_in() < 0) {
				perror("Pipe::process()");
				return;
			}
		}
		if (fds[2].revents & POLLIN) {
			printf("Stuff available from command socket\n");
			if (cmd_sock->handle_in() < 0) {
				perror("UnixDomainSocket::handle_in()");
				return;
			}
		}
		if (fds[0].revents & POLLIN) {
			rv = recv(fd, buf, sizeof(buf), 0);
			if (rv < 0) {
				perror("recv()");
				return;
			}
			nfq_handle_packet(h, buf, rv);
		}
	}
}
