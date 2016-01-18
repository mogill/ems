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
var ems = require('ems')(process.argv[2]);
var assert = require('assert');

var iter, idx, memVal;
var nIters = Math.floor(100000 / process.argv[2]);
var sums = ems.new(ems.nThreads);
sums.writeXF(ems.myID, 0);

for (iter = 0; iter < nIters; iter += 1) {
    ems.barrier();
    idx = (ems.myID + iter) % ems.nThreads;
    memVal = sums.read(idx);
    assert(memVal === iter,
        "myID=" + ems.myID + "  iter=" + iter + "  memval=" + memVal);
    sums.write(idx, memVal + 1);
}
ems.barrier();


for (iter = 0; iter < nIters; iter += 1) {
    ems.barrier();
    idx = (ems.myID + iter) % ems.nThreads;
    memVal = sums.read(idx);
    sums.write(idx, memVal + 1);
}
ems.barrier();

assert(sums.read(ems.myID) === (nIters * 2),
    "myID=" + ems.myID + "  wrong final  memval=" + sums.read(ems.myID) + "  expected=" + (nIters * 2));

ems.barrier();

var shared = ems.new(process.argv[2]*2, 0, "/tmp/EMS_mynewFoo");

shared.write(0, 0);
ems.barrier();
var tmp = shared.read(0);
assert(tmp === 0, "Didn't initialize to 0, got " + tmp);
ems.barrier();
shared.faa(0, 1);
ems.barrier();
assert(shared.read(0) === ems.nThreads, "Didn't count (" + shared.read(0) + ") to nnodes (" + ems.nThreads + ")");
ems.barrier();
if (ems.myID === 0) {
    shared.write(0, 0);
}
ems.barrier();
assert(shared.read(0) === 0, "reinit Didn't initialize to 0, got " + shared.read(0));
ems.barrier();

var nIter = 10000;
var i;
for (i = 0; i < nIter; i += 1) {
    shared.faa(0, 1);
}
ems.barrier();
assert(shared.read(0) === ems.nThreads * nIter, "Random wait FAA failed: count (" + shared.read(0) + ") to nnodes (" + nIter + ")");

ems.barrier();
if (ems.myID === 0) {
    shared.write(0, 0);
}
ems.barrier();
if (ems.myID !== 0) {
    assert(shared.read(0) === 0, "Didn't pass sync after clearing");
}

ems.barrier();
// nIter = 10000 / process.argv[2];

for (i = 0; i < nIter; i += 1) {
    shared.faa(0, 1);
}
ems.barrier();
var sr = shared.read(0);
ems.barrier();
assert(sr === ems.nThreads * nIter, "Fast Looped FAA failed: count (" + sr + ") to nnodes (" + nIter + ")");
