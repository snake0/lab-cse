cmake_minimum_required(VERSION 3.12)
project(lab1)

set(CMAKE_CXX_STANDARD 98)

include_directories(.)
include_directories(lang)
include_directories(rpc)

add_executable(lab1
        lang/algorithm.h
        lang/verify.h
        rpc/connection.h
        rpc/fifo.h
        rpc/jsl_log.h
        rpc/marshall.h
        rpc/method_thread.h
        rpc/pollmgr.h
        rpc/rpc.h
        rpc/slock.h
        rpc/thr_pool.h
        rpc/rpc.cc
        demo_client.cc
        demo_protocol.h
        demo_server.cc
        extent_client.cc
        extent_client.h
        extent_protocol.h
        extent_server.cc
        extent_server.h
        extent_smain.cc
        fuse.cc
        gettime.cc
        gettime.h
        handle.cc
        handle.h
        inode_manager.cc
        inode_manager.h
        lock_client.cc
        lock_client.h
        lock_client_cache.cc
        lock_client_cache.h
        lock_demo.cc
        lock_protocol.h
        lock_server.cc
        lock_server.h
        lock_server_cache.cc
        lock_server_cache.h
        lock_smain.cc
        lock_tester.cc
        test-lab-3-a.c
        test-lab-3-b.c
        test-lab2-part1-g.c
        test-lab2-part2-a.c
        test-lab2-part2-b.c
        tprintf.h
        yfs_client.cc
        yfs_client.h)
