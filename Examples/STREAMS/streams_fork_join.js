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
//
//  Based on John D. McCalpin's (Dr. Bandwdith) STREAMS benchmark:
//         https://www.cs.virginia.edu/stream/
//
//  Also see the Bulk Synchronous Parallel implementation in the Examples directory
// ===============================================================================
'use strict';
var ems = require('ems')(parseInt(process.argv[2]), true, 'fj');

ems.parallel( function() {
    var arrLen = 1000000;
    var array_a = ems.new(arrLen);
    var array_b = ems.new(arrLen);
    var array_c = ems.new(arrLen);

    ems.parForEach(0, arrLen, function(idx) { array_a.write(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.writeXE(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.writeXF(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.read(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.readFF(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.readFE(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_a.writeEF(idx, idx)});
    ems.parForEach(0, arrLen, function(idx) { array_b.writeXF(idx, array_a.readFF(idx))});
    ems.parForEach(0, arrLen, function(idx) { array_c.writeXF(idx, array_a.readFF(idx) * array_b.readFF(idx))});
    ems.parForEach(0, arrLen, function(idx) { array_c.writeXF(idx, array_c.readFF(idx) + (array_a.readFF(idx) * array_b.readFF(idx)))});
} );

// Fork-Join tasks must exit themselves, otherwise they will wait indefinitely for more work
process.exit(0);
