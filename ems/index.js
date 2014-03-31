/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.7   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
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
var fs = require("fs")
var child_process = require('child_process')
var EMS = require("./build/Release/ems") 

//==================================================================
//  Generate a number hash ID for a string
//
function EMShashString( string  //  String to hash to a number
		      ) {
    var hash = 0
    for(var idx = 0;  idx < string.length;  idx++) {
       hash = string.charCodeAt(idx) + (hash << 6) + (hash << 16) - hash;
    }
    return hash
}


//==================================================================
//  Convert the different possible index types into a linear EMS index
//
function EMSidx(indexes,    // Index given by application
		EMSarray    // EMS array being indexed
	       ) {
    var idx = 0
    if( indexes instanceof Array) {       //  Is a Multidimension array: [x,y,z]
	indexes.forEach( function(x, i) {
	    idx += x * EMSarray.dimStride[i]
	} )
    } else { 
	if( !(typeof indexes === 'number')  &&  !EMSarray.useMap  ) {  //  If no map, only use integers
	    console.log("EMS ERROR: Non-integer index used, but EMS memory was not cofigured to use a map (useMap)",
			indexes, typeof indexes,  EMSarray.useMap)
	    idx = -1
	} else {   //  Is a mappable intrinsic type
	    idx = indexes
	}
    }
    return idx
}





//==================================================================
//  Print a message to the console with a prefix indicating which
//  thread is printing
//
function EMSdiag(text) {
    console.log("EMStask " + this.myID + ": " + text)
}




//==================================================================
//  Co-Begin a parallel region, executing the function 'func'
//
function EMSparallel( func )  {
    this.tasks.forEach( function(task, taskN) {
	task.send( { 'args' : taskN+1,  'func' : func.toString() } )
    } )
    func(0)
}





//==================================================================
//  Execute the local iterations of a decomposed loop with
//  the specified scheduling.
//
function EMSparForEach( start,         // First iteration's index
			end,           // Final iteration's index
			loopBody,      // Function to execute each iteration
			scheduleType,  // Load balancing technique
			minChunk      // Smallest block of iterations
		      ) {
    var sched = scheduleType
    if(typeof sched    === 'undefined') { sched = 'guided' }
    if(typeof minChunk === 'undefined') { minChunk = 1 }
    switch(scheduleType) {
    case 'static':     // Evenly divide the iterations across threads
	var range = end - start
	var blocksz = Math.floor(range / EMSglobal.nThreads)+1
	var s = blocksz * EMSglobal.myID
	var e = blocksz * (EMSglobal.myID+1)
	if(e > end) { e = end }
	for(var idx = s;  idx < e;  idx++) {
	    loopBody(idx) 
	}
	break
    case 'dynamic': 
    case 'guided': 
    default:
	//  Initialize loop bounds, block size, etc.
	EMSglobal.data.loopInit(start, end, sched, minChunk)
	//  Do not enter loop until all threads have completed initialization
	//  If the barrier is present at the loop end, this may replaced
	//  with first-thread initialization.
	EMSglobal.data.barrier()  
	var extents = EMSglobal.data.loopChunk()
	while(extents.end - extents.start > 0) {
	    for(var idx = extents.start;  idx < extents.end;  idx++) {
		loopBody(idx) 
	    }
	    extents = EMSglobal.data.loopChunk()
	}
    }

    // Do not proceed until all iterations have completed
    // TODO: OpenMP offers NOWAIT to skip this barrier
    EMSglobal.data.barrier()
}




//==================================================================
//  Start a Transaction
//  Input is an array containing EMS elements to transition from
//  Full to Empty:
//     arr = [ [ emsArr0, idx0 ], [ emsArr1, idx1, true ], [ emsArr2, idx2 ] ]
//
function EMStmStart( emsElems ) {
    var MAX_ELEM_PER_REGION = 10000000000000

    // Build a list of indexes of the elements to lock which will hold
    // the elements sorted for deadlock free acquisition
    var indicies = []
    for (var i = 0; i < emsElems.length; ++i) indicies[i] = i;

    //  Sort the elements to lock according to a global ordering
    var sorted = indicies.sort( function(a, b) {
	return( ((emsElems[a][0].regionN * MAX_ELEM_PER_REGION)+emsElems[a][1]) -
		((emsElems[b][0].regionN * MAX_ELEM_PER_REGION)+emsElems[b][1]) )
    } )

    //  Acquire the locks in the deadlock free order, saving the contents
    //  of the memory when it locked.
    //  Mark read-write data as Empty, and read-only data under a readers-writer lock
    var tmHandle = []
    sorted.forEach( function(e) {
	var val
	if(emsElems[e][2] === true) {
	    val = emsElems[e][0].readRW( emsElems[e][1])
	} else {
	    val = emsElems[e][0].readFE( emsElems[e][1]) 
	}
	tmHandle.push( [ emsElems[e][0], emsElems[e][1], emsElems[e][2], val ] )
    } )
    return tmHandle
}

    

//==================================================================
//  Commit or abort a transaction
//  The tmHandle contains the result from tmStart:
//    [ [ EMSarray, index, isReadOnly, origValue ], ... ]
//
function EMStmEnd( tmHandle,  //  The returned value from tmStart
		   doCommit   //  Commit or Abort the transaction
		 ) {
    tmHandle.forEach( function(emsElem) {
	if(doCommit) {
	    if(emsElem[2] === true) {   
		// Is a read-only element
		emsElem[0].releaseRW(emsElem[1]) 
	    } else {  
		// Is read-write, keep current value and mark Full
		emsElem[0].setTag(emsElem[1], 'full')
	    }
	} else {
	    // Abort the transaction
	    if(emsElem[2] === true) {  
		// Is read-only element
		emsElem[0].releaseRW(emsElem[1]) 
	    } else { 
		// Write back the original value and mark full
		emsElem[0].writeEF(emsElem[1], emsElem[3]) 
	    }
	}
    } )
}



//==================================================================
//  Synchronize memory with storage
//
function EMSsync(emsArr) { return this.data.sync(emsArr) }


//==================================================================
//  Wrappers around Stacks and Queues
function EMSpush(value)    { return this.data.push(value) }
function EMSpop()          { return this.data.pop() }
function EMSenqueue(value) { return this.data.enqueue(value) }
function EMSdequeue()      { return this.data.dequeue() }


//==================================================================
//  Wrappers around Primitive AMOs
//  Translate EMS maps and multi-dimensional array indexes/keys 
//  into EMS linear addresses 
function EMSwrite(indexes, val)   { this.data.write(EMSidx(indexes, this), val) }
function EMSwriteEF(indexes, val) { this.data.writeEF(EMSidx(indexes, this), val) }
function EMSwriteXF(indexes, val) { this.data.writeXF(EMSidx(indexes, this), val) }
function EMSwriteXE(indexes, val) { this.data.writeXE(EMSidx(indexes, this), val) }
function EMSread(indexes)         { return this.data.read(EMSidx(indexes, this)) }
function EMSreadFE(indexes)       { return this.data.readFE(EMSidx(indexes, this)) }
function EMSreadFF(indexes)       { return this.data.readFF(EMSidx(indexes, this)) }
function EMSfaa(indexes, val)     { return this.data.faa(EMSidx(indexes, this), val) }
function EMSreadRW(indexes)       { return this.data.readRW(EMSidx(indexes, this)) }
function EMSreleaseRW(indexes)    { return this.data.releaseRW(EMSidx(indexes, this)) }
function EMSsetTag(indexes, fe)   { return this.data.setTag(EMSidx(indexes, this), 
					    (fe == 'full' ? true : false) ) }
function EMScas(indexes, oldVal, newVal) {
    return this.data.cas(EMSidx(indexes, this), oldVal, newVal)
}


//==================================================================
//  Serialize execution through this function
function EMScritical( func ) {
    this.data.criticalEnter()
    var retObj = func()
    this.data.criticalExit()
    return retObj
}


//==================================================================
//  Perform func only on thread 0
function EMSmaster( func ) {
    if( this.myID == 0 ) { return func() }
}


//==================================================================
//  Perform the function func once by the first thread to reach 
//  the function.  The final barrier is required because  a
//  thread may try to execute the next single-execution region
//  before other threads have finished this region, which the EMS
//  runtime cannot tell apart.  Barriers are phased, so a barrier
//  is used to prevent any thread from entering the next single-
//  execution region before this one is complete
//
function EMSsingle( func ) {
    var retObj
    if( this.data.singleTask() ) { 
	retObj = func()
    }
    this.barrier()
    return retObj
}


//==================================================================
//  Wrapper around the EMS global barrier
function EMSbarrier() { this.data.barrier() }



//==================================================================
//  Utility functions for determining types
//
function EMSisArray(a)   { return( (typeof a.pop !== 'undefined') ) }
function EMSisObject(o)  { return( (typeof o === 'object'  &&  !EMSisArray(o)) ) }
function EMSisDefined(x) { return( (typeof x  !== 'undefined') ) }


//==================================================================
//  Creating a new EMS memory region
//
function EMSnew(arg0,        //  Maximum number of elements the EMS region can hold
		heapSize,    //  #bytes of memory reserved for strings/arrays/objs/maps/etc
		filename     //  Optional filename for persistent EMS memory
	       ) {
    var retObj = { }         //  Object returned as the EMS region handle
    var emsDescriptor = {    //  Internal EMS descriptor
	nElements   : 1,     // Required: Maximum number of elements in array
	heapSize    : 0,     // Optional, default=0: Space, in bytes, for strings, maps, objects, etc.
	useMap      : false, // Optional, default=false: Use a map from keys to indexes
	useExisting : false, // Optional, default=false: Preserve data if a file already exists
	persist     : true,  // Optional, default=true: Preserve the file after threads exit
	setFEtags   : 'empty',
//	dataFill    : undefined,
	dimStride   : []     //  Stride factors for each dimension of multidimensal arrays
    }

    if( !EMSisDefined(arg0) ) {  // Nothing passed in, assume length 1
	emsDescriptor.dimensions = [ 1 ]
    } else {                  
	if(EMSisObject(arg0)) {  // User passed in emsArrayDescriptor
	    if(typeof arg0.dimensions  !== 'undefined') { emsDescriptor.dimensions  = arg0.dimensions }
	    if(typeof arg0.heapSize    !== 'undefined') { emsDescriptor.heapSize    = arg0.heapSize }
	    if(typeof arg0.useMap      !== 'undefined') { emsDescriptor.useMap      = arg0.useMap }
	    if(typeof arg0.filename    !== 'undefined') { emsDescriptor.filename    = arg0.filename }
	    if(typeof arg0.persist     !== 'undefined') { emsDescriptor.persist     = arg0.persist }
	    if(typeof arg0.useExisting !== 'undefined') { emsDescriptor.useExisting = arg0.useExisting }
	    if(typeof arg0.dataFill    !== 'undefined') { emsDescriptor.dataFill    = arg0.dataFill }
	    if(typeof arg0.setFEtags   !== 'undefined') { emsDescriptor.setFEtags   = arg0.setFEtags }
	    if(typeof arg0.hashFunc    !== 'undefined') { emsDescriptor.hashFunc    = arg0.hashFunc }
	} else {
	    if(EMSisArray(arg0)) { // User passed in multi-dimensional array
		emsDescriptor.dimensions  = arg0
	    } else {
		if(typeof arg0 === 'number') { // User passed in scalar 1-D array length
		    emsDescriptor.dimensions  = [ arg0 ]
		} else {
		    console.log("EMSnew: Couldn't determine type of arg0", arg0, typeof arg0)
		} } 
	}
	if(typeof heapSize === 'number') { emsDescriptor.heapSize   = heapSize }
	if(typeof filename === 'string') { emsDescriptor.filename   = filename }
    }

    // Compute the stride factors for each dimension of a multidimensal array
    for( var dimN = 0;  dimN < emsDescriptor.dimensions.length;  dimN++ ) {
	emsDescriptor.dimStride.push(emsDescriptor.nElements)
	emsDescriptor.nElements *= emsDescriptor.dimensions[dimN]
    }
    if(typeof emsDescriptor.dimensions === 'undefined') {
	emsDescriptor.dimensions = [ emsDescriptor.nElements ]
	console.log('dimensions', emsDescriptor.dimensions)
    }

    // Name the region if a name wasn't given
    if( !EMSisDefined(emsDescriptor.filename) ) {
	emsDescriptor.filename = '/EMS_region_' + this.newRegionN
        emsDescriptor.persist = false
    }

    //  init() is first called from thread 0 to perform one-thread
    //  only operations (ie: unlinking an old file, opening a new
    //  file).  After thread 0 has completed initialization, other
    //  threads can safely share the EMS array.
    if(this.myID == 0) {
	emsDescriptor.data   = this.init(emsDescriptor.nElements,         emsDescriptor.heapSize,
					 emsDescriptor.useMap,            emsDescriptor.filename,
					 emsDescriptor.persist,           emsDescriptor.useExisting,
					 (emsDescriptor.dataFill === 'undefined') ? false : true,
					 emsDescriptor.dataFill,
					 (emsDescriptor.setFEtags === 'undefined') ? false : true,
                                         (emsDescriptor.setFEtags == 'full') ? true : false,
					 this.myID, this.pinThreads, this.nThreads)
	this.barrier()
    } else {
	this.barrier()
	emsDescriptor.data   = this.init(emsDescriptor.nElements,         emsDescriptor.heapSize,
					 emsDescriptor.useMap,            emsDescriptor.filename,
					 emsDescriptor.persist,           emsDescriptor.useExisting,
					 (emsDescriptor.dataFill === 'undefined') ? false : true,
					 emsDescriptor.dataFill,
					 (emsDescriptor.setFEtags === 'undefined') ? false : true,
                                         (emsDescriptor.setFEtags == 'full') ? true : false,
					 this.myID,
                                         this.pinThreads, this.nThreads)
    }

    emsDescriptor.regionN   = this.newRegionN
    emsDescriptor.push      = EMSpush
    emsDescriptor.pop       = EMSpop
    emsDescriptor.enqueue   = EMSenqueue
    emsDescriptor.dequeue   = EMSdequeue
    emsDescriptor.setTag    = EMSsetTag
    emsDescriptor.write     = EMSwrite
    emsDescriptor.writeEF   = EMSwriteEF
    emsDescriptor.writeXF   = EMSwriteXF
    emsDescriptor.writeXE   = EMSwriteXE
    emsDescriptor.read      = EMSread
    emsDescriptor.readRW    = EMSreadRW
    emsDescriptor.releaseRW = EMSreleaseRW
    emsDescriptor.readFE    = EMSreadFE
    emsDescriptor.readFF    = EMSreadFF
    emsDescriptor.faa       = EMSfaa
    emsDescriptor.cas       = EMScas
    emsDescriptor.sync      = EMSsync
    this.newRegionN++
    this.barrier()
    return emsDescriptor
}



//==================================================================
//  EMS object initialization, invoked by the require statement
//
function ems_wrapper(nThreadsArg, pinThreadsArg, useSlaveTasks) {
    var retObj = { tasks : [] }

    // TODO: Determining the thread ID should be done via shared memory
    if(process.argv[process.argv.length-2] === 'EMS_Subtask') {
	retObj.myID = parseInt(process.argv[process.argv.length-1])
    } else {
	retObj.myID = 0
    }

    var pinThreads = true
    if(typeof pinThreadsArg === 'boolean') { pinThreads = pinThreadsArg }

    var nThreads
    nThreads = parseInt(nThreadsArg)
    if( !(nThreads > 0) ) {
	console.log("EMS: Must declare number of nodes to use.  Input:"+nThreadsArg)
	process.exit(1)
    }

    //  All arguments are defined -- now do the EMS initialization
    var domainName = '/EMS_MainDomain';
    retObj.data = EMS.initialize(0, 0, false, domainName, false, false,
				 false, 0, false, 0, retObj.myID, pinThreads, nThreads) 

    var targetScript = process.argv[1];
    if(useSlaveTasks) { targetScript = './EMSthreadStub' }

    //  The master thread has completed initialization, other threads may now
    //  safely join and they are forked presently
    if(retObj.myID == 0) {
	var emsThreadStub = '// Automatically Generated EMS Slave Thread Script\n// Edit index.js: emsThreadStub\n ems = require(\'ems\')(parseInt(process.argv[2]));   process.on(\'message\', function(msg) { eval(\'msg.func = \' + msg.func); msg.func(msg.args); } );'
	fs.writeFileSync('./EMSthreadStub.js', emsThreadStub, {flag:'w+'})
	for( var taskN = 1;  taskN < nThreads;  taskN++) {
	    retObj.tasks.push(
                child_process.fork(targetScript,
				   process.argv.slice(2, process.argv.length).concat('EMS_Subtask', taskN)) )
	}
    }

    retObj.nThreads   = nThreads
    retObj.pinThreads = pinThreads
    retObj.domainName = domainName
    retObj.newRegionN = 0
    retObj.init       = EMS.initialize
    retObj.new        = EMSnew
    retObj.critical   = EMScritical
    retObj.master     = EMSmaster
    retObj.single     = EMSsingle
    retObj.diag       = EMSdiag
    retObj.parallel   = EMSparallel
    retObj.barrier    = EMSbarrier
    retObj.parForEach = EMSparForEach
    retObj.tmStart    = EMStmStart
    retObj.tmEnd      = EMStmEnd
    EMSglobal = retObj
    return retObj
}


for(var k in EMS) ems_wrapper[k] = EMS[k];
ems_wrapper.initialize = ems_wrapper;

module.exports = ems_wrapper;
