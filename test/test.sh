#!/bin/bash
set -euxo pipefail

sudo ../out/glass-trans 6 commands.sock &
sleep 1
sudo python3 sendcmd.py commands.sock 61000 61001
python3 connect.py 61000 &
python3 connect.py 61001
sudo pkill --signal SIGINT glass-trans || true
pkill -f "python3 connect.py" || true