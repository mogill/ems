/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.4   |
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
 +-----------------------------------------------------------------------------*/
/*
   Start this JS program first, then start the Python program

   Possible Orders for Starting Processes
   ---------------------------------------------

   A persistent EMS file already exists
     1. A one-time initialization process creates the EMS array
     2. Any number of processes my attach to the EMS array in
        any order.


   A new EMS array will be created
     1. The first program to start must create the EMS file with
        the "useExisting : false" attribute
     2. Subsequent programs attach to the new EMS file with the
        "useExisting : true" attribute

 */
"use strict";
let ems = require("ems")(1, false, "user");  // User mode parallelism -- no EMS parallel intrinsics

const maxNKeys = 100;
const bytesPerEntry = 100;  // Bytes of storage per key, used for key (dictionary word) itself
let shared = ems.new({
    "ES6proxies": true,  // Enable native JS object-like syntax
    "useExisting": false,      // Create a new EMS memory area, do not use existing one if present
    "filename": "interlanguage.ems",  // Persistent EMS array's filename
    "dimensions": maxNKeys,  // Maximum # of different keys the array can store
    "heapSize": maxNKeys *  bytesPerEntry,
    "setFEtags": "empty",  // Set default full/empty state of EMS memory to empty
    "doSetFEtags": true,
    "useMap": true            // Use a key-index mapping, not integer indexes
});



//------------------------------------------------------------------------------------------
//  Begin Main Program

// One-time initialization should be performed before syncing to Py
shared.writeXF("nestedObj", undefined);

// Initial synchronization with Python
// Write a value to empty memory and mark it full
shared.writeEF("JS hello", "from Javascript");

// Wait for Python to start
console.log("Hello " + shared.readFE("Py hello"));

/*
   This synchronous exchange was a "barrier" between the two processes.

   The idiom of exchanging messages by writeEF and readFE constitutes
   a synchronization point between two processes.  A barrier synchronizing
   N tasks would require N² variables, which is a reasonable way to
   implement a barrier when N is small.  For larger numbers of processes
   barriers can be implemented using shared counters and the Fetch-And-Add
   (FAA) EMS instruction.

   The initialization of EMS values as "empty" occurs when the EMS array
   was created with:
        "setFEtags": "empty",
        "doSetFEtags": true,
   If it becomes necessary to reset a barrier, writeXE() will
   unconditionally and immediately write the value and mark it empty.
 */


// Convenience function to synchronize with another process
// The Python side reverses the barrier names
function barrier(message) {
    console.log("Entering Barrier:", message);
    shared.writeEF("py side barrier", undefined);
    shared.readFE("js side barrier");
    console.log("Completed Barrier.");
    console.log("---------------------------------------------------");
}


// --------------------------------------------------------------------
barrier("Trying out the barrier utility function");
// --------------------------------------------------------------------


//  JS and Python EMS can read sub-objects with the same syntax as native objects,
//  but writes are limited to only top level attributes.  This corresponds to the
//  implied "emsArray.read(key).subobj" performed
shared.top = {};
console.log("Assignment to top-level attributes works normally:", shared.top);
shared.top.subobj = 1;  // Does not take effect like top-level attributes
console.log("But sub-object attributes do not take effect. No foo?", shared.top.subobj);

// A nested object can be written at the top level.
shared.nestedObj = {"one":1, "subobj":{"left":"l", "right":"r"}};
console.log("Nested object read references work normally", shared.nestedObj.subobj.left);

// A workaround to operating on nested objects
let nestedObjTemp = shared.nestedObj;
nestedObjTemp.subobj.middle = "m";
shared.nestedObj = nestedObjTemp;

// The explicitly parallel-safe way to modify objects is similar to that "workaround"
nestedObjTemp = shared.readFE("nestedObj");
nestedObjTemp.subobj.front = "f";
shared.writeEF("nestedObj", nestedObjTemp);


// --------------------------------------------------------------------
barrier("This barrier matches where Python is waiting to use nestedObj");
// --------------------------------------------------------------------

// Python does some work now
// Initialize the counter for the next section
shared.writeXF("counter", 0);

// --------------------------------------------------------------------
barrier("JS and Py are synchronized at the end of working with nestedObj");
// --------------------------------------------------------------------

// ---------------------------------------------------------------------------
//  Use the EMS atomic operation intrinsics to coordinate access to data
//  This is 10x as many iterations as Python in the same amount of time
//  because V8's TurboFan compiler kicks in.
for (let count = 0;  count < 1000000;  count += 1) {
    let value = shared.faa("counter", 1);
    if (count % 100000 == 0) {
        console.log("JS iteration", count, "  Shared Counter=", value)
    }
}


// --------------------------------------------------------------------
barrier("Waiting for Py to finish it's counter loop");
// --------------------------------------------------------------------

console.log("The shared counter should be 11000000 ==", shared.counter);



// ---------------------------------------------------------------------------
// Ready to earn your Pro Card?

// Wait for Py to initialize the array+string
barrier("Wait until it's time to glimpse into the future of Javascript");
console.log("The array+string from Py:", shared.arrayPlusString);

barrier("Waiting for shared.arrayPlusString to be initialized");
console.log("JS can do array+string, it produces a string:", shared.arrayPlusString + " world!");
shared.arrayPlusString += " RMW world!";
console.log("JS performing \"array + string\" produces a self-consistent string:", shared.arrayPlusString);
barrier("Arrive at the future.");


// ---------------------------------------------------------------------------
// Leave a counter running on a timer
// First re-initialize the counter to 0
shared.writeXF("counter", 0);

function incrementCounter() {
    // Atomically increment the shared counter
    let oldValue = shared.faa("counter", 1);

    // If the counter has been set to "Stop", clear the timer interval
    if (oldValue == "stop") {
        console.log("Timer counter was signaled to stop");
        clearInterval(timerCounter);
    }

    // Show counter periodically
    if (oldValue % 100  == 0) {
        console.log("Counter =", shared.counter);
    }
}

var timerCounter = setInterval(incrementCounter, 10); // Increment the counter every 1ms 

barrier("Welcome to The Future™!");
// Fall into Node.js event loop with Interval Timer running
