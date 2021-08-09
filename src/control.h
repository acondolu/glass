#include "net.h"
#include "config.h"
#include "pipe.h"
#include "socket.h"
#include "translator.h"

namespace Control {
  int init(Config::config*, Pipe<Translator::cmd>*);
  void run();
  void destroy();
}
