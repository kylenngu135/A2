CC := shell("if command -v zig &> /dev/null; then echo zig cc; else echo gcc; fi")

EXE_NAME := "pcMatrix"
C_FILES := "counter.c prodcons.c matrix.c pcmatrix.c"
C_FLAGS := "-pthread -I. -Wall -Wextra -Wno-int-conversion -D_GNU_SOURCE -fcommon -std=c11"

default:
    @just --list

compile:
    if [ ! -d bin ]; then mkdir bin; fi
    {{CC}} {{C_FLAGS}} -o ./bin/{{EXE_NAME}} {{C_FILES}}

run: compile
    ./bin/{{EXE_NAME}}
