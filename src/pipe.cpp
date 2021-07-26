#include <cstring>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "pipe.h"

#include "net.h"
#include "table.h"

template<typename T>
Pipe<T>::Pipe(int (*callback)(T*)) {
  _callback = callback;
}

template<typename T>
int Pipe<T>::init() {
  int p[2];
  if (pipe(p) < 0) {
    perror("Pipe::init()");
    return errno;
  }
  fd_out = p[1];
  Pipe::fd_in = p[0];
  nbytes = 0;
  return 0;
}

template<typename T>
Pipe<T>::~Pipe() {
  if (fd_out) close(fd_out);
  if (fd_in) close(fd_in);
}

template<typename T>
int Pipe<T>::handle_in() {
  int rb = read(fd_in, read_buf + nbytes, sizeof(T) - nbytes);
  if (rb < 0) return rb;
  nbytes += rb;
  if (nbytes == sizeof(T)) {
    nbytes = 0;
    _callback((T*) read_buf);
  }
  return 0;
}

template<typename T>
int Pipe<T>::enqueue(T* t) {
  // memcpy(write_buf, cmd, sizeof(Pipe::command));
  return write(fd_out, t, sizeof(T));
}

// Instantiate needed templates
template class Pipe<command>;
template class Pipe<bool>;
