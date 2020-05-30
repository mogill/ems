/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.5.0   |
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
'use strict';
var ems = require('ems')(parseInt(process.argv[2]));
var assert = require('assert');
var maxlen = 5000000;
var stats = ems.new({
    filename: '/tmp/EMS_strlen',
    dimensions: [10],
    heapSize: (2 * maxlen) + 1 + 10,  // 1 = Null,  10 = length of key
    useMap: true,          // Use a key-index mapping, not integer indexes
    setFEtags: 'full',     // Initial full/empty state of array elements
    doDataFill: true,      // Initialize data values
    dataFill: 0            // Initial value of new keys
});

if (ems.myID === 0) { stats.writeXE('test_str', 123); }
ems.barrier();

function stringFill(x, n) {
    var s = '';
    for (;;) {
        if (n & 1) s += x;
        n >>= 1;
        if (n) x += x;
        else break;
    }
    return s;
}

for (var len=2;  len < maxlen;  len = Math.floor(len * 1.5) ) {
    if (ems.myID === 0) { console.log("Len = " + len); }
    var str = stringFill('x', len);
    stats.writeEF('test_str', str);
    var readback = stats.readFE('test_str');
    assert(readback === str, 'Mismatched string.  Expected len ' + str.length + ' got ' + readback.length);
    ems.barrier();
}


for (var len=maxlen;  len >= 1;  len = Math.floor(len * 0.666) ) {
    if (ems.myID === 0) { console.log("Len = " + len); }
    var str = stringFill('y', len);
    stats.writeEF('test_str', str);
    var readback = stats.readFE('test_str');
    assert(readback === str, 'Mismatched string.  Expected len ' + str.length + ' got ' + readback.length);
    ems.barrier();
}
