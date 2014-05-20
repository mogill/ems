/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.8   |
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
var ems = require('ems')(parseInt(process.argv[2]))
var arrLen = 1000000
var a = ems.new( {
    dimensions : [ arrLen ],
    heapSize  : 10000,
    useExisting : false,
    filename  : '/tmp/EMS_stack',
    setFEtags : 'empty',
    dataFill  : undefined
} )

var b = ems.new( {
    dimensions : [ arrLen ],
    heapSize  : 200000000,
    setFEtags : 'empty',
    dataFill  : undefined
} )


//-------------------------------------------------------------------
//  Timer function
function stopTimer(timer, nOps, label) {
    function fmtNumber(n) {
	var s = '                                ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
	if(n < 1) return n
	else    { return s.substr(s.length - 15, s.length)  }
    }
    ems.master( function() {
	var now = new Date().getTime()
        var x = (nOps*1000000) / ((now - timer) *1000)
        ems.diag(fmtNumber(nOps) + label + fmtNumber(Math.floor(x).toString()) + " ops/sec")
    } )
}


var x = a.pop()
if(x  !== undefined) { ems.diag( "Initial: should have been empty: " + x) }
ems.barrier()

a.push(1000 + ems.myID)
ems.barrier()
x = a.pop()
if( x === undefined ) { ems.diag("Expected something on the stack") }
ems.barrier()
x = a.pop()
if( x !== undefined ) { ems.diag("Expected nothing on the stack, found: "+x) }

ems.barrier()
a.push(2000.12345 + ems.myID)
a.push('text ' + ems.myID + 'yeah...')
ems.barrier()
a.pop(); a.pop()
ems.barrier()
if(a.pop()  !==  undefined) { ems.diag( "should have been empty") }
ems.barrier()
a.push(3330.12345 + ems.myID)
a.push('more text ' + ems.myID + 'yeah...')
a.pop(); a.pop()
ems.barrier()
if(a.pop()  !==  undefined) { ems.diag( "no barrier version should have been empty") }

b.enqueue(4000+ ems.myID)

ems.barrier()
ems.master( function() {
    var tmp = b.dequeue()
    var count = 0
    while(tmp !== undefined) {
	tmp = b.dequeue()
	count++
    }
    if(count != ems.nThreads) { ems.diag( "Didn't find enough queued items") }
    if(a.pop()  !== undefined) { ems.diag( "dq1: should have been empty") }
} )
ems.barrier()
var tmp = b.dequeue()
if(tmp !== undefined) { ems.diag( "DQ should be mt: " + tmp) }
ems.barrier()
ems.master( function() {
    for(var i = 0;  i < arrLen;  i++) {
	b.enqueue("sequential" + (i * 100))
    }
    for(var i = 0;  i < arrLen;  i++) {
	var tmp = b.dequeue()
	if(tmp === undefined) { ems.diag( "Should have found a value") }
    }
    if(a.pop()  !== undefined) { ems.diag( "dq1 seq: should have been empty") }

} )
ems.barrier()

var timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    b.enqueue( 'foo'+idx)
})
stopTimer(timeStart, arrLen, " enqueued ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    b.dequeue()
} )
stopTimer(timeStart, arrLen, " dequeued ")


var tmp = b.dequeue()
if(tmp !== undefined) { ems.diag( "DQend should be mt: " + tmp) }


var p = ems.new({
    dimensions : [ arrLen+1 ],
    heapSize  : 200000000,
    setFEtags : 'empty',
    dataFill  : undefined
})
var timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    p.push( 'foo'+idx)
})
stopTimer(timeStart, arrLen, " pushed ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    p.pop()
} )
stopTimer(timeStart, arrLen, " popped ")



ems.barrier()
c = []
timeStart = new Date().getTime()
ems.master( function() {
    for(var idx = 0;  idx < arrLen;  idx++ ) {
	c.push('foo'+ idx)
    }
} )
stopTimer(timeStart, arrLen, " native enqueued ")

timeStart = new Date().getTime()
ems.master( function() {
    var sum = 0
    for(var idx = 0;  idx < arrLen;  idx++ ) {
	sum += c.pop()
    }
    if(sum == -1) console.log('dummy')
} )
stopTimer(timeStart, arrLen, " native dequeued ")

var tmp = b.dequeue()
if(tmp !== undefined) { ems.diag( "DQend should be mt: " + tmp) }

/////////////////////////////////////////

var timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    b.enqueue( idx)
})
stopTimer(timeStart, arrLen, " int enqueued ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    b.dequeue()
} )
stopTimer(timeStart, arrLen, " int dequeued ")


var tmp = b.dequeue()
if(tmp !== undefined) { ems.diag( "DQend should be mt: " + tmp) }


var p = ems.new({
    dimensions : [ arrLen+1 ],
    heapSize  : 200000000,
    setFEtags : 'empty',
    dataFill  : undefined
})
var timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    p.push(idx)
})
stopTimer(timeStart, arrLen, " int pushed ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) {
    p.pop()
} )
stopTimer(timeStart, arrLen, " int popped ")



ems.barrier()
c = []
timeStart = new Date().getTime()
ems.master( function() {
    for(var idx = 0;  idx < arrLen;  idx++ ) {
	c.push(idx)
    }
} )
stopTimer(timeStart, arrLen, " int native enqueued ")

timeStart = new Date().getTime()
ems.master( function() {
    var sum = 0
    for(var idx = 0;  idx < arrLen;  idx++ ) {
	c.pop()
    }
    if(sum == -1) console.log('dummy')
} )
stopTimer(timeStart, arrLen, " int native dequeued ")

var tmp = b.dequeue()
if(tmp !== undefined) { ems.diag( "DQend should be mt: " + tmp) }

