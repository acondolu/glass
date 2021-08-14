#include "config.h"
#include "net.h"
#include "pipe.h"
#include "socket.h"
#include "translator.h"

namespace Control {
int init(Config::config *, Pipe<Translator::cmd> *);
void run();
void destroy();
}  // namespace Control
