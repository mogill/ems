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
//var ems = require('ems')(parseInt(process.argv[2]), true, true)
var ems = require('../Addon/index.js')(parseInt(process.argv[2]), true, true)

ems.parallel( function() { 
    //-------------------------------------------------------------------
    //  Timer functions
    function timerStart(){ return new Date().getTime() }
    function timerStop(timer, nOps, label, myID) {
	function fmtNumber(n) {
	    var s = '                       ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
	    if(n < 1) return n
	    else    { return s.substr(s.length - 15, s.length)  }
	}
	var now = new Date().getTime()
	var opsPerSec = (nOps*1000000) / ((now - timer) *1000)
	if(typeof myID === undefined  ||  myID === 0) {
            console.log(fmtNumber(nOps) + label + fmtNumber(Math.floor(opsPerSec).toString()) + " ops/sec")
	}
    }
    
    totalTime = timerStart()
    arrLen = 1000000
    a = ems.new(arrLen)
    b = ems.new(arrLen)
    c = ems.new(arrLen)

    var startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.write(idx, idx) } )
    timerStop(startTime, arrLen, " write   ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.writeXE(idx, idx) } )
    timerStop(startTime, arrLen, " writeXE ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.writeXF(idx, idx) } )
    timerStop(startTime, arrLen, " writeXF ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.read(idx, idx) } )
    timerStop(startTime, arrLen, " read    ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.read(idx, idx) } )
    timerStop(startTime, arrLen, " reread  ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.readFF(idx, idx) } )
    timerStop(startTime, arrLen, " readFF  ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.readFE(idx, idx) } )
    timerStop(startTime, arrLen, " readFE  ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { a.writeEF(idx, idx) } )
    timerStop(startTime, arrLen, " writeEF ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { b.writeXF(idx, a.readFF(idx)) } )
    timerStop(startTime, arrLen, " copy    ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { b.writeXF(idx, a.readFF(idx)) } )
    timerStop(startTime, arrLen, " recopy  ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { c.writeXF(idx, a.readFF(idx) * b.readFF(idx)) } )
    timerStop(startTime, arrLen, " c=a*b   ", ems.myID)

    startTime = timerStart()
    ems.parForEach(0, arrLen, function(idx) { c.writeXF(idx, c.readFF(idx) + (a.readFF(idx) * b.readFF(idx))) } )
    timerStop(startTime, arrLen, " c+=a*b  ", ems.myID)

} )


process.exit(0)

