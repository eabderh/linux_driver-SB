#!/bin/bash

sudo rm /dev/sb
sudo mknod -m666 /dev/sb c 240 0
sudo rmmod sb16driver.ko
sudo insmod ./sb16driver.ko

