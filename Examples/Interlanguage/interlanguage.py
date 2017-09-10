#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
 +-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.5   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2017, Jace A Mogill.  All rights reserved.                   |
 |                                                                             |
 | Redistribution and use in source and binary forms, with or without          |
 | modification, are permitted provided that the following conditions are met: |
 |    * Redistributions of source code must retain the above copyright         |
 |      notice, this list of conditions and the following disclaimer.          |
 |    * Redistributions in binary form must reproduce the above copyright      |
 |      notice, this list of conditions and the following disclaimer in the    |
 |      documentation and/or other materials provided with the distribution.   |
 |    * Neither the name of the Synthetic Semantics nor the names of its       |
 |      contributors may be used to endorse or promote products derived        |
 |      from this software without specific prior written permission.          |
 |                                                                             |
 |    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS      |
 |    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT        |
 |    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR    |
 |    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SYNTHETIC         |
 |    SEMANTICS LLC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,   |
 |    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,      |
 |    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR       |
 |    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF   |
 |    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     |
 |    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS       |
 |    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.             |
 |                                                                             |
 +-----------------------------------------------------------------------------+
"""
"""
    tl;dr  Start the Javascript first, then the Python
    --------------------------------------------------

    See interlanguage.js for more detailed description of starting processes
    sharing EMS memory

    NOTE ABOUT "STUCK" JOBS":
      Python runtime can get "stuck" and may not respond to Control-C
      but can still be interrupted with Control-Z or Control-\
      This "stuck" behavior is due to how the Python/C interface protects
      the Python runtime.
"""

import time
import sys
import random
sys.path.append("../../Python/")
import ems

# Initialize EMS: 1 process, no thread-core affinity, user provided parallelism
# Initialize EMS with 1 process, no thread-CPU affinity,
# "user" mode parallelism, and a unique namespace for EMS runtime
# ("pyExample") to keep the JS program distinct from Javascript EMS
# program also running.
ems.initialize(1, False, "user", "pyExample")


# The EMS array attributes must be the same by all programs sharing the array,
# with the exception of the the initial file creation which uses "useExisting:False"
# instead of "useExisting:True"
maxNKeys = 100
bytesPerEntry = 100  # Bytes of storage per key, used for key (dictionary word) itself
shared = ems.new({
    'useExisting': True,        # Connect the EMS memory created by JS, do not create a new memory
    'filename': "/tmp/interlanguage.ems",  # Persistent EMS array's filename
    'dimensions': maxNKeys,   # Maximum # of different keys the array can store
    'heapSize': maxNKeys * 100,  # 100 bytes of storage per key, used for key (dictionary word) itself
    'useMap': True             # Use a key-index mapping, not integer indexes
})


# ------------------------------------------------------------------------------------------
#  Begin Main Program
print """Start this program after the JS program.
If this Python appears hung and does not respond to ^C, also try ^\ and ^Z
----------------------------------------------------------------------------
"""

#  Initial synchronization with JS
#  Write a value to empty memory and mark it full
shared.writeEF("Py hello", "from Python")

# Wait for JS to start
print "Hello ", shared.readFE("JS hello")


def barrier(message):
    """Define a convenience function to synchronize with another process
    The JS side reverses the barrier names"""
    global shared
    naptime = random.random() + 0.5
    time.sleep(naptime)  # Delay 0.5-1.5sec to make synchronization apparent
    print "Entering Barrier:", message
    shared.writeEF("js side barrier", None)
    shared.readFE("py side barrier")
    print "Completed Barrier after delaying for", naptime, "seconds"
    print "------------------------------------------------------------------"


print "Trying out the new barrier by first napping for 1 second..."
time.sleep(1)
# ---------------------------------------------------------------------
barrier("Trying out the barrier utility function")
# ---------------------------------------------------------------------


# --------------------------------------------------------------------
# This barrier matches where JS has set up "shared.top", and is now waiting
# for Python to finish at the next barrier
barrier("Finished waiting for JS to finish initializing nestedObj")

print "Value of shared.nestedObj left by JS:", shared.nestedObj
try:
    shared.nestedObj.number = 987  # Like JS, setting attributes of sub-objects will not work
    print "ERROR -- This should not work"
    exit()
except:
    print "Setting attributes of sub-objects via native syntax will not work"

print "These two expressions are the same:", \
    shared["nestedObj"].read()["subobj"]["middle"], "or", \
    shared.read("nestedObj")["subobj"]["middle"]


# --------------------------------------------------------------------
barrier("JS and Py are synchronized at the end of working with nestedObj")
# --------------------------------------------------------------------


# Enter the shared counter loop
for count in xrange(100000):
    value = shared.faa("counter", 1)
    if count % 10000 == 0:
        print "Py iteration", count, "  Shared counter=", value
barrier("Waiting for Js to finish it's counter loop")


# --------------------------------------------------------------------
#  Ready to earn your Pro Card?

# Make JS wait while Python demonstrates a "create approach to language design"
shared.arrayPlusString = [1, 2, 3]  # Initialize arrayPlusString before starting next phase
barrier("Wait until it's time to glimpse into the future of Python")
try:
    dummy = shared.arrayPlusString + "hello"
    print "ERROR -- Should result in:   TypeError: can only concatenate list (not \"str\") to list"
    exit()
except:
    print "Adding an array and string is an error in Python"

shared.arrayPlusString += "hello"
print "However 'array += string' produces an array", shared.arrayPlusString

# Let JS know 'shared.arrayPlusString' was initialized
barrier("shared.arrayPlusString is now initialized")
# JS does it's work now, then wait for JS to do it's work
barrier("Arrive at the future.")
# JS does all the work in this section
barrier("Welcome to The Future™!")


# --------------------------------------------------------------------
# JS is now in it's event loop with the counter running
for sampleN in xrange(10):
    time.sleep(random.random())
    print "Shared counter is now", shared.counter

# Restart the counter 
shared.counter = 0
for sampleN in xrange(5):
    time.sleep(random.random() + 0.5)
    print "Shared reset counter is now", shared.counter


# Wait 1 second and stop the JS timer counter
time.sleep(1)
shared.counter = "stop"

# Show the counter has stopped.  Last value = "stop" + 1
for sampleN in xrange(5):
    time.sleep(random.random() + 0.5)
    print "Shared reset counter should have stopped changing:", shared.counter

print "Exiting normally."
