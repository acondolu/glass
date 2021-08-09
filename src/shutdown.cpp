#include <cstdlib>

#include "shutdown.h"
#include "pipe.h"

Pipe<void> p = Pipe<void>();

int Shutdown::fd_in;

int Shutdown::init() {
  int ret = p.init();
  if (ret < 0) return ret;
  fd_in = p.fd_in;
  return 0;
}

void Shutdown::exit() {
  if (p.enqueue(nullptr) < 0) {
    // Kind shutdown has failed. This should never happen.
    // Well, there's nothing else to do than just exit.
    std::exit(1);
  }
}
