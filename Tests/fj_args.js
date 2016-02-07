/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.1.0   |
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
var ems = require('ems')(parseInt(process.argv[2]), true, 'fj');

var assert;
var global_str;
var check_glob_str;

ems.parallel("This is the block that defines global vars", function () {
    assert = require('assert');
    global_str = '_this is str0_';
    var local_str = 'you should never see this';

    check_glob_str = function () {
        if(ems.myID === 0) {
            assert(global_str === "Process 0 clobbered the global string");
        } else {
            assert(global_str === "_this is str0_Updated by process " + ems.myID);
        }
    }
});


ems.parallel(global_str, 'two', 'three',
    function (a, b, c, taskN) {
        assert(typeof taskN === "undefined", "Mysterious fourth argument:" + taskN);
        assert(typeof local_str === "undefined", "The local string did not stay local");
        ems.diag("global_str=" + global_str + " a =" + a + "  b=" + b + "  c=" + c);
        assert(a === global_str, "A argument not in closure?  (" + JSON.stringify(a) + ")  global_str=" + global_str);
        global_str += "Updated by process " + ems.myID;
    }
);

ems.diag("==============================================This message happens once: Thus concludes first parallel region");
global_str = "Process 0 clobbered the global string";


ems.parallel('xxx', 'yyy', 'zzz', -321,
    function (a, b, c, x, taskN) {
        assert(a === 'xxx'  &&  b === 'yyy'  &&  c === 'zzz'  &&  x === -321  &&  typeof taskN === "undefined",
            "arguments are missing or wrong");
        check_glob_str();
        ems.diag("Second parallel region taskN=" + taskN + "  a =" + a + "  b=" + b + "  c=" + c + "   global_str is now:" + global_str);
    }
);

ems.diag("==============================================This message happens once: This is the barrier");
ems.parallel(function () {
    check_glob_str();
    ems.barrier();
});

ems.diag("==============================================This message happens once: This is the end.");

process.exit(0);
