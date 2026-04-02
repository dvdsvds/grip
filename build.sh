#!/bin/bash
g++ -std=c++20 \
    -Iinclude \
    -Ithird_party \
    src/main.cpp \
    src/config.cpp \
    src/scanner.cpp \
    src/compiler.cpp \
    src/deps.cpp \
    src/thread_pool.cpp \
    src/http_client.cpp \
    src/installer.cpp \
    src/lock.cpp \
    src/test.cpp \
    src/process.cpp \
    -o grip
