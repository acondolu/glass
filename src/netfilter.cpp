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

#include "config.h"
#include "control.h"
#include "net.h"
#include "pipe.h"
#include "shutdown.h"
#include "socket.h"
#include "table.h"
#include "tcp.h"
#include "translator.h"
#include "expire.h"
#include "logging.h"

struct nfq_handle* h;

struct nfq_q_handle* qh;

Pipe<Translator::cmd>* cmd_pipe;

Expire<Translator::cmd>* expire;

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
						// expire session
						Translator::cmd* cmd = new Translator::cmd(Translator::EXPIRE_CONN);
						cmd->expire_conn = h;
						expire->timeout(cmd, expire->now + Translator::EXPIRE_CONN_SECS);
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
						session->last_activity = expire->now;
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
						if (peer != NULL && peer->status == SYN_RCVD) {
							session->last_activity = expire->now;
							session->syn_ack->send();
						}
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
				session->last_activity = expire->now;
				printf(">>>> Relaying TCP segment\n");
				relay_packet(ip_info, len, session);
			}
			// RST
			if (flags == TH_RST) {
				printf(">>>> RST\n");
				session->status = CLOSED; // CLOSED? FIXME:
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
}

/**
 * Process an incoming command.
 */
int cb_command(Translator::cmd* cmd) {
	// Reminder: we are responsibile for freeing cmd !
	switch (cmd->tag) {
		case Translator::LINK: {
			Addr a0 = cmd->link.a0;
			Addr a1 = cmd->link.a1;
			free(cmd);
			session_data* s0 = (session_data*) calloc(1, sizeof(session_data));
			s0->last_activity = 0;
			session_data* s1 = (session_data*) calloc(1, sizeof(session_data));
			s1->last_activity = 0;
			init_data* d0 = (init_data*) malloc(sizeof(init_data));
			d0->me = s0;
			d0->peer = s1;
			init_data* d1 = (init_data*) malloc(sizeof(init_data));
			d1->me = s1;
			d1->peer = s0;
			init_map[a0] = d0;
			init_map[a1] = d1;
			// Expire data
			Translator::cmd* ex = new Translator::cmd(Translator::EXPIRE_LISTEN);
			ex->expire_listen = {a0, a1};
			expire->timeout(ex, expire->now + Translator::EXPIRE_LISTEN_SECS);
			return 0;
		}
		case Translator::EXPIRE_LISTEN: {
			Addr a0 = cmd->expire_listen.a0;
			Addr a1 = cmd->expire_listen.a1;
			init_data* i0 = init_map[a0];
			init_data* i1 = init_map[a1];
			session_data* s0;
			session_data* s1;
			if (i0 != nullptr) {
				s0 = i0->me;
				s1 = i0->peer;
			} else if (i1 != nullptr) {
				s0 = i1->peer;
				s1 = i1->me;
			} else {
				// Both entries are not present anymore.
				free(cmd);
				return 0;
			}
			if (s0->last_activity + Translator::EXPIRE_LISTEN_SECS <= expire->now || s1->last_activity + Translator::EXPIRE_LISTEN_SECS <= expire->now) {
				// FIXME: check more statuses when they will be implemented
				if (s0->status == SYN_RCVD || s0->status == ESTABLISHED) state.erase(s0->addr);
				if (s1->status == SYN_RCVD || s1->status == ESTABLISHED) state.erase(s1->addr);
				init_map.erase(a0);
				init_map.erase(a1);
				free(s0);
				free(s1);
				if (i0 != nullptr) free(i0);
				if (i1 != nullptr) free(i1);
				free(cmd);
			} else {
				expire->timeout(cmd, std::min(s0->last_activity, s1->last_activity) + Translator::EXPIRE_LISTEN_SECS);
			}
			return 0;
		}
		case Translator::EXPIRE_CONN: {
			Addr a = cmd->expire_conn;
			session_data* s = state[a];
			if (s != nullptr) {
				if (s->last_activity + Translator::EXPIRE_CONN_SECS <= expire->now) {
					state.erase(a);
					if (s->peer->status == CLOSED) {
						free(s->peer);
						free(s);
					} else {
						s->status = CLOSED;
					}
				} else {
					expire->timeout(cmd, s->last_activity + Translator::EXPIRE_CONN_SECS);
					return 0; // don't free cmd
				}
			}
			free(cmd);
			return 0;
		}
		default:
			free(cmd);
			printf("FATAL: command not implemented\n");
			exit(1);
	}
	return 0;
}

void time_callback(Translator::cmd* c) {
	// Send address of the command on the queue.
	// Resposibility for freeing c is deferred to the other end of the pipe.
	cmd_pipe->enqueue(c);
}

/**
 * Initialisation function (netfilter & raw socket)
 */
int Netfilter::init(Config::config* cfg) {
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

	printf("binding this socket to queue '%d'\n", cfg->nfqueue_number);
	qh = nfq_create_queue(h,  cfg->nfqueue_number, &cb, NULL);
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
	cmd_pipe = new Pipe<Translator::cmd>();
  if (cmd_pipe->init() < 0) {
    perror("Pipe::init()");
    return -1;
  }

	// Init Unix domain socket
	if (Control::init(cfg, cmd_pipe) < 0) {
		perror("Control::init()");
    return -1;
	}

	expire = new Expire<Translator::cmd>(&time_callback);

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
	fds[0].fd = Shutdown::fd_in;
	fds[1].fd = cmd_pipe->fd_in;
	fds[2].fd = fd;
	fds[0].events = fds[1].events = fds[2].events = POLLIN;
	while (true) {
		if (poll(fds, 3, -1) <= 0) {
			Logging::error("Bug in Netfilter::main_loop: poll errored out");
			return;
		}
		if (fds[0].revents & POLLIN) {
			// Shutdown requested
			return;
		}
		if (fds[1].revents & POLLIN) {
			Translator::cmd* c = cmd_pipe->receive();
			if (c == nullptr) {
				perror("Bug in Pipe::receive()");
				return;
			}
			cb_command(c);
		}
		if (fds[2].revents & POLLIN) {
			rv = recv(fd, buf, sizeof(buf), 0);
			if (rv < 0) {
				perror("recv()");
				return;
			}
			nfq_handle_packet(h, buf, rv);
		}
	}
}
