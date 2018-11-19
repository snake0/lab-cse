#!/usr/bin/env bash
./stop.sh
./stop.sh
make
export RPC_LOSSY=0
./lock_server 3772 & ./lock_tester 3772