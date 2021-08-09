#pragma once
#include <cstdint>
#include "config.h"

namespace Netfilter {
  int init(Config::config*);
  void main_loop();
  void destroy();
}
