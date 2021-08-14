#include "control.h"

#include <poll.h>
#include <pthread.h>
#include <stdlib.h>

#include "config.h"
#include "net.h"
#include "pipe.h"
#include "shutdown.h"
#include "socket.h"

namespace {
UnixDomainSocket<command, int> *cmd_sock;
Pipe<Translator::cmd> *cmd_pipe;

int uds_cb(command *cmd) {
  Translator::cmd *ic = (Translator::cmd *)malloc(sizeof(Translator::cmd));
  ic->tag = Translator::LINK;
  ic->link = *cmd;
  return cmd_pipe->enqueue(ic);
}
}  // namespace

void *start_routine(void *arg) {
  struct pollfd fds[3];
  fds[0].fd = cmd_sock->socket_fd;
  fds[1].fd = cmd_pipe->fd_out;
  fds[2].fd = Shutdown::fd_in;
  fds[0].events = fds[1].events = fds[2].events = POLLIN;
  while (true) {
    if (poll(fds, 3, -1) <= 0) {
      printf("poll error\n");
      exit(1);
    }
    if (fds[2].revents & POLLIN) {
      // Shutdown requested
      return nullptr;
    }
    if (fds[1].revents & POLLIN) {
      printf("Not implemented.\n");
      exit(1);
    }
    if (fds[0].revents & POLLIN) {
      if (cmd_sock->handle_in() < 0) {
        perror("UnixDomainSocket::handle_in()");
        return nullptr;
      }
    }
  }
  return nullptr;
}

int Control::init(Config::config *config, Pipe<Translator::cmd> *cmd_pipe_) {
  cmd_sock = new UnixDomainSocket<command, int>(uds_cb);
  if (cmd_sock->init(const_cast<char const *>(config->unix_socket)) != 0) {
    perror("UnixDomainSocket::init()");
    return -1;
  }
  cmd_pipe = cmd_pipe_;
  return 0;
}

void Control::run() {
  pthread_t th;
  int ret = pthread_create(&th, NULL, &start_routine, NULL);
  if (ret < 0) {
  }
}

void Control::destroy() { delete cmd_sock; }
