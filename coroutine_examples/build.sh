#!/bin/bash

set -e -v

export CXX_ARGS="-std=c++2a -fcoroutines-ts -stdlib=libc++"

clang++ $CXX_ARGS generator_test.cpp -o generator_test
./generator_test

clang++ $CXX_ARGS task_test.cpp -o task_test
./task_test

clang++ $CXX_ARGS blocking_wait_test.cpp -o blocking_wait_test
./blocking_wait_test
