#pragma once

namespace Config {

struct config {
  // int port_start;
  // int port_end;
  int nfqueue_number;
  char *unix_socket;
};

config *parseConfig(char const *jsonpath);

}  // namespace Config