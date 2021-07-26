#pragma once

template<typename T>
class Pipe {
  private:
  int fd_out = 0;
  int nbytes;
  unsigned char read_buf[sizeof(T)] __attribute__ ((aligned));
  unsigned char write_buf[sizeof(T)] __attribute__ ((aligned));
  int (*_callback)(T*);

  public:
  int fd_in = 0;
  Pipe(int (*callback)(T*));
  ~Pipe();
  int init();
  int handle_in();
  int enqueue(T*);
};
