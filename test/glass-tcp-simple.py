#!/usr/bin/env python3

import base64
import os
import socket
import sys
from telnetlib import Telnet

from controller import Controller

def wrongUsage():
  print("Usage: %s [CTRL-IP] [CTRL-PORT] [MODE] ([SESSION])")
  sys.exit(1)

if __name__ == "__main__":
  if len(sys.argv) < 4:
    wrongUsage()
  else:
    ip = sys.argv[1]
    port = int(sys.argv[2])
    mode = sys.argv[3]
    if mode == "BIND":
      channel = os.urandom(16)
      print("Binding to channel:", base64.b16encode(channel).decode())
      (_ip, portX) = Controller.send((ip, port), b"BIND", channel)
    elif mode == "CONN":
      if len(sys.argv) != 5:
        wrongUsage()
      channel = base64.b16decode(str.encode(sys.argv[4]))
      (_ip, portX) = Controller.send((ip, port), b"CONN", channel)
    else:
      wrongUsage()
    print("Connected.")
    with Telnet(ip, portX) as tn:
      tn.interact()
