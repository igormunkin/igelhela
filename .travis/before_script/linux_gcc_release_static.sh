#!/usr/bin/env bash

cmake -H. -B${BUILD_DIR} \
    -DCMAKE_BUILD_TYPE=Release \
    -DUJIT_TEST_LIB_TYPE=static \
    -DUJIT_TEST_OPTIONS="-O4"
