/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.5   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2017, Jace A Mogill.  All rights reserved.              |
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
const {EMS_BINDINGS_FILE, EMS_MODULE_FILE} = process.env
const EMS = require("bindings")(EMS_BINDINGS_FILE);
const fs = require("fs");
const child_process = require("child_process");

var EMSglobal;

//==================================================================
//  Convert the different possible index types into a linear EMS index
function EMSidx(
    indexes,    // Index given by application
    EMSarray    // EMS array being indexed
) {
    if (indexes instanceof Array) { // Is a Multidimension array: [x,y,z]
        let idx = 0
        const l = indexes.length
        let i = 0
        while (i < l) {
            idx += indexes[i] * EMSarray.dimStride[i];
            i = i + 1
        }
        return idx
    }
    if (!(typeof indexes === "number") && !EMSarray.useMap) {  //  If no map, only use integers
        console.log("EMS ERROR: Non-integer index used, but EMS memory was not configured to use a map (useMap)",
            indexes, typeof indexes, EMSarray.useMap);
        return -1;
    }
    //  Is a mappable intrinsic type
    return indexes;
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
    const user_args = (arguments.length === 1)
        ? [arguments[0]]
        : Array.apply(null, arguments)
    ;
    const func = user_args.pop();  // Remove the function
    const func_string = func.toString()
    // Loop over remote processes, starting each of them
    const task0_opts = {"taskN": 0, "args": user_args, "func": func_string}
    const task1_opts = {"taskN": 0, "args": [],        "func": "function() { ems.barrier(); }"}
    const l = this.tasks.length
    var i = 0
    while (i < l) {
        const taskN = i++
        const task = this.tasks[taskN]
        task0_opts.taskN = task1_opts.taskN = taskN + 1
        task.send(task0_opts);
        task.send(task1_opts);
    }
    func.apply(null, user_args);  // Invoke on master process
    EMSbarrier();  // Wait for all processes to finish
    EMSglobal.inParallelContext = false;
}


//==================================================================
//  Execute the local iterations of a decomposed loop with
//  the specified scheduling.
function EMSparForEach(
    start,         // First iteration's index
    end,           // Final iteration's index
    loopBody,      // Function to execute each iteration
    scheduleType,  // Load balancing technique
    minChunk       // Smallest block of iterations
) {
    const sched = scheduleType === undefined ? 'guided' : scheduleType;
    (minChunk === undefined) && (minChunk = 1);
    const {
        nThreads,
        myID
    } = EMSglobal.nThreads
    switch (scheduleType) {
        case "static":     // Evenly divide the iterations across threads
            const range = end - start;
            const blocksz = ((range / nThreads)|0) + 1;
            const s = blocksz * myID;
            var e = blocksz * (myID + 1);
            if (e > end)
                e = end
            ;
            for (let idx = s; idx < e; idx = idx + 1)
                loopBody(idx + start)
            ;
            break;
        case "dynamic":
        case "guided":
        default:
            //  Initialize loop bounds, block size, etc.
            EMSglobal.loopInit(start, end, sched, minChunk);
            //  Do not enter loop until all threads have completed initialization
            //  If the barrier is present at the loop end, this may replaced
            //  with first-thread initialization.
            let extents;
            EMSbarrier();
            do {
                extents = EMSglobal.loopChunk();
                for (let idx = extents.start; idx < extents.end; idx = idx + 1)
                    loopBody(idx)
                ;
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
    const MAX_ELEM_PER_REGION = 10000000000000;
    // Build a list of indexes of the elements to lock which will hold
    // the elements sorted for deadlock free acquisition
    const l = emsElems.length
    var i = 0
    const indicies = [];
    while (i < l)
        indicies[i] = i++
    ;
    //  Sort the elements to lock according to a global ordering
    const sorted = indicies.sort(function (a, b) {
        return (
            ((emsElems[a][0].regionN * MAX_ELEM_PER_REGION) + emsElems[a][1]) -
            ((emsElems[b][0].regionN * MAX_ELEM_PER_REGION) + emsElems[b][1])
        )
    });
    //  Acquire the locks in the deadlock free order, saving the contents
    //  of the memory when it locked.
    //  Mark read-write data as Empty, and read-only data under a readers-writer lock
    const tmHandle = [];
    i = 0
    while (i < l) {
        const e = sorted[i]
        const val = (emsElems[e][2] === true)
            ? emsElems[e][0].readRW(emsElems[e][1])
            : emsElems[e][0].readFE(emsElems[e][1])
        ;
        tmHandle[i] = [emsElems[e][0], emsElems[e][1], emsElems[e][2], val]
        i++
    }
    return tmHandle;
}


//==================================================================
//  Commit or abort a transaction
//  The tmHandle contains the result from tmStart:
//    [ [ EMSarray, index, isReadOnly, origValue ], ... ]
function EMStmEnd(
    tmHandle,  //  The returned value from tmStart
    doCommit   //  Commit or Abort the transaction
) {
    const l = tmHandle.length
    var i = 0
    while (i < l) {
        const emsElem = tmHandle[i++]
        // Is a read-only element
        if (emsElem[2] === true)
            emsElem[0].releaseRW(emsElem[1])
        ;
        // Is read-write, keep current value and mark Full
        else if (doCommit)
            emsElem[0].setTag(emsElem[1], true)
        ;
        // Write back the original value and mark full
        else
            emsElem[0].writeEF(emsElem[1], emsElem[3])
        ;
    }
}


//==================================================================
function EMSreturnData(value) {
    if (typeof value !== "object")
        return value
    ;
    if (Buffer.isBuffer(value))
        return value
    ;
    try {
        if (value.data[0] === "[" && value.data.slice(-1) === "]")
            return eval(value.data)
        ;
        return JSON.parse(value.data)
    }
    catch (err) {
        // TODO: Throw error?
        console.log(
            'EMSreturnData: Corrupt memory, unable to reconstruct JSON value of (' +
            value.data + ')\n'
        );
    }
    return undefined;
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
    if (typeof index !== "number") {
        console.log('EMSindex2key: Index (' + index + ') is not an integer');
        return undefined;
    }
    return this.data.index2key(index);
}


//==================================================================
//  Wrappers around Stacks and Queues
function EMSpush(value) {
    if (typeof value === "object")
        return this.data.push(JSON.stringify(value), true)
    ;
    else
        return this.data.push(value)
    ;
}

function EMSpop() {
    return EMSreturnData(this.data.pop());
}

function EMSdequeue() {
    return EMSreturnData(this.data.dequeue());
}

function EMSenqueue(value) {
    if (typeof value === "object")
        return this.data.enqueue(JSON.stringify(value), true) // Retuns only integers
    ;
    else
        return this.data.enqueue(value) // Retuns only integers
    ;
}


//==================================================================
//  Wrappers around Primitive AMOs
//  Translate EMS maps and multi-dimensional array indexes/keys
//  into EMS linear addresses
//  Apparently it is illegal to pass a native function as an argument
function EMSwrite(indexes, value) {
    const linearIndex = EMSidx(indexes, this);
    if (typeof value === "object" && !Buffer.isBuffer(value))
        this.data.write(linearIndex, JSON.stringify(value), true)
    ;
    else
        this.data.write(linearIndex, value, false)
    ;
}

function EMSwriteEF(indexes, value) {
    const linearIndex = EMSidx(indexes, this);
    if (typeof value === "object" && !Buffer.isBuffer(value))
        this.data.writeEF(linearIndex, JSON.stringify(value), true)
    ;
    else
        this.data.writeEF(linearIndex, value, false)
    ;
}

function EMSwriteXF(indexes, value) {
    const linearIndex = EMSidx(indexes, this);
    if (typeof value === "object" && !Buffer.isBuffer(value))
        this.data.writeXF(linearIndex, JSON.stringify(value), true)
    ;
    else
        this.data.writeXF(linearIndex, value, false)
    ;
}

function EMSwriteXE(indexes, value) {
    const nativeIndex = EMSidx(indexes, this);
    if (typeof value === "object" && !Buffer.isBuffer(value))
        this.data.writeXE(nativeIndex, JSON.stringify(value), true)
    ;
    else
        this.data.writeXE(nativeIndex, value, false)
    ;
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
    }
    return this.data.faa(EMSidx(indexes, this), val);  // No returnData(), FAA can only return JSON primitives
}

function EMScas(indexes, oldVal, newVal) {
    if (typeof newVal === "object") {
        console.log("EMScas: ERROR -- objects are not a valid new type");
        return undefined;
    }
    return this.data.cas(EMSidx(indexes, this), oldVal, newVal);
}


//==================================================================
//  Serialize execution through this function
function EMScritical(func, timeout) {
    isNaN(timeout) && (timeout = 500000); // TODO: Magic number -- long enough for errors, not load imbalance
    this.criticalEnter(timeout);
    const retObj = func();
    this.criticalExit();
    return retObj
}


//==================================================================
//  Perform func only on thread 0
function EMSmaster(func) {
    return (this.myID === 0) ? func() : undefined;
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
    const retObj = (this.singleTask()) ? func() : undefined;
    EMSbarrier();
    return retObj
}


//==================================================================
//  Wrapper around the EMS global barrier
function EMSbarrier(timeout) {
    return (EMSglobal.inParallelContext === false)
        ? timeout
        : _EMSbarrier_inParallelContext(timeout)
    ;
}

function _EMSbarrier_inParallelContext(timeout) {
    isNaN(timeout) && (timeout = 500000); // TODO: Magic number -- long enough for errors, not load imbalance
    const remaining_time = EMS.barrier(timeout);
    if (remaining_time < 0) // TODO: Probably should throw an error
        console.log("EMSbarrier: ERROR -- Barrier timed out after", timeout, "iterations.")
    ;
    return remaining_time;
}


//==================================================================
//  Release all resources associated with an EMS memory region
function EMSdestroy(unlink_file) {
    EMSbarrier();
    if (EMSglobal.myID == 0)
        this.data.destroy(unlink_file)
    ;
    EMSbarrier(); // TODO: if not needed if EMSglobal.myID != 0 ?
}


//==================================================================
//  Creating a new EMS memory region
function EMSnew(
    arg0,        //  Maximum number of elements the EMS region can hold
    heapSize,    //  #bytes of memory reserved for strings/arrays/objs/maps/etc
    filename     //  Optional filename for persistent EMS memory
) {

    const emsDescriptor = _init_descriptor.call(this, arg0, heapSize, filename)

    //  init() is first called from thread 0 to perform one-thread
    //  only operations (ie: unlinking an old file, opening a new
    //  file).  After thread 0 has completed initialization, other
    //  threads can safely share the EMS array.
    if (!emsDescriptor.useExisting && this.myID !== 0)
        EMSbarrier()
    ;
    emsDescriptor.data = this.init(emsDescriptor.nElements, emsDescriptor.heapSize,  // 0, 1
        emsDescriptor.useMap, emsDescriptor.filename,  // 2, 3
        emsDescriptor.persist, emsDescriptor.useExisting,  // 4, 5
        emsDescriptor.doDataFill, emsDescriptor.fillIsJSON, // 6, 7
        emsDescriptor.dataFill,  // 8
        emsDescriptor.doSetFEtags,  // 9
        emsDescriptor.setFEtagsFull,  // 10
        this.myID, this.pinThreads, this.nThreads,  // 11, 12, 13
        emsDescriptor.mlock)  // 14
    ;
    if (!emsDescriptor.useExisting && this.myID === 0)
        EMSbarrier()
    ;
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
        const EMSproxyHandler = {
            get(target, name) {
                return (name in target) ? target[name] : target.read(name);
            },
            set(target, name, value) {
                target.write(name, value);
                return true
            }
        };
        return new Proxy(emsDescriptor, EMSproxyHandler);
    }

    return emsDescriptor;
}


function _init_descriptor(arg0, heapSize, filename) {
    if (isNaN(heapSize))
        heapSize = 0
    ;
    const desc = {   // Internal EMS descriptor
        dimensions: undefined,
        filename,
        nElements: 1,           // Required: Maximum number of elements in array
        heapSize,               // Optional, default=0: Space, in bytes, for strings, maps, objects, etc.
        mlock: 0,               // Optional, 0-100% of EMS memory into RAM
        useMap: false,          // Optional, default=false: Use a map from keys to indexes
        useExisting: false,     // Optional, default=false: Preserve data if a file already exists
        ES6proxies: false,      // Optional, default=false: Inferred EMS read/write syntax
        persist: true,          // Optional, default=true: Preserve the file after threads exit
        doDataFill: false,      // Optional, default=false: Data values should be initialized
        dataFill: undefined,    //Optional, default=false: Value to initialize data to
        doSetFEtags: false,     // Optional, initialize full/empty tags
        setFEtagsFull: true,    // Optional, used only if doSetFEtags is true
        dimStride: [],          // Stride factors for each dimension of multidimensional arrays
        fillIsJSON: false
    };

    switch (arg0 instanceof Array ? 'array' : typeof arg0) {
        case 'undefined':
            desc.dimensions = [1]
            break
        case 'array':
            desc.dimensions = arg0
            break
        case 'number':
            desc.dimensions = [arg0]
            break
        case 'object':
            const arg = (key) => (arg0[key] !== undefined) ? (desc[key] = arg0[key]) : desc[key]
            arg('filename')
            arg('persist')
            arg('ES6proxies')
            arg('mlock')
            arg('doSetFEtags')
            arg('hashFunc')
            arg('useExisting')
            arg('heapSize')
            if (arg('useMap') && desc.heapSize <= 0) {
                console.log("Warning: New EMS array with no heap, disabling mapped keys");
                emsDescriptor.useMap = false;
            }
            if (!(arg('dimensions') instanceof Array)) {
                desc.dimensions = [ arg0.dimensions ]
            }
            if (arg('doDataFill')) {
                arg('dataFill');
                if (typeof desc.dataFill === "object" && !Buffer.isBuffer(desc.dataFill)) {
                    desc.dataFill = JSON.stringify(desc.dataFill);
                    desc.fillIsJSON = true;
                }
            }
            if (arg0.setFEtags !== undefined) {
                desc.setFEtagsFull = (arg0.setFEtags === "full");
            }
            break
        default:
            console.log("EMSnew: Couldn't determine type of arg0", arg0, typeof arg0)
    }

    desc.filename || (
        desc.filename = "/EMS_region_" + this.newRegionN, // Name the region if a name wasn't given
        desc.persist = false
    )

    if (desc.useExisting) {
        try {
            fs.openSync(desc.filename, "r");
        }
        catch (err) {
            return;
        }
    }

    // Compute the stride factors for each dimension of a multidimensional array
    if (desc.dimensions !== undefined) {
        const dimLen = desc.dimensions.length
        let dimN = 0
        while (dimN < dimLen) {
            desc.dimStride.push(desc.nElements);
            desc.nElements *= desc.dimensions[dimN];
            dimN = dimN + 1
        }
    }
    else {
        desc.dimensions = [desc.nElements];
    }
    return desc
}


// //==================================================================
// //  EMS object initialization, invoked by the require statement
// //
function ems_wrapper(nThreads, pinThreads, threadingType, filename) {

    // TODO: Determining the thread ID should be done via shared memory
    const myID = (process.env.EMS_Subtask === undefined) ? 0 : parseInt(process.env.EMS_Subtask)
    const tasks = []
    const domainName = filename ? filename : "/EMS_MainDomain";
    pinThreads = pinThreads === true
    nThreads = parseInt(nThreads)
    if (nThreads <= 0) {
        if (process.env.EMS_Ntasks !== undefined) {
            nThreads = parseInt(process.env.EMS_Ntasks);
        } else {
            console.log("EMS: Must declare number of nodes to use.  Input:" + nThreads);
            process.exit(1);
        }
    }

    const data =  EMS.initialize( //All arguments are defined -- now do the EMS initialization
        0, 0,                       // 0= # elements, 1=Heap Size
        false,                      // 2 = useMap
        domainName, false, false,   //  3=name, 4=persist, 5=useExisting
        false, false, undefined,    //  6=doDataFill, 7=fillIsJSON, 8=fillValue
        false, false,  myID,        //  9=doSetFEtags, 10=setFEtags, 11=EMS myID
        pinThreads, nThreads, 99    // 12=pinThread,  13=nThreads, 14=pctMlock
    )

    var inParallelContext
    var targetScript;
    switch (threadingType) {
        case undefined:
        case "bsp":
            targetScript = process.argv[1];
            threadingType = "bsp";
            inParallelContext = true;
            break;
        case "fj":
            targetScript = "./EMSthreadStub";
            inParallelContext = false;
            break;
        case "user":
            targetScript = undefined;
            inParallelContext = false;
            break;
        default:
            console.log("EMS: Unknown threading model type:", threadingType);
            inParallelContext = false;
            break;
    }

    //  The master thread has completed initialization, other threads may now
    //  safely execute.
    if (targetScript !== undefined && myID === 0) {
        const emsThreadStub =
            "// Automatically Generated EMS Slave Thread Script\n" +
            "// To edit this file, see ems.js:emsThreadStub()\n" +
            "var ems = require('"+EMS_MODULE_FILE+"')(parseInt(process.env.EMS_Ntasks));\n" +
            "process.on(\"message\", function(msg) {\n" +
            "    eval(\"func = \" + msg.func);\n" +
            "    func.apply(null, msg.args);\n" +
            "} );\n";
        fs.writeFileSync('./EMSthreadStub.js', emsThreadStub, {flag: 'w+'});
        process.env.EMS_Ntasks = nThreads;
        for (let taskN = 1; taskN < nThreads; taskN++) {
            process.env.EMS_Subtask = taskN;
            tasks.push(
                child_process.fork(targetScript,
                    process.argv.slice(2, process.argv.length)));
        }
    }

    return EMSglobal = {
        tasks,
        inParallelContext,
        myID,
        data,
        nThreads,
        threadingType,
        pinThreads,
        domainName,
        newRegionN: 0,
        init: EMS.initialize,
        new: EMSnew,
        critical: EMScritical,
        criticalEnter: EMS.criticalEnter,
        criticalExit: EMS.criticalExit,
        master: EMSmaster,
        single: EMSsingle,
        diag: EMSdiag,
        parallel: EMSparallel,
        barrier: EMSbarrier,
        parForEach: EMSparForEach,
        tmStart: EMStmStart,
        tmEnd: EMStmEnd,
        loopInit: EMS.loopInit,
        loopChunk: EMS.loopChunk,
    };
}

module.exports =
ems_wrapper.initialize = ems_wrapper;