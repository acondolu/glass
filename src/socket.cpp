#include "socket.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include "net.h"

template <typename T, typename U>
UnixDomainSocket<T, U>::UnixDomainSocket(int (*callback)(T *)) {
  _callback = callback;
}

template <typename T, typename U>
int UnixDomainSocket<T, U>::init(char const *path) {
  _path = path;
  struct sockaddr_un address;

  socket_fd = socket(PF_UNIX, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    printf("socket() failed\n");
    return 1;
  }

  unlink(path);

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  strncpy(address.sun_path, path, 107);

  if (bind(socket_fd, (struct sockaddr *)&address,
           sizeof(struct sockaddr_un)) != 0) {
    perror("Socket::init()/bind()");
    return 1;
  }
  return 0;
}

template <typename T, typename U>
int UnixDomainSocket<T, U>::handle_in() {
  int len = recvfrom(socket_fd, buf, sizeof(T) - nbytes, 0,
                     (struct sockaddr *)&from, &fromlen);
  if (len < 0) {
    perror("Socket::recv()");
    return len;
  }
  nbytes += len;
  if (nbytes == sizeof(T)) {
    nbytes = 0;
    return _callback((T *)buf);
  }
  return 0;
}

template <typename T, typename U>
int UnixDomainSocket<T, U>::send(U *u) {
  // todo: pad?
  return sendto(socket_fd, (unsigned char *)u, sizeof(U), 0,
                (struct sockaddr *)&from, fromlen);
}

template <typename T, typename U>
UnixDomainSocket<T, U>::~UnixDomainSocket() {
  if (socket_fd != 0) close(socket_fd);
  if (_path != nullptr) unlink(_path);
}

// Instantiate needed templates
template class UnixDomainSocket<command, int>;
