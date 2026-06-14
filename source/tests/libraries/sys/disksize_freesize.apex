import os
import math
import sys

function disksize_freesize()
    disk_total = sys.disksize(".")
    disk_free = sys.freesize(".")

    if disk_total == false or disk_free == false
        os.output("Error: Could not get disk info")
    else
        total_gb = math.round(disk_total / 1024 / 1024 / 1024, 2)
        free_gb = math.round(disk_free / 1024 / 1024 / 1024, 2)
        used_gb = math.round(total_gb - free_gb, 2)
        os.output("Total Disk Size: {total_gb} GB")
        os.output("Free Disk Space: {free_gb} GB")
        os.output("Used Disk Space: {used_gb} GB")

disksize_freesize()