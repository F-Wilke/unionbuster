#!/bin/bash
COMMAND="./reproduce_QEMU.sh 0"
TARGET_FILES=("../rand0.bin" "../rand1.bin")
VM_PATH="/home/fwilke/edu/BU/ec721"



# echo -e "\nStarting two VMs (vmA and vmB)..."

# sudo qemu-system-x86_64 \
# -enable-kvm \
# -m 2048 \
# -drive file=$VM_PATH/vmA.qcow2,if=virtio \
# -boot c \
# -nic user,hostfwd=tcp:127.0.0.1:2222-:22&

# sudo qemu-system-x86_64 \
# -enable-kvm \
# -m 2048 \
# -drive file=$VM_PATH/vmB.qcow2,if=virtio \
# -boot c \
# -nic user,hostfwd=tcp:127.0.0.1:2223-:22&


# sleep 20  # wait for VMs to boot up



for secret in {0..3}; do  
    for i in {1..100}; do
        #iterate over target files  
        for index in {0..1}; do
            TARGET_FILE=${TARGET_FILES[$index]}
            #prime file if the secret's bit at the index of the target file is 1
            ((bit = (secret >> index) & 1 ))
            if [ $bit -eq 1 ]; then
                echo -e "\nPriming file $TARGET_FILE for secret $secret (bit $index is 1)..."
                echo "$COMMAND $TARGET_FILE 1"
                $COMMAND $TARGET_FILE 1
            else
                echo "Not priming file $TARGET_FILE for secret $secret (bit $index is 0)."
                echo "$COMMAND $TARGET_FILE"
                $COMMAND $TARGET_FILE
            fi
        done
    done
done
