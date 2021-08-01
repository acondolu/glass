#!/usr/bin/env python3

import socket
import struct
import sys

if __name__ == "__main__":
  [_, path, p0, p1] = sys.argv
  with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as s:
    msg = \
      struct.pack(">BBBBHH", 127, 0, 0, 1, int(p0), 0) \
      + struct.pack(">BBBBHH", 127, 0, 0, 1, int(p1), 0)
    s.sendto(msg, path)
