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
var assert = require('assert');
var ems = require('ems')(parseInt(process.argv[2]), false);
var a = ems.new(1, 0, '/tmp/EMS_a');
var arrLen = 10000;
var nTimes;
var nLocks;
var count = ems.new({
    dimensions: [10],
    heapSize: arrLen * 200,
    useMap: true,
    useExisting: false,
    setFEtags: 'full'
});

a.writeXF(0, 123);
count.writeXF(0, 0);
ems.barrier();

for (nTimes = 0; nTimes < 100000; nTimes += 1) {
    a.readRW(0);
    nLocks = count.faa(0, 1) + 1;
    assert(nLocks <= 7, "Too many locks: " + nLocks);
    nLocks = count.faa(0, -1) - 1;
    assert(nLocks >= 0, "Too few locks: " + nLocks);
    for (var i = 0; i < 100; i++) {
        nLocks += Math.sin(i);
    }
    a.releaseRW(0);
}

ems.barrier();

var objMap = ems.new({
    dimensions: [arrLen],
    heapSize: arrLen * 200,
    useMap: true,
    useExisting: false,
    setFEtags: 'full'
});

['abcd', 1234.567, true, 987].forEach( function(elem) {
    objMap.writeXF(elem, elem);
    count.writeXF(elem, 0);
});
ems.barrier();

['abcd', 1234.567, true, 987].forEach( function(elem) {
    for (var nTimes = 0; nTimes < 10000; nTimes++) {
        var readback = objMap.readRW(elem);
        assert(readback === elem,
            "Multi Reader read wrong data.  Expected(" + elem + "), got(" + readback + ")");
        var nLocks = count.faa(elem, 1) + 1;
        assert(nLocks <= 7, "Too many locks: " + nLocks);
        nLocks = count.faa(elem, -1) - 1;
        assert(nLocks >= 0, "Too few locks: " + nLocks);
        for (var i = 0; i < 100; i++) {
            nLocks += Math.sin(i);
        }
        nReaders = objMap.releaseRW(elem);
    }
});
