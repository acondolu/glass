#pragma once
#include <stdio.h>
#include <unistd.h>

#include <cstring>

template <typename T>
class Pipe {
 private:
  T *read_buf[1];
  int (*_callback)(T *);

 public:
  int fd_in = 0;
  int fd_out = 0;
  // The callback is responsible for freeing the pointer
  ~Pipe() {
    if (fd_out != 0) close(fd_out);
    if (fd_in != 0) close(fd_in);
  };
  // Returns 0 if successful, -1 if not.
  int init() {
    int p[2];
    if (pipe(p) < 0) {
      return -1;
    }
    Pipe::fd_out = p[1];
    Pipe::fd_in = p[0];
    return 0;
  };
  T *receive() {
    if (read(fd_in, (void *)read_buf, sizeof(T *)) != sizeof(T *))
      return nullptr;
    return *read_buf;
  };
  // enqueue takes ownership of t
  int enqueue(T *t) { return write(fd_out, &t, sizeof(T *)); };
};
