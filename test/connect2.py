#!/usr/bin/env python3

import os
import struct
import sys

from controller import Controller

if __name__ == "__main__":
  if len(sys.argv) != 4:
    print("Usage: %s [TARGET_PORT] [CMD] [SESSION]")
    sys.exit(1)
  else:
    [_, dest, cmd, session] = sys.argv
    (_ip, port) = Controller.send(("127.0.0.1", int(dest)), str.encode(cmd), str.encode(session))
    print("Port received from control: %d" % port)
    os.system("./connect.py %d" % port)
