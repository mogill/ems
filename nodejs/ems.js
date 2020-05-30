/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.6.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2017, Jace A Mogill.  All rights reserved.              |
 |                                                                             |
 |  Updated to replace NAN with N-API                                          |
 |  Copyright (c) 2019 Aleksander J Budzynowski.                               |
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
"use strict";
var fs = require("fs");
var child_process = require("child_process");
var EMS = require("bindings")("ems.node");
var EMSglobal;

// The Proxy object is built in or defined by Reflect
try {
    var EMS_Harmony_Reflect = require("harmony-reflect");
} catch (err) {
    // Not installed, but possibly not needed anyhow
}

//==================================================================
//  Convert the different possible index types into a linear EMS index
function EMSidx(indexes,    // Index given by application
                EMSarray) { // EMS array being indexed
    var idx = 0;
    if (indexes instanceof Array) {       //  Is a Multidimension array: [x,y,z]
        indexes.forEach(function (x, i) {
            idx += x * EMSarray.dimStride[i];
        });
    } else {
        if (!(typeof indexes === "number") && !EMSarray.useMap) {  //  If no map, only use integers
            console.log("EMS ERROR: Non-integer index used, but EMS memory was not configured to use a map (useMap)",
                indexes, typeof indexes, EMSarray.useMap);
            idx = -1;
        } else {   //  Is a mappable intrinsic type
            idx = indexes;
        }
    }
    return idx;
}


//==================================================================
//  Print a message to the console with a prefix indicating which
//  thread is printing
function EMSdiag(text) {
    console.log("EMStask " + this.myID + ": " + text);
}


//==================================================================
//  Co-Begin a parallel region, executing the function "func"
function EMSparallel() {
    EMSglobal.inParallelContext = true;
    var user_args = (arguments.length === 1?[arguments[0]]:Array.apply(null, arguments));
    var func = user_args.pop();  // Remove the function
    // Loop over remote processes, starting each of them
    this.tasks.forEach(function (task, taskN) {
        task.send({"taskN": taskN + 1, "args": user_args, "func": func.toString()});
        task.send({"taskN": taskN + 1, "args": [], "func": "function() { ems.barrier(); }"});
    });
    func.apply(null, user_args);  // Invoke on master process
    EMSbarrier();  // Wait for all processes to finish
    EMSglobal.inParallelContext = false;
}


//==================================================================
//  Execute the local iterations of a decomposed loop with
//  the specified scheduling.
function EMSparForEach(start,         // First iteration's index
                       end,           // Final iteration's index
                       loopBody,      // Function to execute each iteration
                       scheduleType,  // Load balancing technique
                       minChunk) {    // Smallest block of iterations
    var sched = scheduleType;
    var idx;
    if (typeof sched === "undefined") {
        sched = "guided";
    }
    if (typeof minChunk === "undefined") {
        minChunk = 1;
    }
    switch (scheduleType) {
        case "static":     // Evenly divide the iterations across threads
            var range = end - start;
            var blocksz = Math.floor(range / EMSglobal.nThreads) + 1;
            var s = blocksz * EMSglobal.myID;
            var e = blocksz * (EMSglobal.myID + 1);
            if (e > end) {
                e = end;
            }
            for (idx = s; idx < e; idx += 1) {
                loopBody(idx + start);
            }
            break;
        case "dynamic":
        case "guided":
        default:
            //  Initialize loop bounds, block size, etc.
            EMSglobal.loopInit(start, end, sched, minChunk);
            //  Do not enter loop until all threads have completed initialization
            //  If the barrier is present at the loop end, this may replaced
            //  with first-thread initialization.
            var extents;
            EMSbarrier();
            do {
                extents = EMSglobal.loopChunk();
                for (idx = extents.start; idx < extents.end; idx++) {
                    loopBody(idx);
                }
            } while (extents.end - extents.start > 0);
    }

    // Do not proceed until all iterations have completed
    // TODO: OpenMP offers NOWAIT to skip this barrier
    EMSbarrier();
}


//==================================================================
//  Start a Transaction
//  Input is an array containing EMS elements to transition from
//  Full to Empty:
//     arr = [ [ emsArr0, idx0 ], [ emsArr1, idx1, true ], [ emsArr2, idx2 ] ]
function EMStmStart(emsElems) {
    var MAX_ELEM_PER_REGION = 10000000000000;

    // Build a list of indexes of the elements to lock which will hold
    // the elements sorted for deadlock free acquisition
    var indicies = [];
    for (var i = 0; i < emsElems.length; ++i) indicies[i] = i;

    //  Sort the elements to lock according to a global ordering
    var sorted = indicies.sort(function (a, b) {
        return ( ((emsElems[a][0].regionN * MAX_ELEM_PER_REGION) + emsElems[a][1]) -
        ((emsElems[b][0].regionN * MAX_ELEM_PER_REGION) + emsElems[b][1]) )
    });

    //  Acquire the locks in the deadlock free order, saving the contents
    //  of the memory when it locked.
    //  Mark read-write data as Empty, and read-only data under a readers-writer lock
    var tmHandle = [];
    sorted.forEach(function (e) {
        var val;
        if (emsElems[e][2] === true) {
            val = emsElems[e][0].readRW(emsElems[e][1]);
        } else {
            val = emsElems[e][0].readFE(emsElems[e][1]);
        }
        tmHandle.push([emsElems[e][0], emsElems[e][1], emsElems[e][2], val])
    });
    return tmHandle;
}


//==================================================================
//  Commit or abort a transaction
//  The tmHandle contains the result from tmStart:
//    [ [ EMSarray, index, isReadOnly, origValue ], ... ]
function EMStmEnd(tmHandle,   //  The returned value from tmStart
                  doCommit) { //  Commit or Abort the transaction
    tmHandle.forEach(function (emsElem) {
        if (emsElem[2] === true) {
            // Is a read-only element
            emsElem[0].releaseRW(emsElem[1]);
        } else {
            if (doCommit) {
                // Is read-write, keep current value and mark Full
                emsElem[0].setTag(emsElem[1], true);
            } else {
                // Write back the original value and mark full
                emsElem[0].writeEF(emsElem[1], emsElem[3]);
            }
        }
    });
}


//==================================================================
function EMSreturnData(value) {
    if (typeof value === "object") {
        var retval;
        try {
            if (value.data[0] === "[" && value.data.slice(-1) === "]") {
                retval = eval(value.data);
            } else {
                retval = JSON.parse(value.data);
            }
        } catch (err) {
            // TODO: Throw error?
            console.log('EMSreturnData: Corrupt memory, unable to reconstruct JSON value of (' +
                value.data + ')\n');
            retval = undefined;
        }
        return retval;
    } else {
        return value;
    }
}


//==================================================================
//  Synchronize memory with storage
//
function EMSsync(emsArr) {
    return this.data.sync(emsArr);
}


//==================================================================
//  Convert an EMS index into a mapped key
function EMSindex2key(index) {
    if(typeof(index) !== "number") {
        console.log('EMSindex2key: Index (' + index + ') is not an integer');
        return undefined;
    }

    return this.data.index2key(index);
}


//==================================================================
//  Wrappers around Stacks and Queues
function EMSpush(value) {
    if (typeof value === "object") {
        return this.data.push(JSON.stringify(value), true);
    } else {
        return this.data.push(value);
    }
}

function EMSpop() {
    return EMSreturnData(this.data.pop());
}

function EMSdequeue() {
    return EMSreturnData(this.data.dequeue());
}

function EMSenqueue(value) {
    if (typeof value === "object") {
        return this.data.enqueue(JSON.stringify(value), true);   // Retuns only integers
    } else {
        return this.data.enqueue(value);   // Retuns only integers
    }
}


//==================================================================
//  Wrappers around Primitive AMOs
//  Translate EMS maps and multi-dimensional array indexes/keys
//  into EMS linear addresses
//  Apparently it is illegal to pass a native function as an argument
function EMSwrite(indexes, value) {
    var linearIndex = EMSidx(indexes, this);
    if (typeof value === "object") {
        this.data.write(linearIndex, JSON.stringify(value), true);  
    } else {
        this.data.write(linearIndex, value);
    }
}

function EMSwriteEF(indexes, value) {
    var linearIndex = EMSidx(indexes, this);
    if (typeof value === "object") {
        this.data.writeEF(linearIndex, JSON.stringify(value), true);
    } else {
        this.data.writeEF(linearIndex, value);
    }
}

function EMSwriteXF(indexes, value) {
    var linearIndex = EMSidx(indexes, this);
    if (typeof value === "object") {
        this.data.writeXF(linearIndex, JSON.stringify(value), true);
    } else {
        this.data.writeXF(linearIndex, value);
    }
}

function EMSwriteXE(indexes, value) {
    var nativeIndex = EMSidx(indexes, this);
    if (typeof value === "object") {
        this.data.writeXE(nativeIndex, JSON.stringify(value), true);
    } else {
        this.data.writeXE(nativeIndex, value);
    }
}

function EMSread(indexes) {
    return EMSreturnData(this.data.read(EMSidx(indexes, this)))
}

function EMSreadFE(indexes) {
    return EMSreturnData(this.data.readFE(EMSidx(indexes, this)))
}

function EMSreadFF(indexes) {
    return EMSreturnData(this.data.readFF(EMSidx(indexes, this)))
}

function EMSreadRW(indexes) {
    return EMSreturnData(this.data.readRW(EMSidx(indexes, this)))
}

function EMSreleaseRW(indexes) {
    return this.data.releaseRW(EMSidx(indexes, this))
}

function EMSsetTag(indexes, fe) {
    return this.data.setTag(EMSidx(indexes, this), fe)
}

function EMSfaa(indexes, val) {
    if (typeof val === "object") {
        console.log("EMSfaa: Cannot add an object to something");
        return undefined;
    } else {
        return this.data.faa(EMSidx(indexes, this), val);  // No returnData(), FAA can only return JSON primitives
    }
}

function EMScas(indexes, oldVal, newVal) {
    if (typeof newVal === "object") {
        console.log("EMScas: ERROR -- objects are not a valid new type");
        return undefined;
    } else {
        return this.data.cas(EMSidx(indexes, this), oldVal, newVal);
    }
}


//==================================================================
//  Serialize execution through this function
function EMScritical(func, timeout) {
    if (typeof timeout === "undefined") {
        timeout = 500000;  // TODO: Magic number -- long enough for errors, not load imbalance
    }
    EMSglobal.criticalEnter(timeout);
    var retObj = func();
    EMSglobal.criticalExit();
    return retObj
}


//==================================================================
//  Perform func only on thread 0
function EMSmaster(func) {
    if (this.myID === 0) {
        return func();
    }
}


//==================================================================
//  Perform the function func once by the first thread to reach 
//  the function.  The final barrier is required because  a
//  thread may try to execute the next single-execution region
//  before other threads have finished this region, which the EMS
//  runtime cannot tell apart.  Barriers are phased, so a barrier
//  is used to prevent any thread from entering the next single-
//  execution region before this one is complete
function EMSsingle(func) {
    var retObj;
    if (this.singleTask()) {
        retObj = func();
    }
    EMSbarrier();
    return retObj
}


//==================================================================
//  Wrapper around the EMS global barrier
function EMSbarrier(timeout) {
    if (EMSglobal.inParallelContext) {
        if(typeof timeout === "undefined") {
            timeout = 500000;  // TODO: Magic number -- long enough for errors, not load imbalance
        }
        var remaining_time = EMS.barrier.call(EMSglobal, timeout);
        if (remaining_time < 0) {
            console.log("EMSbarrier: ERROR -- Barrier timed out after", timeout, "iterations.");
            // TODO: Probably should throw an error
        }
        return remaining_time;
    }
    return timeout;
}


//==================================================================
//  Utility functions for determining types
function EMSisArray(a) {
    return typeof a.pop !== "undefined"
}

function EMSisObject(o) {
    return typeof o === "object" && !EMSisArray(o)
}

function EMSisDefined(x) {
    return typeof x !== "undefined"
}


//==================================================================
//  Release all resources associated with an EMS memory region
function EMSdestroy(unlink_file) {
    EMSbarrier();
    if (EMSglobal.myID == 0) {
        this.data.destroy(unlink_file);
    }
    EMSbarrier();
}


//==================================================================
//  Creating a new EMS memory region
function EMSnew(arg0,        //  Maximum number of elements the EMS region can hold
                heapSize,    //  #bytes of memory reserved for strings/arrays/objs/maps/etc
                filename     //  Optional filename for persistent EMS memory
) {
    var fillIsJSON = false;
    var emsDescriptor = {    //  Internal EMS descriptor
        nElements: 1,     // Required: Maximum number of elements in array
        heapSize: 0,     // Optional, default=0: Space, in bytes, for strings, maps, objects, etc.
        mlock: 0,   // Optional, 0-100% of EMS memory into RAM
        useMap: false, // Optional, default=false: Use a map from keys to indexes
        useExisting: false, // Optional, default=false: Preserve data if a file already exists
        ES6proxies: false, // Optional, default=false: Inferred EMS read/write syntax
        persist: true,  // Optional, default=true: Preserve the file after threads exit
        doDataFill: false, // Optional, default=false: Data values should be initialized
        dataFill: undefined,//Optional, default=false: Value to initialize data to
        doSetFEtags: false, // Optional, initialize full/empty tags
        setFEtagsFull: true, // Optional, used only if doSetFEtags is true
        dimStride: []     //  Stride factors for each dimension of multidimensional arrays
    };

    if (!EMSisDefined(arg0)) {  // Nothing passed in, assume length 1
        emsDescriptor.dimensions = [1];
    } else {
        if (EMSisObject(arg0)) {  // User passed in emsArrayDescriptor
            if (typeof arg0.dimensions !== "undefined") {
                if (typeof arg0.dimensions !== "object") {
                    emsDescriptor.dimensions = [ arg0.dimensions ]
                } else {
                    emsDescriptor.dimensions = arg0.dimensions
                }
            }
            if (typeof arg0.ES6proxies !== "undefined") {
                emsDescriptor.ES6proxies = arg0.ES6proxies;
            }
            if (typeof arg0.heapSize !== "undefined") {
                emsDescriptor.heapSize = arg0.heapSize;
            }
            if (typeof arg0.mlock !== "undefined") {
                emsDescriptor.mlock = arg0.mlock
            }
            if (typeof arg0.useMap !== "undefined") {
                emsDescriptor.useMap = arg0.useMap
            }
            if (typeof arg0.filename !== "undefined") {
                emsDescriptor.filename = arg0.filename
            }
            if (typeof arg0.persist !== "undefined") {
                emsDescriptor.persist = arg0.persist
            }
            if (typeof arg0.useExisting !== "undefined") {
                emsDescriptor.useExisting = arg0.useExisting
            }
            if(arg0.doDataFill) {
                emsDescriptor.doDataFill = arg0.doDataFill;
                if (typeof arg0.dataFill === "object") {
                    emsDescriptor.dataFill = JSON.stringify(arg0.dataFill);
                    fillIsJSON = true;
                } else {
                    emsDescriptor.dataFill = arg0.dataFill;
                }
            }
            if (typeof arg0.doSetFEtags !== "undefined") {
                emsDescriptor.doSetFEtags = arg0.doSetFEtags
            }
            if (typeof arg0.setFEtags !== "undefined") {
                if (arg0.setFEtags === "full") {
                    emsDescriptor.setFEtagsFull = true;
                } else {
                    emsDescriptor.setFEtagsFull = false;
                }
            }
            if (typeof arg0.hashFunc !== "undefined") {
                emsDescriptor.hashFunc = arg0.hashFunc
            }
        } else {
            if (EMSisArray(arg0)) { // User passed in multi-dimensional array
                emsDescriptor.dimensions = arg0
            } else {
                if (typeof arg0 === "number") { // User passed in scalar 1-D array length
                    emsDescriptor.dimensions = [arg0]
                } else {
                    console.log("EMSnew: Couldn't determine type of arg0", arg0, typeof arg0)
                }
            }
        }
        if (typeof heapSize === "number") {
            emsDescriptor.heapSize = heapSize;
            if (heapSize <= 0  &&  emsDescriptor.useMap) {
                console.log("Warning: New EMS array with no heap, disabling mapped keys");
                emsDescriptor.useMap = false;
            }
        }
        if (typeof filename === "string") {
            emsDescriptor.filename = filename
        }
    }

    // Compute the stride factors for each dimension of a multidimensional array
    for (var dimN = 0; dimN < emsDescriptor.dimensions.length; dimN++) {
        emsDescriptor.dimStride.push(emsDescriptor.nElements);
        emsDescriptor.nElements *= emsDescriptor.dimensions[dimN];
    }
    if (typeof emsDescriptor.dimensions === "undefined") {
        emsDescriptor.dimensions = [emsDescriptor.nElements];
    }

    // Name the region if a name wasn't given
    if (!EMSisDefined(emsDescriptor.filename)) {
        emsDescriptor.filename = "/EMS_region_" + this.newRegionN;
        emsDescriptor.persist = false;
    }

    if (emsDescriptor.useExisting) {
        try { fs.openSync(emsDescriptor.filename, "r"); }
        catch (err) {
            return;
        }
    }

    //  init() is first called from thread 0 to perform one-thread
    //  only operations (ie: unlinking an old file, opening a new
    //  file).  After thread 0 has completed initialization, other
    //  threads can safely share the EMS array.
    if (!emsDescriptor.useExisting && this.myID !== 0) EMSbarrier();
    emsDescriptor.data = this.init(emsDescriptor.nElements, emsDescriptor.heapSize,  // 0, 1
        emsDescriptor.useMap, emsDescriptor.filename,  // 2, 3
        emsDescriptor.persist, emsDescriptor.useExisting,  // 4, 5
        emsDescriptor.doDataFill, fillIsJSON, // 6, 7
        emsDescriptor.dataFill,  // 8
        emsDescriptor.doSetFEtags,  // 9
        emsDescriptor.setFEtagsFull,  // 10
        this.myID, this.pinThreads, this.nThreads,  // 11, 12, 13
        emsDescriptor.mlock);  // 14

    if (!emsDescriptor.useExisting && this.myID === 0) EMSbarrier();

    emsDescriptor.regionN = this.newRegionN;
    emsDescriptor.push = EMSpush;
    emsDescriptor.pop = EMSpop;
    emsDescriptor.enqueue = EMSenqueue;
    emsDescriptor.dequeue = EMSdequeue;
    emsDescriptor.setTag = EMSsetTag;
    emsDescriptor.write = EMSwrite;
    emsDescriptor.writeEF = EMSwriteEF;
    emsDescriptor.writeXF = EMSwriteXF;
    emsDescriptor.writeXE = EMSwriteXE;
    emsDescriptor.read = EMSread;
    emsDescriptor.readRW = EMSreadRW;
    emsDescriptor.releaseRW = EMSreleaseRW;
    emsDescriptor.readFE = EMSreadFE;
    emsDescriptor.readFF = EMSreadFF;
    emsDescriptor.faa = EMSfaa;
    emsDescriptor.cas = EMScas;
    emsDescriptor.sync = EMSsync;
    emsDescriptor.index2key = EMSindex2key;
    emsDescriptor.destroy = EMSdestroy;
    this.newRegionN++;
    EMSbarrier();

    // Wrap the object with a proxy
    if(emsDescriptor.ES6proxies) {
        // Setter/Getter methods for the proxy object
        // If the target is built into EMS use the built-in object,
        // otherwise read/write the EMS value without honoring the Full/Empty tag
        var EMSproxyHandler = {
            get: function(target, name) {
                if (name in target) {
                    return target[name];
                } else {
                    return target.read(name);
                }
            },
            set: function(target, name, value) {
                target.write(name, value);
                return true
            }
        };
        try {
            emsDescriptor = new Proxy(emsDescriptor, EMSproxyHandler);
        }
        catch (err) {
            // console.log("Harmony proxies not supported:", err);
        }
    }

    return emsDescriptor;
}


//==================================================================
//  EMS object initialization, invoked by the require statement
//
function ems_wrapper(nThreadsArg, pinThreadsArg, threadingType, filename) {
    var retObj = {tasks: []};

    // TODO: Determining the thread ID should be done via shared memory
    if (process.env.EMS_Subtask !== undefined ) {
        retObj.myID = parseInt(process.env.EMS_Subtask);
    } else {
        retObj.myID = 0;
    }

    var pinThreads = false;
    if (typeof pinThreadsArg === "boolean") {
        pinThreads = pinThreadsArg;
    }

    var nThreads;
    nThreads = parseInt(nThreadsArg);
    if (!(nThreads > 0)) {
        if (process.env.EMS_Ntasks !== undefined) {
            nThreads = parseInt(process.env.EMS_Ntasks);
        } else {
            console.log("EMS: Must declare number of nodes to use.  Input:" + nThreadsArg);
            process.exit(1);
        }
    }

    var domainName = "/EMS_MainDomain";
    if (filename) domainName = filename;
    //  All arguments are defined -- now do the EMS initialization
    retObj.data = EMS.initialize(0, 0, // 0= # elements, 1=Heap Size
        false, // 2 = useMap
        domainName, false, false,  // 3=name, 4=persist, 5=useExisting
        false, false, undefined,  //  6=doDataFill, 7=fillIsJSON, 8=fillValue
        false, false,  retObj.myID, //  9=doSetFEtags, 10=setFEtags, 11=EMS myID
        pinThreads, nThreads, 99);  // 12=pinThread,  13=nThreads, 14=pctMlock

    var targetScript;
    switch (threadingType) {
        case undefined:
        case "bsp":
            targetScript = process.argv[1];
            threadingType = "bsp";
            retObj.inParallelContext = true;
            break;
        case "fj":
            targetScript = "./EMSthreadStub";
            retObj.inParallelContext = false;
            break;
        case "user":
            targetScript = undefined;
            retObj.inParallelContext = false;
            break;
        default:
            console.log("EMS: Unknown threading model type:", threadingType);
            retObj.inParallelContext = false;
            break;
    }

    //  The master thread has completed initialization, other threads may now
    //  safely execute.
    if (targetScript !== undefined && retObj.myID === 0) {
        var emsThreadStub =
            "// Automatically Generated EMS Slave Thread Script\n" +
            "// To edit this file, see ems.js:emsThreadStub()\n" +
            "var ems = require(\"ems\")(parseInt(process.env.EMS_Ntasks));\n" +
            "process.on(\"message\", function(msg) {\n" +
            "    eval(\"func = \" + msg.func);\n" +
            "    func.apply(null, msg.args);\n" +
            "} );\n";
        fs.writeFileSync('./EMSthreadStub.js', emsThreadStub, {flag: 'w+'});
        process.env.EMS_Ntasks = nThreads;
        for (var taskN = 1; taskN < nThreads; taskN++) {
            process.env.EMS_Subtask = taskN;
            retObj.tasks.push(
                child_process.fork(targetScript,
                    process.argv.slice(2, process.argv.length)));
        }
    }

    retObj.nThreads = nThreads;
    retObj.threadingType = threadingType;
    retObj.pinThreads = pinThreads;
    retObj.domainName = domainName;
    retObj.newRegionN = 0;
    retObj.init = EMS.initialize;
    retObj.new = EMSnew;
    retObj.critical = EMScritical;
    retObj.criticalEnter = EMS.criticalEnter;
    retObj.criticalExit  = EMS.criticalExit;
    retObj.master = EMSmaster;
    retObj.single = EMSsingle;
    retObj.diag = EMSdiag;
    retObj.parallel = EMSparallel;
    retObj.barrier = EMSbarrier;
    retObj.parForEach = EMSparForEach;
    retObj.tmStart = EMStmStart;
    retObj.tmEnd = EMStmEnd;
    retObj.loopInit = EMS.loopInit;
    retObj.loopChunk = EMS.loopChunk;
    EMSglobal = retObj;
    EMSglobal.mmapID = retObj.data.mmapID;
    return retObj;
}

ems_wrapper.initialize = ems_wrapper;
module.exports = ems_wrapper;
