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
var ems = require('ems')(process.argv[2])
var singleton = ems.new(1)
var nTimes = 10000
var sum = 0

ems.single( function() {
    singleton.writeXF(0, 0)
    for(var i = 0;  i < nTimes * ems.myID;  i++) {
	sum += Math.sin(i)
    }
} )

for(var i = 0;  i < nTimes;  i++) { ems.barrier() }

for(var i = 0;  i < nTimes;  i++) {
    ems.single( function() {
	singleton.write(0, singleton.read(0) + 1)
    } )
}

ems.barrier()
if(singleton.read(0) != nTimes) {
    ems.diag("Singleton was " + singleton.read(0) + ' should have been ' + nTimes)
}


var arr = ems.new(ems.nThreads)
for(var i = 0;  i < nTimes;  i++) {
    arr.write(ems.myID, i)
    ems.barrier()
    for(var j = 0;  j < ems.nThreads;  j++) {
	if(arr.read(j) != i) {
	    ems.diag("Barrier array is wrong: was " + arr.read(j) + ' should have been ' + i)
	}
    }
    ems.barrier()
}
