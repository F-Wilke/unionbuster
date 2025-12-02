#!/usr/bin/bash
# Script to reproduce and plot the main results locally.
#
# Author: Novak Boskov <boskov@bu.edu>
# Date: February 2022.

TARGET_PAGE=0 # default value

#overload default values if arguments are provided
if [ ! -z "$1" ]; then
    TARGET_PAGE=$1
fi

RUNTIME=runsc

#overload default runtime if provided
if [ ! -z "$2" ]; then
    RUNTIME=$2
fi


set -e

docker --version

ITERS=10

page0_cycles=()
page1_cycles=()

for ((i=1; i<=$ITERS; i++)); do

    echo "Running the two containers..."
    IMAGE_PATH=union-buster:latest
    CONTAINER_NAMES=(infected_1 infected_2)
    for n in ${CONTAINER_NAMES[@]}; do
        echo "sudo docker run --runtime=$RUNTIME -d --name ${n} $IMAGE_PATH"
        sudo docker run --runtime=$RUNTIME -d --name ${n} $IMAGE_PATH
    done

    echo -e "\n------------------------------ CONTAINERS ------------------------------"
    sudo docker container ls
    echo -e "------------------------------------------------------------------------\n"

    echo -e "\nEvicting page cache on the host..."
    sync
    echo 1 | sudo tee -a /proc/sys/vm/drop_caches
    echo "========> Entire OS Page Cache is now evicted."

    echo -e "\n---> [infected_1]: /workspace/read_page /usr/sbin/nginx-debug $TARGET_PAGE"
    sudo docker exec -it infected_1 /workspace/read_page /usr/sbin/nginx-debug $TARGET_PAGE
    echo "========> page $TARGET_PAGE of /usr/sbin/nginx-debug is now loaded in the page cache."
    echo -e "\n---> [infected_2]: /workspace/spy_on_diff  /usr/sbin/nginx-debug"
    output=$(sudo docker exec -it infected_2 /workspace/spy_on_diff  /usr/sbin/nginx-debug) 
    echo "$output"
    echo -e "========> infected_2 has received the message from infected_1.\n"

    page0=$(echo "$output" | grep "Page 0:" | awk '{print $3}')
    page1=$(echo "$output" | grep "Page 1:" | awk '{print $3}')

    page0_cycles+=("$page0")
    page1_cycles+=("$page1")

    echo "Cleanup..."
    sudo docker rm -f infected_1 infected_2
    echo -e "Done.\n"

done

p0_str="${page0_cycles[*]}"
p1_str="${page1_cycles[*]}"

read avg0 std0 <<< $(echo "$p0_str" | awk '{
    sum=0; sumsq=0; n=NF;
    for(i=1;i<=NF;i++){sum+=$i; sumsq+=$i*$i}
    mean=sum/n; std=sqrt(sumsq/n - mean*mean);
    print mean, std
}')

read avg1 std1 <<< $(echo "$p1_str" | awk '{
    sum=0; sumsq=0; n=NF;
    for(i=1;i<=NF;i++){sum+=$i; sumsq+=$i*$i}
    mean=sum/n; std=sqrt(sumsq/n - mean*mean);
    print mean, std
}')

echo "Page 0: mean=$avg0, std=$std0"
echo "Page 1: mean=$avg1, std=$std1"

title=""
out=""

if [ "$TARGET_PAGE" -eq 0 ]; then
    title="Priming Page 0"
    out="p0_prime.png"
elif [ "$TARGET_PAGE" -eq 1 ]; then
    title="Priming Page 1"
    out="p1_prime.png"
else
    title="No Priming"
    out="no_prime.png"
fi

python3 plot_gVisor.py --p0 "$avg0" --p1 "$avg1" --std0 "$std0" --std1 "$std1" --title "$title" --out "$out"
