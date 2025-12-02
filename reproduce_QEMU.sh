#!/usr/bin/bash
# Script to reproduce the main results locally.
#
# Author: Novak Boskov <boskov@bu.edu>
# Date: February 2022.

TARGET_PAGE=0 # default value
TARGET_PATH="~/gvisor/bin/runsc" # default value

#overload default values if arguments are provided
if [ ! -z "$1" ]; then
    TARGET_PAGE=$1
fi

if [ ! -z "$2" ]; then
    TARGET_PATH=$2
fi

set -e

#To come: start two VMs


# connect to VM1 on localhost:2222
# connect to VM2 on localhost:2223


echo -e "\nEvicting page cache on the host..."
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
echo "========> Entire OS Page Cache is now evicted."

# run read_page /usr/sbin/nginx-debug $TARGET_PAGE in VM1
echo -e "\n---> [VM1]: ./read_page $TARGET_PATH $TARGET_PAGE"

ssh -p 2222 fwilke@localhost << EOF
echo 1 | sudo tee -a /proc/sys/vm/drop_caches

cd ~/unBust/union-buster
./read_page $TARGET_PATH $TARGET_PAGE
EOF


echo -e "\n---> [VM2]: ./spy_on_diff $TARGET_PATH"

ssh -p 2222 fwilke@localhost << EOF
echo 1 | sudo tee -a /proc/sys/vm/drop_caches

cd ~/unBust/union-buster
./spy_on_diff $TARGET_PATH $TARGET_PAGE
EOF

echo "Done."
