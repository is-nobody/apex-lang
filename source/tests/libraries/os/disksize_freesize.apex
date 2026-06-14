import os
import math

function disksize_freesize()
    disk_total = os.disksize(".")
    disk_free = os.freesize(".")

    if disk_total == false or disk_free == false
        os.output("Error: Could not get disk info")
    else
        // Convert to MB for readability
        total_mb = math.round(disk_total / 1024 / 1024, 2)
        free_mb = math.round(disk_free / 1024 / 1024, 2)
        os.output("Total Disk Size: {total_mb} MB")
        os.output("Free Disk Space: {free_mb} MB")

disksize_freesize()