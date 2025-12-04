#!/bin/sh

#read first argument 
TARGET_PAGE=$1
MEASURE_PAGE_COUNT=$2
TARGET_FILE="/usr/sbin/nginx-debug"

#use $3 for target file if provided
if [ ! -z "$3" ]; then
    TARGET_FILE="$3"
fi

# repeat 100 times, print output only for first and last iteration
echo -e "\n---> [infected_1]: ./read_page $TARGET_FILE $TARGET_PAGE"

for i in $(seq 1 100); do
    if [ $i -eq 1 ] || [ $i -eq 100 ]; then
        echo -e "\nIteration $i:"
        LD_BIND_NOW=1 ./read_page $TARGET_FILE $TARGET_PAGE
    else
        LD_BIND_NOW=1 ./read_page $TARGET_FILE $TARGET_PAGE > /dev/null
    fi
    echo 1 | sudo tee -a /proc/sys/vm/drop_caches
done

echo "========> Last page of $TARGET_FILE is now loaded in the page cache."
echo -e "\n---> [infected_2]: ./spy_on $TARGET_FILE"
LD_BIND_NOW=1 ./spy_on $TARGET_FILE $MEASURE_PAGE_COUNT
echo -e "========> infected_2 has received the message from infected_1.\n"