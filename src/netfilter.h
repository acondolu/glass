#pragma once
#include <cstdint>

namespace Netfilter {
  int init(uint16_t queue_num, char* socket_file);
  void main_loop();
  void destroy();
}
