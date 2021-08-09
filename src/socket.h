#pragma once
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

template<typename T, typename U>
class UnixDomainSocket {
  private:
  const char* _path = nullptr;
  int nbytes = 0;
  char buf[sizeof(T)];
  struct sockaddr_un from;
  socklen_t fromlen = sizeof(from);
  int (*_callback)(T*);

  public:
  int socket_fd = 0;
  UnixDomainSocket(int (*callback)(T*));
  int init(char const* path);
  int handle_in();
  int send(U*);
  ~UnixDomainSocket();
};
