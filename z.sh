#!/usr/bin/env bash
./stop.sh
make
export RPC_LOSSY=5
./lock_server 3772 & ./lock_tester 3772