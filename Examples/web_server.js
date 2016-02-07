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
var http = require('http');
var port = 8080;
var shared_data;

/* Connect to the shared data on every task.  The map key will be
 * the URL, the value will be the concatenation of work performed
 * by all the processes.  */
ems.parallel(function () {
    shared_data = ems.new({
        dimensions: [1000],
        heapSize: [100000],
        useExisting: false,
        useMap: true,
        setFEtags: 'full',
        filename: '/tmp/EMS_shared_web_data.ems'
    });
});


// When a request arrives, each process does some work and appends the results
// to shared memory
function handleRequest(request, response) {
    // If this URL has not yet been requested, the data is undefined
    // and must be initialized to
    // Alternatively, may be initialized not here but at ems.new()
    shared_data.cas(request.url, undefined, "Response preamble.");

    // Enter a parallel region, each process does some work, and then
    // appends the result the value.
    ems.parallel(request.url, function (url) {
        // Do some work
        shared_data.faa(url, "  Work from process " + ems.myID + ".");
        ems.barrier();  // Wait for all processes to finish region before any may exit
    });

    // Return the results, leaving them in shared memory for later updates
    response.end('Shared results from(' + request.url + "):" + shared_data.readFF(request.url));
}

// Create the Web server
http.createServer(handleRequest).listen(port, function () {
    ems.diag("Server listening on: http://localhost:" + port);
});
