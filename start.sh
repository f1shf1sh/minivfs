#! /bin/sh

dd if=/dev/zero of=disk.img bs=4M count=64
make clean
make all > /dev/null
./bin/mkfs disk.img
cd bin && ./sh ../disk.img
