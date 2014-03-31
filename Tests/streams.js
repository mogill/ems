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
var ems = require('ems')(parseInt(process.argv[2]), false)
var arrLen = 1000000
var heapSize = 0
var a = ems.new(arrLen)
var b = ems.new(arrLen)
var c = ems.new(arrLen)
var x = Array(arrLen)
var y = Array(arrLen)
var z = Array(arrLen)


//-------------------------------------------------------------------
//  Timer function
function stopTimer(timer, nOps, label) {
    function fmtNumber(n) {
	var s = '                       ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
	if(n < 1) return n
	else    { return s.substr(s.length - 15, s.length)  }
    }
    ems.master( function() {
	var now = new Date().getTime()
        var x = (nOps*1000000) / ((now - timer) *1000)
        ems.diag(fmtNumber(nOps) + label + fmtNumber(Math.floor(x).toString()) + " ops/sec")
    } )
}


var timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.write(idx, idx) } )
stopTimer(timeStart, arrLen, " write   ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.writeXE(idx, idx) } )
stopTimer(timeStart, arrLen, " writeXE ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.writeXF(idx, idx) } )
stopTimer(timeStart, arrLen, " writeXF ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.read(idx, idx) } )
stopTimer(timeStart, arrLen, " read    ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.read(idx, idx) } )
stopTimer(timeStart, arrLen, " reread  ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.readFF(idx, idx) } )
stopTimer(timeStart, arrLen, " readFF  ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.readFE(idx, idx) } )
stopTimer(timeStart, arrLen, " readFE  ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { a.writeEF(idx, idx) } )
stopTimer(timeStart, arrLen, " writeEF ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { b.writeXF(idx, a.readFF(idx)) } )
stopTimer(timeStart, arrLen, " copy    ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { b.writeXF(idx, a.readFF(idx)) } )
stopTimer(timeStart, arrLen, " recopy  ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { c.writeXF(idx, a.readFF(idx) * b.readFF(idx)) } )
stopTimer(timeStart, arrLen, " c=a*b   ")

timeStart = new Date().getTime()
ems.parForEach(0, arrLen, function(idx) { c.writeXF(idx, c.readFF(idx) + (a.readFF(idx) * b.readFF(idx))) } )
stopTimer(timeStart, arrLen, " c+=a*b  ")


//===========================================================================================

if(ems.myID == 0) {
    console.log('------------------------ START NATIVE ARRAY')
    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) {  x[idx] = idx }
    stopTimer(timeStart, arrLen, " write   ")

    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) {  x[idx] += idx }
    stopTimer(timeStart, arrLen, " rd/write")
    
    timeStart = new Date().getTime()
    var dummy = 0
    for(var idx = 0;  idx < arrLen;  idx++ ) { dummy += x[idx] }
    stopTimer(timeStart, arrLen, " read    ")
    
    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) { dummy += x[idx] }
    stopTimer(timeStart, arrLen, " reread  ")
    
    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) { y[idx] = x[idx] }
    stopTimer(timeStart, arrLen, " copy    ")
    
    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) { y[idx] += x[idx] }
    stopTimer(timeStart, arrLen, " rmwcopy ")
    
    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) { z[idx] = x[idx] * y[idx] }
    stopTimer(timeStart, arrLen, " c=a*b   ")

    timeStart = new Date().getTime()
    for(var idx = 0;  idx < arrLen;  idx++ ) { z[idx] += x[idx] * y[idx] }
    stopTimer(timeStart, arrLen, " c+=a*b  ")
}

