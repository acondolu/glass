#!/bin/bash
set -euxo pipefail

export PID_GLASS_TRANS=
export PID_GLASS_CTRL=

function finish {
  echo "> Shutdown"
  pkill -f "connect.py" || true
  pkill -f "connect2.py" || true
  sudo kill --signal SIGINT $PID_GLASS_TRANS || true
  sudo kill $PID_GLASS_CTRL || true
}
trap finish EXIT

echo "Starting glass translator"
sudo ../out/glass-trans 6 commands.sock &
export PID_GLASS_TRANS=$!
sleep 1

echo "> 61000 <-> 61001"
sudo ./sendcmd.py commands.sock 61000 61001
./connect.py 61000 &
./connect.py 61001

echo "> Start control"
sudo ./server.py 2004 commands.sock 62000 63000 &
export PID_GLASS_CTRL=$!
sleep 1

echo "> Bind/Conn scenario"
./connect2.py 2004 BIND 0123456789ABCDEF &
sleep 1
./connect2.py 2004 CONN 0123456789ABCDEF
