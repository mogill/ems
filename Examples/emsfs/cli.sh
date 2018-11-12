#!/bin/bash

EMSFS_FILE=/tmp/EMSFS_example
EMSFS_SIZE=1000000000
EMSFS_DIMENSIONS=1000
EMSFS_N_THREADS=1

function start() {
    export EMSFS_INIT=true
    export EMSFS_FILE
    export EMSFS_SIZE
    export EMSFS_DIMENSIONS
    export EMSFS_N_THREADS
    ../../node_modules/.bin/electron "$@"
}

function restart() {
    export EMSFS_INIT=false
    export EMSFS_FILE
    export EMSFS_SIZE
    export EMSFS_DIMENSIONS
    export EMSFS_N_THREADS
    ../../node_modules/.bin/electron "$@"
}
