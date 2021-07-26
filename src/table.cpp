// see https://github.com/Tessil/hopscotch-map

#include <tsl/hopscotch_map.h>

#include "net.h"
#include "table.h"

bool operator==(const Addr& lhs, const Addr& rhs) {
  return lhs.raw == rhs.raw;
  // return lhs.addr == rhs.addr && lhs.port == rhs.port;
}

tsl::hopscotch_pg_map<Addr2, init_data*> init_map;

tsl::hopscotch_pg_map<Addr, session_data*> state;
