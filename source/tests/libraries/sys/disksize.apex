import os
import math
import sys

function disksize()
    disk = sys.disksize()

    if disk == false
        os.output("Error: Could not get disk info")
    else
        os.output("Total Disk Size: {disk.total / 1024} GB")
        os.output("Free Disk Space: {disk.free / 1024} GB")
        os.output("Used Disk Space: {disk.used / 1024} GB")

disksize()