#!/bin/sh


echo -e "\nEvicting page cache on this machine..."
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
echo "========> Entire OS Page Cache is now evicted."

echo -e "\n---> [infected_1]: ./read_page ../../gvisor/bin/runsc 0"
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
./read_page ../../gvisor/bin/runsc 10

echo -e "\nEvicting page cache on this machine..."
sync
echo 1 | sudo tee -a /proc/sys/vm/drop_caches
echo "========> Entire OS Page Cache is now evicted."

./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10
./read_page ../../gvisor/bin/runsc 10

echo "========> Last page of /usr/sbin/nginx-debug is now loaded in the page cache."
echo -e "\n---> [infected_2]: ./spy_on ../../gvisor/bin/runsc"
./spy_on ../../gvisor/bin/runsc
echo -e "========> infected_2 has received the message from infected_1.\n"