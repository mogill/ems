/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.3.0   |
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
var ems = require('ems')(parseInt(process.argv[2]), false);
var assert = require('assert');
var arrLen = 10000;
var a = ems.new(arrLen, arrLen * 400);

var arrayElem = ['abcd', true, 1234.567, false, {x: 'xxx', y: 'yyyyy'}, 987, null, [10, 11, 12, 13]];
var objElem = {a: 1, b: 321.653, c: 'asdasd'};

var objMap = ems.new({
    dimensions: [arrLen],
    heapSize: arrLen * 200,
    useMap: true,
    useExisting: false,
    setFEtags: 'full'
});


var arrObj = ems.new({
    dimensions: [arrLen],
    heapSize: arrLen * 200,
    useMap: false,
    useExisting: false,
    setFEtags: 'full',
    dataFill: arrayElem,
    doDataFill: true
});


if (ems.myID == 0) {
    objMap.writeXF('any obj', objElem);
    assert(objMap.readFF('any obj').a === objElem.a);
    assert(objMap.readFF('any obj').b === objElem.b);
    assert(objMap.readFE('any obj').c === objElem.c);

    arrayElem.forEach(function (elem, idx) {
        if (typeof elem === 'object') {
            assert(typeof arrObj.readFF(123)[idx] === 'object');
        } else {
            var readback = arrObj.readFF(123);
            assert(readback[idx] === elem);
        }
    });
    arrObj.readFE(123);


    var newObj = {xa: 10000, xb: 32100.653, xc: 'xxxxxxxasdasd'};
    objMap.writeEF('any obj', newObj);
    assert(objMap.readFF('any obj').xa === newObj.xa);
    assert(objMap.readFF('any obj').xb === newObj.xb);
    assert(objMap.readFE('any obj').xc === newObj.xc);

    arrObj.writeEF(123, 'overwrite the old array');
    arrObj.writeXF(1, []);
    var newArr = [9, 8, 7, 6, , , , 'abs', {one: 1, two: 2, three: 'threeeeee'}, 1, 2, 3];
    arrObj.writeXF(2, newArr);

    assert(arrObj.readFE(123) === 'overwrite the old array');
    assert(arrObj.readFE(1).length === 0);
    newArr.forEach(function (elem, idx) {
        if (typeof elem == 'object') {
            if (typeof arrObj.readFF(2)[idx] != 'object') {
                console.log('Object in array is no longer an object?');
            }
        } else {
            assert(arrObj.readFF(2)[idx] === elem);
        }
    });

    var newerObj = {q: 123, r: 345, x: [1, 2, 3, 4]};
    arrObj.writeEF(123, newerObj);
    assert(arrObj.readFF(123).q === newerObj.q);
    assert(arrObj.readFF(123).r === newerObj.r);
    assert(arrObj.readFF(123).x[2] === newerObj.x[2]);

}

ems.barrier();

//----------------------------------------

var newIdx, newVal, oldVal, js;
var data = [false, true, 1234, 987.654321, 'hello', undefined];

for (var old = 0; old < data.length; old++) {
    for (newIdx = 0; newIdx < data.length; newIdx++) {
        a.writeXF(ems.myID, data[old]);
        js = data[old];
        js += data[newIdx];
        oldVal = a.faa(ems.myID, data[newIdx]);
        newVal = a.readFF(ems.myID);

        assert(((newVal === js) || (isNaN(oldVal) && isNaN(newVal) ) ||
        (isNaN(newVal) && data[newIdx] === undefined) ),
            'FAA: old=' + data[old] + '   new=' + data[newIdx] +
            '  oldVal=' + oldVal + '/' + typeof oldVal + '   newVal=' +
            newVal + '/' + typeof newVal + '  js=' + js + '/' + typeof js);
    }
}


ems.barrier();
var id = (ems.myID + 1) % ems.nThreads;
for (var memIdx = 0; memIdx < data.length; memIdx++) {
    for (var oldIdx = 0; oldIdx < data.length; oldIdx++) {
        for (newIdx = 0; newIdx < data.length; newIdx++) {
            a.writeXF(id, data[memIdx]);
            var memVal = a.cas(id, data[oldIdx], data[newIdx]);
            newVal = a.readFF(id);
            js = data[memIdx];
            if (js === data[oldIdx]) {
                js = data[newIdx];
            }

            assert(js === newVal,
                'CAS: intial=' + data[memIdx] + "    memval=" + memVal +
                "   readback=" + newVal + "  oldVal=" + data[oldIdx] +
                '   expected=' + data[newIdx] + '    JS:' + js);
        }
    }
}


ems.parForEach(0, arrLen, function (idx) {
    a.writeXF(idx, undefined);
    a.faa(idx, undefined);
    a.faa(idx, 'bye byte');
    a.faa(idx, ems.myID);
    a.faa(idx, 0.1);
    a.faa(idx, false);
    assert(a.readFF(idx) == 'nanbye byte' + ems.myID + '0.100000false',
        'Failed match =' + a.read(idx));
});


//-----------------------------------------------------------


ems.parForEach(0, arrLen, function (idx) {
    a.writeXF(idx, 0);
});

var nTimes = 150;
ems.parForEach(0, arrLen, function (previdx) {
    for (var i = 0; i < nTimes; i++) {
        a.faa((previdx + i) % arrLen, 1)
    }
});

ems.parForEach(0, arrLen, function (idx) {
    a.faa(idx, 0.1);
    a.faa(idx, 'fun!');
    a.faa(idx, false);
    assert(a.readFE(idx) == nTimes + '.100000fun!false',
        'Failed match =' + a.read(idx) + '   idx=' + idx);
    });
