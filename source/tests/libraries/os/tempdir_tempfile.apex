import os

function tempdir()
    temp_dir = os.tempdir()
    if temp_dir == false
        os.output("Error: Could not get temp directory")
    else
        os.output("Temp Directory: {temp_dir}")

function tempfile()
    temp_file = os.tempfile()
    if temp_file == false
        os.output("Error: Could not create temp file")
    else
        os.output("Created Temp File: {temp_file}")
        
        if os.exists(temp_file)
            os.output("Verification: File exists")
        else
            os.output("Verification: File NOT found (error)")
        
        os.rmfile(temp_file)
        if os.exists(temp_file) == false
            os.output("Cleanup: File deleted successfully")

tempdir()
tempfile()