#!/usr/bin/env python3
"""Drive the standalone FAS test over native-USB serial.
Usage: fas_drive.py <port> <read_seconds> [cmd1] [delay1] [cmd2] [delay2] ...
Sends each cmd (followed by \n), waits delayN seconds reading, repeats,
then reads the remaining time. Prints everything received.
"""
import sys, time, serial

port = sys.argv[1]
total = float(sys.argv[2])
# remaining args are (cmd, delay) pairs
pairs = sys.argv[3:]

ser = serial.Serial(port, 115200, timeout=0.2)
t0 = time.time()

def drain(duration):
    end = time.time() + duration
    while time.time() < end:
        data = ser.read(4096)
        if data:
            sys.stdout.write(data.decode("utf-8", "replace"))
            sys.stdout.flush()

# initial drain
drain(1.0)
i = 0
while i + 1 < len(pairs):
    cmd = pairs[i]
    delay = float(pairs[i+1])
    sys.stdout.write("\n>>> SEND: %r\n" % cmd)
    sys.stdout.flush()
    ser.write((cmd + "\n").encode())
    ser.flush()
    drain(delay)
    i += 2

# drain remaining
remaining = total - (time.time() - t0)
if remaining > 0:
    drain(remaining)

ser.close()
