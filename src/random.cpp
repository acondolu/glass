#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define M 1024

class Random {
  unsigned char* buf;
  size_t p;
  FILE* fd;
  
  void populate() {
    if (fread(buf, M, 1, fd) < 0) exit(1);
    p = M;
  }

  public:
  Random() {
    fd = fopen("/dev/urandom", "r");
    if (fd < 0) exit(1);
    buf = (unsigned char*) malloc(M);
    populate();
  }

  void read(unsigned char* x, size_t num_bytes) {
    if (p < num_bytes) {
      if (num_bytes > M) {
        printf("Random pool is smaller than requested randomness.");
        exit(1);
      } else {
        populate();
      }
    }
    memcpy(x, buf + (M - p), num_bytes);
    p -= num_bytes;
  }

  ~Random() {
    fclose(fd);
    free(buf);
  }
};
