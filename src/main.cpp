#include <errno.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdint>
#include <ctime>
#include <iostream>
#include <string>

#include "config.h"
#include "control.h"
#include "logging.h"
#include "netfilter.h"
#include "pipe.h"
#include "shutdown.h"
#include "utils.h"

/**
 *
 */
void TERMhandler(int sig) { Shutdown::exit(); }

int main(int argc, char *argv[]) {
  // Parse command line arguments
  if (argc != 2) {
    fprintf(stderr, "Usage: %s [CONF.JSON]\n", argv[0]);
    return 1;
  }
  Config::config *cfg = Config::parseConfig(argv[1]);
  if (cfg == NULL) {
    Logging::fatal("Error parsing JSON file");
    return -1;
  }
  // Initialize random
  std::srand(std::time(NULL));
  // Initialize shutdown switch
  if (Shutdown::init() < 0) {
    Logging::fatal("Could not init self pipe");
    return 1;
  }
  // Check root
  if (geteuid() != 0) {
    Logging::fatal("Must be run as root");
    return EACCES;
  }
  // Initialize TCP handler
  if (Netfilter::init(cfg) < 0) {
    perror("Could not init sockets");
    return 1;
  }
  // Drop root privileges
  uid_t uid = exeUserId();
  uid = 0;
  if (uid > 0) {
    fprintf(stderr, "Dropping root privileges (target uid = %i)\n", uid);
    passwd *user = getpwuid(uid);
    if (user == NULL || !droproot(user)) {
      perror("Could not drop root privileges");
      return errno;
    }
  } else if (uid < 0) {
    perror("Could not drop root privileges");
    return errno;
  }
  // Install handlers
  signal(SIGTERM, TERMhandler);
  // Go!
  Control::run();
  Netfilter::main_loop();
  Netfilter::destroy();
  return 0;
}
