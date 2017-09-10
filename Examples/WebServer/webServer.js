/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.5   |
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
// Number of processes to use in parallel regions
var nProcs = process.argv[2] || 8;
// Initialize EMS for FJ parallelism
var ems = require('ems')(parseInt(nProcs), true, 'fj');
// Load modules for dealing with HTTP
var http = require('http');
// Load modules for parsing a request URL
var url = require('url');
// Port number to which the server listens
var port = 8080;
// Global, persistent, shared memory that is visible from any process during any HTTP request callback
var globallySharedData;


/**
 * Fork into a parallel region to initialize each parallel processes
 * by attaching the shared server status and loading 'require'd modules
 */
ems.parallel(function () {
    globallySharedData = ems.new({
        dimensions: [1000],
        heapSize: [10000],
        useExisting: false,
        useMap: true,
        doDataFill: true,
        dataFill: 0,
        filename: 'serverState.ems'
    });
});



// Shared memory that last for a session, global so it is scoped to last more more than one request
var sessionSharedData;


/**
 * When an HTTP request arrives this callback is executed.
 * From within that callback, parallel processes are forked
 * to work on the response, then join together back to the original
 * callback thread from where the response is issued.
 * @param request - Request object with URL
 * @param response - Object used respond to the request
 */
function handleRequest(request, response) {
    // Increment (Fetch And Add) the number of requests counter
    var requestN = globallySharedData.faa('nRequests', 1);
    // Request URL parsed into UTF8
    var parsedRequest = url.parse(request.url, true);
    // Session ID part of the request path
    var key = parsedRequest.pathname.split(/\//g)[1];

    if(!key) {
        response.end('ERROR: Incorrectly formatted or missing session unique ID (' + parsedRequest.pathname + ')');
    } else {
        // Fork a parallel region
        ems.parallel(parsedRequest, requestN, key, (parsedRequest, requestN, key) => {
            /// EMS options for the session's shared memory
            var sessionData = {
                dimensions: [100],  // Up to 100 keys in this memory
                heapSize: [100000], // At least 100KB of memory
                useMap: true,       // Index using any type of key, not linear integer indexing
                filename: 'session.' + key + '.ems',  // Filename of persistent memory image
            };

            // Set EMS memory options to create a new or attach to an existing persistent EMS memory
            if (parsedRequest.query.new === "") {
                sessionData.dataFill = 0;
                sessionData.doDataFill = true;
                sessionData.useExisting = false;
                sessionData.doSetFEtags = true;
                sessionData.setFEtags = 'full';
            } else if (parsedRequest.query.old === "") {
                sessionData.useExisting = true;
            } else {
                // Send only one response, but all tasks exit the parallel region
                if (ems.myID === 0) {
                    response.end('ERROR: Invalid new/old state (' + JSON.stringify(parsedRequest.query) + ') for private shared memory');
                }
                return;
            }

            // Create or attach to existing EMS memory
            sessionSharedData = ems.new(sessionData);
            if (!sessionSharedData) {
                // Send only one response, but all tasks exit the parallel region
                if (ems.myID === 0) {
                    response.end('ERROR: Requested "old" shared memory ' + key + ' does not already exist.');
                }
                return;
            }

            // Each task generates it's own random number and performs all the iterations.
            var sum = 0;
            for(var idx = 0;  idx < Math.floor(Math.random() * 1000);  idx += 1) {
                sum += idx;
            }
            // All processes accumulate (Fetch And Add) directly into globally shared counter
            globallySharedData.faa('sum', sum);

            // Distribute (ie: map) the iterations of a loop across all the processes
            ems.parForEach(0, 100000, function(idx) {
                sessionSharedData.faa('bignum', 1);  // Accumulate into a per-session counter
            });

            // Each process appends it's results to the session's EMS memory using Fetch And Add.
            // Note that the memory was initialized with 0 to which this text is appended
            sessionSharedData.faa('results', "(Req:" + requestN + "/" + ems.myID + ") ");
        });
        // All parallel processes have returned before the serial region resumes here

        /// Construct a return object containing data from both the globally shared memory
        /// and session-private data.
        var resp = { 'requestN' : requestN };
        // Parallel region may have exited due to error, in which case there are no results to return
        if (sessionSharedData) {
            resp.sessionData = sessionSharedData.readFF('results');
            resp.bignum = sessionSharedData.readFF('bignum');
            resp.randomSum = globallySharedData.readFF('sum');
            response.end(JSON.stringify(resp));

            if (parsedRequest.query.free === "") {
                sessionSharedData.destroy(true);
            }
        } else {
            response.end('ERROR: Parallel region exited without creating/accessing session shared memory');
        }
    }
}


// Create the Web server
http.createServer(handleRequest).listen(port, function () {
    console.log(
        "REST API:  /UID?[old|new]&[keep|free]\n" +
        "  /UID          Unique session identifier\n" +
        "  ?[old|new]    A 'new' session will remove and create a new named private shared memory\n" +
        "  ?[keep|free]  Release or preserve, respectively, the named private shared memory\n" +
        "\nExamples:\n" +
        "  curl http://localhost:8080/foo?new&keep  # Create a new persistent memory for session \"foo\"\n" +
        "  curl http://localhost:8080/foo?old&keep  # Modify the existing \"foo\" memory\n" +
        "  curl http://localhost:8080/bar?old       # Error because \"bar\" does not yet exist\n" +
        "  curl http://localhost:8080/bar?new&free  # Create and free session memory \"bar\"\n" +
        "\nServer listening on: http://localhost:" + port + "\n"
    );
});
