/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.0.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2016, Jace A Mogill.  All rights reserved.              |
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
'use strict';
var util = require('util');
var assert = require('assert');
var ems = require('ems')(1, 0, 'bsp', '/tmp/facto.ems');
var array_len = 10000;
var ems_config = {
    dimensions  : [array_len],
    heapSize    : 10000,
    useMap      : false,
    useExisting : false,
    filename    : '/tmp/refactor.ems'
};

var sums = ems.new(ems_config);

ems_config.useMap = true;
ems_config.filename = '/tmp/anotherplace';
var emptyland = ems.new(ems_config);

ems.barrier();
/*
setTimeout(function () {
    // console.log("Pre timeout barrier", ems.myID);
    ems.barrier();
    console.log("Post timeout barrier", ems.myID);
}, ems.myID * 3000);
*/

var magicval = 0x15432100 + ems.myID;
sums.writeXF(ems.myID, magicval);
var readback = sums.readFF(ems.myID);
assert(readback === magicval, "Expected " + magicval.toString() + ", got " + readback.toString());
sums.readFF(ems.myID);

ems.barrier();
ems.barrier();

sums.writeXE(ems.myID, "Nothing to see");
var idx, teststr;
for (idx = 0;  idx < 100000;  idx += 1) {
    teststr = "some text" + idx;
    sums.writeEF(ems.myID, teststr);
    readback = sums.readFE(ems.myID);
    assert(teststr === readback, "Nope...|" + teststr + "|" + readback + "|   len=" + teststr.length + "/" + readback.length);
}

for (idx = 0;  idx < 100000;  idx += 1) {
    teststr = 'foo' + (idx * 3 + idx * 7);
    readback = emptyland.read(teststr);
    assert(readback === undefined, "Read actual data during undefined map test:" + teststr);
}

var oldval;
for (idx = 0;  idx < array_len / 40;  idx += 1) {
    teststr = 'foo' + (idx * 5) + (idx * array_len);

    readback = emptyland.read(teststr);
    assert(readback === undefined, "CAS phase: Read actual data during undefined map test:" + teststr);
    readback = emptyland.read(teststr);
    assert(readback === undefined, "CAS phase: Second Read actual data during undefined map test:" + teststr);

    oldval = emptyland.cas(teststr, undefined, teststr);
    assert(oldval === undefined, "CAS at teststr(" + teststr + ") wasn't undefined -- value was:" + oldval + "/" + (oldval || 0xf00dd00d).toString(16));
    var newmemval = emptyland.readFF(teststr);
    assert(teststr === newmemval, "wrong post CAS mem value -- value was:" + newmemval + "   expected:" + teststr);
}


// Check index2key
var all_types_len = 10;
var all_types = ems.new({
    dimensions  : [ all_types_len ],
    heapSize    : 10000,     // Optional, default=0: Space, in bytes, for
    useMap      : true,       // Optional, default=false: Map keys to indexes
    filename    : '/tmp/refactor.ems'  // Optional, default=anonymous:
});

var all_data_types = [false, true, 1234, 987.654321, 'hello'];
all_data_types.forEach(function(type) {
    console.log("Trying type", type);
    all_types.writeXF(type, type);
});

for(idx = 0;  idx < all_types_len;  idx += 1) {
    console.log("Index:" + idx + " -- key:" + all_types.index2key(idx));
}
