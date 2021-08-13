#pragma once
#include "net.h"


namespace Translator {
  // Internal commands
  enum ICMD_TAG {
    LINK, EXPIRE_LISTEN, EXPIRE_CONN
  };

  struct cmd_expire_listen {
    Addr2 a0;
    Addr2 a1;
  };
  constexpr int EXPIRE_LISTEN_SECS = 60;
  constexpr int EXPIRE_CONN_SECS = 60;

  struct cmd {
    cmd(ICMD_TAG tag): tag(tag) {};
    ICMD_TAG tag;
    union {
      command link;
      cmd_expire_listen expire_listen;
      Addr expire_conn;
    };
  };
}
