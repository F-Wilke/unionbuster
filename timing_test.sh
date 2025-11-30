#!/bin/sh

#read first argument 
TARGET_PAGE=$1
MEASURE_PAGE_COUNT=$2

# repeat 100 times, print output only for first and last iteration
echo -e "\n---> [infected_1]: ./read_page /usr/sbin/nginx-debug $TARGET_PAGE"

for i in $(seq 1 100); do
    if [ $i -eq 1 ] || [ $i -eq 100 ]; then
        echo -e "\nIteration $i:"
        LD_BIND_NOW=1 ./read_page /usr/sbin/nginx-debug $TARGET_PAGE
    else
        LD_BIND_NOW=1 ./read_page /usr/sbin/nginx-debug $TARGET_PAGE > /dev/null
    fi
done

echo "========> Last page of /usr/sbin/nginx-debug is now loaded in the page cache."
echo -e "\n---> [infected_2]: ./spy_on /usr/sbin/nginx-debug"
LD_BIND_NOW=1 ./spy_on /usr/sbin/nginx-debug $MEASURE_PAGE_COUNT
echo -e "========> infected_2 has received the message from infected_1.\n"