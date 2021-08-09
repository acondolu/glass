#pragma once
#include "net.h"

namespace Translator {
  // Internal commands
  enum ICMD_TAG {
    LINK, EXPIRE_LISTEN, EXPIRE_CONN
  };

  struct cmd {
    ICMD_TAG tag;
    union {
      command link;
      Addr2 expire_listen;
      Addr expire_conn;
    };
  };
}
