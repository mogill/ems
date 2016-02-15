/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.2.0   |
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
var querystring = require("querystring");
var url_module = require("url");
var port = 8080;
var crypto;
var shared_counters;
var ephemeral_data;
var preamble;


/* Connect to the shared data on every task.  The map key will be
 * the URL, the value will be the concatenation of work performed
 * by all the processes.  */
ems.parallel(function () {
    crypto = require('crypto');
    preamble = '------ Response Preamble ---------------\n';
    shared_counters = ems.new({
        dimensions: [1000],
        heapSize: [10000],
        useExisting: false,
        useMap: true,
        doDataFill: true,
        dataFill: 0,
        setFEtags: 'full',
        filename: '/tmp/counters.ems'
    });
});


// When a request arrives, each process does some work and appends the results
// to shared memory
function handleRequest(request, response) {
    var requestN = shared_counters.faa('nRequests', 1);  // Increment the count of requests
    var parsed_request = url_module.parse(request.url, true);
    var useExisting          = parsed_request.pathname.indexOf("/existing") >= 0;
    var persistent           = parsed_request.pathname.indexOf("/persist") >= 0;
    var reset_after_response = parsed_request.pathname.indexOf("/reset") >= 0;
    var make_req_unique      = parsed_request.pathname.indexOf("/unique") >= 0;
    var key = Object.keys(parsed_request.query)[0];
    var value = parsed_request.query[key];
    ems.diag("persist=" + persistent + "   reset_after=" + reset_after_response +
        "  useExisting=" + useExisting + "  unique=" + make_req_unique +
        "   key=" + key + "  val=" + value);

    if (value === undefined) {
        response.end('ERROR: No key/value query');
        return;
    }

    ems.parallel(persistent, useExisting, function (persistent, useExisting) {
        if (persistent) {
            preamble = '------ Appending to Persistent Preamble ---------------\n';
        } else {
            preamble = '------ New EMS Array Preamble --------------\n';
        }
        var options = {
            dimensions: [1000],
            heapSize: [100000],
            useExisting: useExisting,
            useMap: true,
            filename: '/tmp/ephemeral_shared_web_data.ems'
        };
        if (!useExisting) {
            options.doDataFill = true;
            options.dataFill   = preamble;
            options.setFEtags  = 'full';
        }
        ephemeral_data = ems.new(options);
    });


    // If the request URL is "unique", generate a hashed GUID to append to the URL
    if (make_req_unique) {
        var hash = crypto.createHash('sha256');
        hash.update(shared_counters.faa('GUID', 1).toString());
        key += '-GUID-' + hash.digest('hex');
    }

    // Enter a parallel region, each process does some work, and then
    // appends the result the value.
    ems.parallel(requestN, key, value, function (requestN, key, value) {
        // Do some work
        ephemeral_data.faa(key, "  Request " + requestN + " done by process " +
            ems.myID + " using data: " + value + "\n");
    });

    ephemeral_data.faa(key, 'Persist this data: ' + persistent + '\n---- Request ' + requestN + ' epilogue -----\n');

    // Return the results, leaving them in shared memory for later updates
    response.end('Shared results from(' + key + "):\n" + "ephemeral_data\n" + ephemeral_data.readFF(key));

    if (reset_after_response) { ephemeral_data.writeXF(key, preamble); }
    if (!persistent) { ephemeral_data.destroy(true); }
}


// Seed the GUID generator with a unique starting value
shared_counters.writeXF('GUID', Math.floor(Math.random() * 10000000));

// Create the Web server
http.createServer(handleRequest).listen(port, function () {
    console.log(
        "REST API:\n" +
        "  ?key=value   Key-Value pair to store in the ephemeral EMS array\n" +
        "  /persist     The ephemeral EMS data is not removed at the end of the query\n" +
        "  /reset       After the response is sent the Key-Value added is restored to the initial value\n" +
        "  /unique      The key is made unique\n" +
        "  /existing    Persisted ephemeral EMS is used, otherwise re-initialize the EMS array\n" +
        "               Note: If the ephemeral data was not persisted, the server will fail.\n" +
        "\nExamples:\n" +
        "  curl http://localhost:8080/?foo=data1   # Insert ephemeral data1 with the key \"foo\"\n" +
        "  curl http://localhost:8080/persist?foo=data2   # Previous data was not persistent, this is a new persistent foo\n" +
        "  curl http://localhost:8080/existing/persist?foo=data3   # This data is appended to the already persisted data\n" +
        "  curl http://localhost:8080/existing/persist/unique?foo=nowhere  # A GUID is generated for this operation\n" +
        "  curl http://localhost:8080/existing/persist/reset?foo=data5  # Persisted data is appended to, the response is issued, and the data is deleted \n" +
        "  curl http://localhost:8080/existing?foo=final  # Persisted reset data is appended to\n" +
        "\nServer listening on: http://localhost:" + port + "\n"
    );

});
