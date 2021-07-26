#include <cstdint>
#include <ctime>
#include <errno.h>
#include <iostream>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "netfilter.h"
#include "pipe.h"
#include "shutdown.h"
#include "utils.h"

/**
 * 
 */
void INThandler(int sig)
{
  Shutdown::exit();
}

int main(int argc, char *argv[])
{
  uint16_t queue_num;
  char *socket_file;
  // Parse command line arguments
  if (argc < 3)
  {
    fprintf(stderr, "Usage: %s [QUEUE_NUM] [SOCKET_FILE]\n", argv[0]);
    return 1;
  }
  socket_file = argv[2];
  try
  {
    queue_num = std::stoi(argv[1]);
  }
  catch (std::invalid_argument const &e)
  {
    std::cerr << "Bad input: QUEUE_NUM must be a positive integer" << std::endl;
    return 1;
  }
  catch (std::out_of_range const &e)
  {
    std::cerr << "Bad input: QUEUE_NUM must be a small number" << std::endl;
    return 1;
  }
  // Initialize random
  std::srand(std::time(NULL));
  // Initialize shutdown switch
  if (Shutdown::init() < 0)
  {
    std::cerr << "Fatal: could not init self pipe." << std::endl;
    return 1;
  }
  // Check root
  if (geteuid() != 0)
  {
    std::cerr << "Must be run as root." << std::endl;
    return EACCES;
  }
  // Initialize TCP handler
  if (Netfilter::init(queue_num, socket_file) < 0)
  {
    perror("Could not init sockets");
    return 1;
  }
  // Drop root privileges
  uid_t uid = exeUserId();
  uid = 0;
  if (uid > 0)
  {
    fprintf(stderr, "Dropping root privileges (target uid = %i)\n", uid);
    passwd *user = getpwuid(uid);
    if (user == NULL || !droproot(user))
    {
      perror("Could not drop root privileges");
      return errno;
    }
  }
  else if (uid < 0)
  {
    perror("Could not drop root privileges");
    return errno;
  }
  // Install handlers
  signal(SIGINT, INThandler);
  // Go!
  Netfilter::main_loop();
  Netfilter::destroy();
  return 0;
}
