/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.0   |
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
var ems = require('..')(parseInt(process.argv[2]));
var assert = require('assert');
var arrLen = 1000000;
var a = ems.new({
    dimensions: [arrLen],
    heapSize: arrLen * 100,
    useExisting: false,
    filename: '/tmp/EMS_stack',
    doSetFEtags: true,
    setFEtags: 'empty',
    doSetDataFill: true,
    dataFill: undefined
});

var b = ems.new({
    dimensions: [arrLen],
    heapSize: arrLen * 100,
    doSetFEtags: true,
    setFEtags: 'empty',
    doSetDataFill: true,
    dataFill: undefined
});

var timeStart, tmp, i, idx;


//-------------------------------------------------------------------
//  Timer function
function stopTimer(timer, nOps, label) {
    function fmtNumber(n) {
        var s = '                                ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
        if (n < 1) return n;
        else {
            return s.substr(s.length - 15, s.length);
        }
    }

    ems.master(function () {
        var now = new Date().getTime();
        var x = (nOps * 1000000) / ((now - timer) * 1000);
        ems.diag(fmtNumber(nOps) + label + fmtNumber(Math.floor(x).toString()) + " ops/sec");
    });
}

var x = a.pop();
assert(x === undefined, "Initial: should have been empty: " + x);
ems.barrier();

a.push(1000 + ems.myID);
ems.barrier();
x = a.pop();
assert(x >= 1000, "Expected something >= 1000 on the stack, found " + x);
ems.barrier();
x = a.pop();
assert(x === undefined, "Expected nothing on the stack, found: " + x);

ems.barrier();
a.push(2000.12345 + ems.myID);
a.push('text ' + ems.myID + 'yeah...');
ems.barrier();
assert(a.pop() !== undefined, "Expected text or ### on the stack, was undefined.")
assert(a.pop() !== undefined, "Expected second text or ### on the stack, was undefined.")
ems.barrier();
assert(a.pop() === undefined, "after two pops stack should have been empty");
ems.barrier();
a.push(3330.12345 + ems.myID);
a.push('more text ' + ems.myID + 'yeah...');
assert(a.pop() !== undefined, "Expected more text or more ### on the stack, was undefined.")
assert(a.pop() !== undefined, "Expected second more text or more ### on the stack, was undefined.")
ems.barrier();
assert(a.pop() === undefined, "repeated two pops should have been empty");

var value = b.dequeue();
ems.barrier();
assert(value === undefined, "Intitial b dequeue should have beeen undefined, was " + value);
ems.barrier();
idx = b.enqueue(4000 + ems.myID);

ems.barrier();
ems.master(function () {
    tmp = b.dequeue();
    var count = 0;
    while (tmp !== undefined) {
        tmp = b.dequeue();
        count++;
    }
    assert(count === ems.nThreads, "Didn't find enough queued items");
    assert(a.pop() === undefined, "dq1: should still have been empty");
});
ems.barrier();
tmp = b.dequeue();
assert(tmp === undefined, "DQ should be mt: " + tmp);
ems.barrier();
ems.master(function () {
    for (i = 0; i < arrLen; i++) {
        var str = "sequential" + (i * 100);
        b.enqueue(str);
    }
    for (i = 0; i < arrLen; i++) {
        tmp = b.dequeue();
        assert(tmp === "sequential" + (i * 100), "Iter " + i + " Should have found ... got " + tmp);
    }
    assert(a.pop() === undefined, "a pop again should have been empty");
});
ems.barrier();

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function (idx) {
    b.enqueue('foo' + idx);
});
stopTimer(timeStart, arrLen, " enqueued ");

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function () {
    b.dequeue();
});
stopTimer(timeStart, arrLen, " dequeued ");


tmp = b.dequeue();
assert(tmp === undefined, "DQ at end should be mt: " + tmp);

var p = ems.new({
    dimensions: [arrLen + 1],
    heapSize: arrLen * 50,
    doSetFEtags: true,
    setFEtags: 'empty',
    dataFill: undefined
});
timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function (idx) {
    p.push('foo' + idx);
});
stopTimer(timeStart, arrLen, " pushed ");

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function () {
    p.pop();
});
stopTimer(timeStart, arrLen, " popped ");

ems.barrier();
var c = [];
timeStart = new Date().getTime();
ems.master(function () {
    for (idx = 0; idx < arrLen; idx++) {
        c.push('foo' + idx);
    }
});
stopTimer(timeStart, arrLen, " native enqueued ");

timeStart = new Date().getTime();
ems.master(function () {
    for (idx = 0; idx < arrLen; idx++) {
        c.pop();
    }
});
stopTimer(timeStart, arrLen, " native dequeued ");

tmp = b.dequeue();
assert(tmp === undefined, "DQ of B after native end should be mt: " + tmp);

/////////////////////////////////////////

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function (idx) {
    b.enqueue(idx);
});
stopTimer(timeStart, arrLen, " int enqueued ");

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function () {
    b.dequeue();
});
stopTimer(timeStart, arrLen, " int dequeued ");


tmp = b.dequeue();
assert(tmp === undefined, "DQend second time should be mt: " + tmp);


p = ems.new({
    dimensions: [arrLen + 1],
    heapSize: arrLen * 50,
    doSetFEtags: true,
    setFEtags: 'empty',
    dataFill: undefined
});
timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function (idx) {
    p.push(idx);
});
stopTimer(timeStart, arrLen, " int pushed ");

timeStart = new Date().getTime();
ems.parForEach(0, arrLen, function () {
    p.pop();
});
stopTimer(timeStart, arrLen, " int popped ");

ems.barrier();
c = [];
timeStart = new Date().getTime();
ems.master(function () {
    for (idx = 0; idx < arrLen; idx++) {
        c.push(idx);
    }
});
stopTimer(timeStart, arrLen, " int native enqueued ");

timeStart = new Date().getTime();
ems.master(function () {
    for (idx = 0; idx < arrLen; idx++) {
        c.pop();
    }
});
stopTimer(timeStart, arrLen, " int native dequeued ");

tmp = b.dequeue();
assert(tmp === undefined, "Final deque should be mt: " + tmp);

