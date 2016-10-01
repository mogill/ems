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
var ems = require('ems')(parseInt(process.argv[2]), false);
var assert = require('assert');
var start;
var dim1 = ems.new(1000);
var dims3d = [800, 120, 50];
var dims2d = [5000, 500];
var dim3 = ems.new(dims3d, 0, "/tmp/EMS_3d_space");
var dim2 = ems.new(dims2d, 800000000);
var idx = ems.myID;
var i, j, k;

dim1.write(idx * 10, idx * 100);
var v = dim1.read(idx * 10);
assert(v == ems.myID * 100, "Since 1d write-read failed");

function val3d(i, j, k) {
    return i + (j * 10000) + (k * 100000);
}


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
    })
}

//------------------------------------------------------------------------------
// Spinup loop to physically allocate all memory as RW
ems.barrier();
start = new Date().getTime();
for (k = ems.myID; k < dims3d[2]; k += ems.nThreads) {
    for (j = 0; j < dims3d[1]; j += 1) {
        for (i = 0; i < dims3d[0]; i += 1) {
            dim3.write([i, j, k], val3d(i, j, k));
        }
    }
}
ems.barrier();
stopTimer(start, dims3d[0] * dims3d[1] * dims3d[2], " First touch      ");


//------------------------------------------------------------------------------
// Test read & write on all nodes
ems.barrier();
start = new Date().getTime();
for (k = ems.myID; k < dims3d[2]; k += ems.nThreads) {
    for (j = 0; j < dims3d[1]; j += 1) {
        for (i = 0; i < dims3d[0]; i += 1) {
            dim3.write([i, j, k], val3d(k, j, i));  // different pattern;
        }
    }
}

for (k = ems.myID; k < dims3d[2]; k += ems.nThreads) {
    for (j = 0; j < dims3d[1]; j += 1) {
        for (i = 0; i < dims3d[0]; i += 1) {
            assert(dim3.read([i, j, k]) === val3d(k, j, i),
                "Failed to verify parallel 3D data " + dim3.read([i, j, k]) +
                " != " + val3d(k, j, i) + "  " + i + " " + j + " " + k);
        }
    }
}

ems.barrier();
stopTimer(start, 2 * dims3d[0] * dims3d[1] * dims3d[2], " Read/Write ops   ");


//------------------------------------------------------------------------------
// How long would it take node 0 alone on native array?
ems.barrier();
start = new Date().getTime();
ems.master(function () {
    var native = new Array(dims3d[0] * dims3d[1] * dims3d[2]);
    for (k = 0; k < dims3d[2]; k += 1) {
        for (j = 0; j < dims3d[1]; j += 1) {
            for (i = 0; i < dims3d[0]; i += 1) {
                var idx = 0;
                dims3d.forEach(function (x, i) {
                    idx += x * dims3d[i]
                });

                native[idx] = val3d(i, j, k)
            }
        }
    }
});
ems.barrier();
stopTimer(start, dims3d[0] * dims3d[1] * dims3d[2], " native array     ");


//------------------------------------------------------------------------------
// Critical Regions
dim1.write(30, 3333333);
var prev = dim1.read(30);
ems.barrier();
start = new Date().getTime();

var nIters = Math.floor(1000000 / ems.nThreads);
for (i = 0; i < nIters; i++) {
    ems.critical(function () {
        var x = dim1.read(30);
        x++;
        dim1.write(30, x);
    }, 1000);  // TODO: Write proper fail case test for critical timeout
}


ems.barrier();
stopTimer(start, nIters * ems.nThreads, " critical regions ");
ems.master(function () {
    assert(dim1.read(30) === (prev + (ems.nThreads * nIters)),
        "Critical region was racing x=" + dim1.read(30) + "   sum=" + (prev + (ems.nThreads * nIters)) +
        "  prev=" + prev);
});


//------------------------------------------------------------------------------
// Purge D2
start = new Date().getTime();
for (j = ems.myID; j < dims2d[1]; j += ems.nThreads) {
    for (i = 0; i < dims2d[0]; i += 1) {
        dim2.writeXE([i, j], -val3d(i + 10, j + 10, 0));
    }
}
ems.barrier();
stopTimer(start, dims2d[0] * dims2d[1], " writeXF purges   ");


//------------------------------------------------------------------------------
// ReadFE then WriteEF
start = new Date().getTime();

if (ems.myID != 0) {
    for (j = ems.myID; j < dims2d[1]; j += ems.nThreads) {
        for (i = 0; i < dims2d[0]; i += 1) {
            assert(dim2.readFF([i, j]) === val3d(i + 10, j + 10, 0),
                "Failed to verify 2D FE data: " +
                dim2.readFF([i, j]) + "  " + val3d(i + 10, j + 10, 0) + "   i-j: " + i + " " + j);
        }
    }
} else {
    for (j = 0; j < dims2d[1]; j += 1) {
        for (i = 0; i < dims2d[0]; i += 1) {
            dim2.writeEF([i, j], val3d(i + 10, j + 10, 0));
        }
    }
    for (i = 0; i < dims2d[0]; i += 1) {
        assert(dim2.readFF([i, 0]) === val3d(i + 10, 10, 0),
            "Failed to verify 2D FE node 0 data: " +
            dim2.readFF([i, 0]) + "  " + val3d(i + 10, 10, 0) + "   i-j: " + i + " " + 0);
    }
}
ems.barrier();
stopTimer(start, 2 * dims2d[0] * dims2d[1], " FE-EF Dataflow   ");


//---------------------------------------------------------------
//  Redo dataflow but using strings
start = new Date().getTime();
for (j = ems.myID; j < dims2d[1]; j += ems.nThreads) {
    for (i = 0; i < dims2d[0]; i += 1) {
        dim2.writeXE([i, j], 'mem' + (-1 * val3d(i + 10, j + 10, 0)));
    }
}
ems.barrier();
stopTimer(start, dims2d[0] * dims2d[1], " XF srting purge  ");


//------------------------------------------------------------------------------
// ReadFE then WriteEF
start = new Date().getTime();

if (ems.myID != 0) {
    for (j = ems.myID; j < dims2d[1]; j += ems.nThreads) {
        for (i = 0; i < dims2d[0]; i += 1) {
            assert(dim2.readFF([i, j]) === 'mem' + (val3d(i + 10, j + 10, 0)),
                "Failed to verify 2D string FE data: " +
                dim2.readFF([i, j]) + "  " + val3d(i + 10, j + 10, 0) + "   i-j: " + i + " " + j);
        }
    }
} else {
    for (j = 0; j < dims2d[1]; j += 1) {
        for (i = 0; i < dims2d[0]; i += 1) {
            dim2.writeEF([i, j], 'mem' + val3d(i + 10, j + 10, 0))
        }
    }
    for (i = 0; i < dims2d[0]; i += 1) {
        assert(dim2.readFF([i, 0]) === 'mem' + val3d(i + 10, 10, 0),
            "Failed to verify 2D FE node 0 data: " +
            dim2.readFF([i, 0]) + "  " + val3d(i + 10, 10, 0) + "   i-j: " + i + " " + 0);
    }
}
ems.barrier();
stopTimer(start, 2 * dims2d[0] * dims2d[1], " Dataflow w/strgs ");



