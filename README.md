# Union Buster


# redux before NESD '26

bigger sample sizes for each experiment: >500
Multiple files and pages instead of one file,page
    eliminates need for cache erase on host. 
    -> read_page gets a digit, encodes it into a set of files Xth page.
    -> spy_on checks all these files, spits out the timing.

    -> I plot the times for page expected to be in cache vs. those not expected to be in cache

Run on server with SSD?


create vmA.qcow:

sudo qemu-img create -f qcow2 -b base.qcow2 -F qcow2 vmA.qcow2