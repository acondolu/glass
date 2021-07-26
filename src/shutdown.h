// Self-pipe trick http://cr.yp.to/docs/selfpipe.html

namespace Shutdown {
  extern int fd_in;
  int init();
  void exit();
}
