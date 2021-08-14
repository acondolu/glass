#pragma once
#include <cstdio>
#include <systemd/sd-journal.h>

namespace Logging {

template <typename... Args> inline void info(Args... args) {
  sd_journal_print(LOG_NOTICE, args...);
}

template <typename... Args> inline void debug(Args... args) {
  sd_journal_print(LOG_DEBUG, args...);
}

template <typename... Args> inline void error(Args... args) {
  sd_journal_print(LOG_ERR, args...);
}

template <typename... Args> inline void fatal(Args... args) {
  sd_journal_print(LOG_ERR, args...);
  fprintf(stderr, "FATAL: ");
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
  fprintf(stderr, args...);
#pragma GCC diagnostic pop
  fprintf(stderr, "\n");
}
}
