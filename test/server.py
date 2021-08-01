#!/usr/bin/env python3

import socket
import socketserver
import struct
import sys

# globals
table = None
ic = None

def ipToBytes(ip):
  return bytes(map(int, ip.split('.')))

class InternalCommands:
  def __init__(self, path):
    msgSock = self.msgSock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    self.__send = lambda msg: msgSock.sendto(msg, path)

  def link(self, addr0, p0, addr1, p1):
    msg = \
      struct.pack(">BBBBHH", addr0[0], addr0[1], addr0[2], addr0[3], p0, 0) \
      + struct.pack(">BBBBHH", addr1[0], addr1[1], addr1[2], addr1[3], p1, 0)
    self.__send(msg)
  
  def __del__(self):
    self.msgSock.close()

class Table:
  def __init__(self, portA, portB):
    self.curPort = portA
    self.lastPort = portB
    self.table = {}

  def bind(self, session, host, conn):
    t = self.table
    if session not in t:
      t[session] = []
    t[session].append((host, conn))

  def conn(self, session, host0, conn0):
    t = self.table
    if session not in t:
      return
    bs = t[session]
    (host1, conn1) = bs.pop()
    if not bs:
      del t[session]
    #
    p0 = self.curPort
    p1 = self.curPort + 1
    self.curPort += 2
    if self.curPort > self.lastPort:
      print("Ports are over")
      sys.exit(1)
    #
    global ic
    ic.link(host0, p0, host1, p1)
    ip = ipToBytes(conn0.getsockname()[0])
    conn1.sendall(struct.pack(">BBBBHH", ip[0], ip[1], ip[2], ip[3], p0, 0))
    conn0.sendall(struct.pack(">BBBBHH", ip[0], ip[1], ip[2], ip[3], p1, 0))
    return p1

class ThreadedTCPRequestHandler(socketserver.BaseRequestHandler):
  def handle(self):
    addr = ipToBytes(self.client_address[0])
    conn = self.request
    global table
    req = conn.recv(20)
    cmd = req[:4]
    session = req[4:]
    if cmd == b"BIND":
      table.bind(session, addr, conn)
      self.request = None
      # time.sleep(60) # hack
    elif cmd == b"CONN":
      table.conn(session, addr, conn)
      conn.close()
    else:
      print("cmd error")
      conn.close()

class ThreadedTCPServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
  def shutdown_request(self, request):
    pass # nothing, don't close connections !!!

if __name__ == "__main__":
  if len(sys.argv) != 5:
    print("Usage: %s [PORT] [CMD_SOCKET] [PORT-RANGE]")
    sys.exit(1)
  else:
    [_, port, cmdSocket, portA, portB] = sys.argv
  table = Table(int(portA), int(portB))
  ic = InternalCommands(cmdSocket)

  print("Serving on port %s..." % port)
  with ThreadedTCPServer(('0.0.0.0', int(port)), ThreadedTCPRequestHandler) as server:
    server.serve_forever()
