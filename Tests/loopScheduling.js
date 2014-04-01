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
var util = require('./testUtils')
var assert = require('assert')
var startTime, nOps;
var idx = ems.myID

var stats = ems.new(1)
var sum = 0
var start = 0
var end = 1000000
var nIters = 1000
var n = nIters * (end-start)

function doWork(idx) {
    if(idx == 0) { console.log("index 0") }
    if(idx == end-1) { console.log("index final") }

    if(idx < 0  ||  idx >= end) {
	console.log("Index out of bounds?!")
    }
    for(var i = 0;  i < nIters; i++) {
	sum++
    } 
}

stats.writeXF(0, 0)
ems.barrier()
startTime = util.timerStart()
ems.parForEach( start, end, doWork )
stats.faa(0, sum)
util.timerStop(startTime, n* ems.nThreads, " Default      ", ems.myID)

ems.barrier()
sum = stats.read(0)
assert(sum == n, "Default scheduling failed sum="+sum+"  expected "+n)
                                    

sum = 0
startTime = util.timerStart()
ems.parForEach( start, end, doWork, 'dynamic')
util.timerStop(startTime, n* ems.nThreads, " Dynamic      ", ems.myID)
ems.barrier()
sum = stats.read(0)
assert(sum == n, "Dyamic scheduling failed sum="+sum+"  expected "+n)


sum = 0
startTime = util.timerStart()
ems.parForEach( start, end, doWork, 'static')
util.timerStop(startTime, n* ems.nThreads, " Static       ", ems.myID)
ems.barrier()
sum = stats.read(0)
assert(sum == n, "Static scheduling failed sum="+sum+"  expected "+n)


sum = 0
startTime = util.timerStart()
ems.parForEach( start, end, doWork, 'guided', 20)
util.timerStop(startTime, n* ems.nThreads, " Guided by 20 ", ems.myID)
ems.barrier()
sum = stats.read(0)
assert(sum == n, "Guided scheduling failed sum="+sum+"  expected "+n)


