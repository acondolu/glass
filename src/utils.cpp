#include "utils.h"

#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

uid_t exeUserId() {
  size_t buff_len;
  static const size_t dest_len = 1000;  // TODO
  char path[dest_len];

  if ((buff_len = readlink("/proc/self/exe", path, dest_len - 1)) == -1lu)
    return -1;
  path[buff_len] = '\0';
  struct stat info;
  if (stat(path, &info) < 0) {
    return -1;
  }
  return info.st_uid;
}

/**
 * Drop root privileges and chroot if necessary
 */
bool droproot(const passwd *pw) {
  return initgroups(pw->pw_name, pw->pw_gid) == 0 && setgid(pw->pw_gid) == 0 &&
         setuid(pw->pw_uid) == 0;
}
