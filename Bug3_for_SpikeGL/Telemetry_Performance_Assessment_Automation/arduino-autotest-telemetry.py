
#
# This script streams the telemetry output from the FlySim arduino and saves it to file.
#

import serial
from tkinter import *
from tkinter import filedialog
from time import sleep, localtime, strftime

if __name__ == "__main__":
    
    ser = None
    try:
        ser = serial.Serial('COM6', 9600)
    except:
        print("Could not connect to FlySim Arduino on port COM 6")
    
    Tk().withdraw() # we don't want a full GUI, so keep the root window from appearing
    with filedialog.asksaveasfile(filetypes = (("Arduino Position Log", \
        "'*.arduino.position.log'"), ("all files","*.*"))) as f:
    
        print("Saving to file: "+str(f.name))
        
        while True:
            try:
                line = ser.readline()
                t = strftime("%Y-%m-%d %H:%M:%S.%f", localtime())
                f.write(t + ',' + line + '\n')
            except:
                print("Can't read from Arduino, retrying...'")
                sleep(1.0)
