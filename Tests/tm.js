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
var ems = require('ems')(parseInt(process.argv[2]));
var util = require('./testUtils');
var assert = require('assert');
var arrLen = 1000000;
var nTransactions = 1000000;
var nTables = 6;
var maxNops = 5;
var tables = [];
var workQ = ems.new({
    dimensions: [nTransactions + ems.nThreads],
    heapSize: nTransactions * 200,
    useExisting: false,
    //dataFill : 0,
    setFEtags: 'empty'
});

var totalNops = ems.new(2);
var checkNops = ems.new(1);


var startTime = util.timerStart();
//---------------------------------------------------------------------------
//  Create all the tables
for (var tableN = 0; tableN < nTables; tableN++) {
    tables[tableN] = ems.new({
        dimensions: [arrLen],
        heapSize: 0,
        useExisting: false,
        filename: '/tmp/EMS_tm' + tableN,
        dataFill: 0,
        doDataFill: true,
        setFEtags: 'full'
    });
}
util.timerStop(startTime, nTables, " tables initialized    ", ems.myID);


//---------------------------------------------------------------------------
//  One thread enqueues all the work
ems.master(function () {
    totalNops.writeXF(0, 0);
    totalNops.writeXF(1, 0);
    checkNops.writeXF(0, 0);
});
ems.barrier();


startTime = util.timerStart();
//---------------------------------------------------------------------------
// Generate operations involving random elements in random EMS arrays
// and enqueue them on the work queue
ems.parForEach(0, nTransactions, function (transN) {
    var ops = [];
    var nOps = util.randomInRange(1, maxNops);
    for (var opN = 0; opN < nOps; opN++) {
        var tableN = util.randomInRange(0, nTables);
        var idx = util.randomInRange(0, arrLen);
        if (transN % 2 == 0 || opN % 3 > 0) {
            ops.push([tableN, idx, true]);
        }
//	if(opN % 3 > 0)  { ops.push([tableN, idx, true]) }
        else {
            ops.push([tableN, idx]);
        }
    }


    // De-duplicate operations in a transaction which would deadlock
    var indicies = [];
    var uids = [];
    for (var i = 0; i < ops.length; i++) {
        indicies[i] = i;
        uids[i] = (ops[i][0] * 1000000000000) + ops[i][1];
    }
    var uniq = [];
    for (opN = 0; opN < ops.length; opN++) {
        var isDupl = false;
        for (var checkN = 0; checkN < ops.length; checkN++) {
            if (opN != checkN && uids[opN] == uids[checkN]) {
                isDupl = true;
                break;
            }
        }
        if (!isDupl) {
            uniq.push(ops[opN]);
        }
    }
    workQ.enqueue(JSON.stringify(uniq));
});

//  After all the work has been enqueued, add DONE semaphores to the
//  end of the queue so they are processed only after all the work
//  has been issued.  Each thread enqueues one event and can only
//  consume one before exiting.
workQ.enqueue("DONE");
util.timerStop(startTime, nTransactions, " transactions enqueued ", ems.myID);


var rwNops = 0;
var readNops = 0;
startTime = util.timerStart();

while (true) {
    var str = workQ.dequeue();
    if (str !== undefined) {
        if (str === "DONE") {
            break;
        } else {
            var ops = JSON.parse(str);
            for (var opN = 0; opN < ops.length; opN++) {
                ops[opN][0] = tables[ops[opN][0]];
            }
            var transaction = ems.tmStart(ops);
            ops.forEach(function (op) {
                var tmp = op[0].read(op[1]);
                if (op[2] != true) {
                    rwNops++;
                    op[0].write(op[1], tmp + 1);
                } else {
                    readNops++;
                }
            });
            ems.tmEnd(transaction, true);
        }
    } else {
        ems.diag("Should not find nothing in queue");
    }
}
totalNops.faa(0, rwNops);
totalNops.faa(1, readNops);

ems.barrier();
util.timerStop(startTime, nTransactions, " transactions performed", ems.myID);
util.timerStop(startTime, totalNops.readFF(0), " table updates         ", ems.myID);
util.timerStop(startTime, totalNops.readFF(0) + totalNops.readFF(1), " elements referenced   ", ems.myID);

startTime = util.timerStart();
ems.parForEach(0, nTables, function (tableN) {
    var localSum = 0;
    for (var idx = 0; idx < arrLen; idx++) {
        localSum += tables[tableN].read(idx);
    }
    checkNops.faa(0, localSum);
}, 'dynamic');
util.timerStop(startTime, nTables * arrLen, " elements checked      ", ems.myID);

ems.master(function () {
    assert(checkNops.readFF(0) == totalNops.readFF(0),
        "Error in final sum = " + checkNops.readFF(0) + "   should be=" + totalNops.readFF(0));
});
