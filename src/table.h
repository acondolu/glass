#pragma once
#include <tsl/hopscotch_map.h>
#include "net.h"

// see https://github.com/Tessil/hopscotch-map

// For SYN TCP packets
extern tsl::hopscotch_pg_map<Addr2, init_data*> init_map;

// For all other TCP packets
extern tsl::hopscotch_pg_map<Addr, session_data*> state;
