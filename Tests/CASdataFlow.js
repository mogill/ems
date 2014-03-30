/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.0   |
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
var assert = require('assert')
var start, nOps;

function val3d(i, j, k) { return i + (j * 10000) + (k * 100000) }


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




//------------------------------------------------------------------------------
// Compare and Swap numbers
var casBuf = ems.new(ems.nNodes, 10000, '/tmp/EMS_3dstrings')

casBuf.writeXF(ems.myID, 100000+ems.myID);


v = casBuf.readFF(ems.myID)
assert(v === 100000+ems.myID,  "Bad first readback v="+v+"   computed="+100000+ems.myID)
v = 2000000+casBuf.readFE(ems.myID)
casBuf.writeEF(ems.myID, v)

assert(casBuf.readFE(ems.myID) == 2100000+ems.myID,  "Bad second readback")

third = 'third'+ems.myID
casBuf.writeEF(ems.myID, third);

v = casBuf.readFF(ems.myID)
assert(v == third,  "Bad third (string) readback v="+v+"   computed="+third)

v = casBuf.cas(ems.myID, third, 'fourth'+ems.myID)
assert(casBuf.readFE(ems.myID) == 'fourth'+ems.myID,  "Bad fourth (string) mem="+v+"|  old="+third+"|   computed="+'fourth'+ems.myID)

casBuf.writeEF(ems.myID, 100*ems.myID)

assert(casBuf.readFF(ems.myID) == 100*ems.myID, "Failed to go from strings to numbers")

ems.barrier()

start = new Date().getTime()
nIters = 50000;
for(var i = 0;  i < nIters;  i++) {
    var oldVal = -123
    while(oldVal != ems.myID) {
	var oldVal = casBuf.cas(0, ems.myID, (ems.myID+1)%ems.nNodes )
    }
}
stopTimer(start, nIters * ems.nNodes, " CAS Numbers      ")


ems.barrier()
//------------------------------------------------------------------------------
// Compare and Swap strings
ems.master( function() {casBuf.writeXF(0, 'test0')} )
ems.barrier()
start = new Date().getTime()
nIters = 50000

for(var i = 0;  i < nIters;  i++) {
    var oldVal = "no match"
    var testMe = 'test'+ems.myID
    var testNext = 'test'+((ems.myID+1)%ems.nNodes)
    while(oldVal != testMe) {
	var oldVal = casBuf.cas(0, testMe, testNext)
    }
}
ems.barrier()
var rff = casBuf.readFF(0)
assert(rff == 'test0', "Incorrect final CAS string: "+rff+"  test0")
stopTimer(start, nIters * ems.nNodes, " CAS Strings      ")

ems.barrier()



//------------------------------------------------------------------------------
//    Clobbering old casBuf definition forces destructor to be called
var casBuf = ems.new(1, 10000, '/tmp/EMS_3dstrings')  // TODO : memory allocator for strings and objects


