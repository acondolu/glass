import socket
import struct
import sys

class Controller:
  @staticmethod
  def send(addr, cmd, session):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
      s.connect(addr)
      s.send(cmd + session)
      bs = s.recv(8)
      if len(bs) != 8:
        print("Wrong response from control")
        sys.exit(1)
      return (None, struct.unpack(">BBBBHH", bs)[4]) # TODO: return IP address too