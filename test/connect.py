#!/usr/bin/env python3

import socket
import sys

if __name__ == "__main__":
  if len(sys.argv) != 2:
    print("Usage: %s [TARGET_PORT]")
    sys.exit(1)
  else:
    [_, dest] = sys.argv
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
      addr = ("127.0.0.1", int(dest))
      s.connect(addr)
      s.send(b"test")
      r = s.recv(4)
      assert r == b"test"
    finally:
      s.close()
