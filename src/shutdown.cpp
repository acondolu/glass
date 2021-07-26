#include <cstdlib>

#include "shutdown.h"
#include "pipe.h"

// calling p.handle_in() would make the program crash
Pipe<bool> p(nullptr);

int Shutdown::fd_in;

int Shutdown::init() {
  int ret = p.init();
  if (ret < 0) return ret;
  fd_in = p.fd_in;
  return 0;
}

void Shutdown::exit() {
  bool b;
  if (p.enqueue(&b) < 0) std::exit(1);
}
