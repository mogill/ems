/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.0.7   |
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
var ems = require('ems')(parseInt(process.argv[2]));
var fs = require('fs');


//-------------------------------------------------------------------
//  Allocate the word count dictionary
var maxNKeys = 100000000;
var wordCounts = ems.new({
    dimensions: [maxNKeys],  // Maximum # of different keys the array can store
    heapSize: maxNKeys * 10, // 10 bytes of storage per key, used for key (dictionary word) itself
    useMap: true,            // Use a key-index mapping, not integer indexes
    setFEtags: 'full',       // Initial full/empty state of array elements
    doDataFill: true,        // Initialize data values
    dataFill: 0              // Initial value of new keys
});


//-------------------------------------------------------------------
//  Use a key-value mapped array to store execution statistics
var nStats = 200;
var stats = ems.new({
    dimensions: [nStats],  // Maximum number of stats that can be stored
    heapSize: nStats * 200,// Space for keys, data values, and sorting
    useMap: true,          // Use a key-index mapping, not integer indexes
    setFEtags: 'full',     // Initial full/empty state of array elements
    doDataFill: true,      // Initialize data values
    dataFill: 0            // Initial value of new keys
});


// ===============================================================================
//  Perform Word Count
//
//  Use the word as a key to the EMS array, the value is the count of the
//  number of times the word has been encountered. This is incremented
//  atomically.
//
//  Bookkeeping for statistics is performed by keeping track of the total number
//  of words and bytes processed in the "stats" EMS array.
//
var doc_path = '/path/to/your/document/collection/';
if (!process.argv[3]) {
    console.log("usage: wordCount <# processes> /path/to/your/document/collection/");
    return -1;
} else {
    doc_path = process.argv[3];
}
var file_list = fs.readdirSync(doc_path);
var splitPattern = new RegExp(/[ \n,\.\\/_\-\<\>:\;\!\@\#\$\%\&\*\(\)=\[\]|\"\'\{\}\?\—]/);


//  Loop over the files in parallel, counting words
var startTime = Date.now();
ems.parForEach(0, file_list.length, function (fileNum) {
    try {
        var text = fs.readFileSync(doc_path + file_list[fileNum], 'utf8', "r");
        var words = text.replace(/[\n\r]/g, ' ').toLowerCase().split(splitPattern);
        //  Iterate over all the wods in the document
        words.forEach(function (word) {
            //  Ignore words over 15 characters long as non-English
            if (word.length < 15 && word.length > 0) {
                //  Atomically increment the count of times this word was seen
                var count = wordCounts.faa(word, 1);
            }
        });

        //  Accumulate some statistics: word and data counters, print progress
        var nWords_read = stats.faa('nWords', words.length);
        var nBytes_read = stats.faa('nBytesRead', text.length);

        var now = Date.now();
        if (ems.myID === 0  &&  fileNum % 10 === 0) {
            console.log("Average Words/sec=" + Math.floor((1000 * nWords_read) / (now - startTime)) +
                "   MBytes/sec=" + Math.floor((100000 * nBytes_read) / ((now - startTime) * (1 << 20)))/100);
        }
    }
    catch (Err) {
        ems.diag("This is not a text file:" + file_list[fileNum]);
    }
});



// ===============================================================================
//  Output the Most Frequently Occurring Words
//
//  First perform many sequential insertion sorts in parallel,
//  then serially perform a merge sort of all the partial lists.
//
//  Start by printing the total amount of data processed during word counting
ems.master(function() {
    console.log("Totals: ", stats.read('nWords'), " words parsed,  ",
        stats.read('nBytesRead'), "bytes read.");
});

//  Divide the array across all the processes, each process keeps track
//  of the "local_sort_len" most frequent word it encounters.
var local_sort_len = Math.max(10, process.argv[2]);
var biggest_counts = new Array(local_sort_len).fill({"key": 0, "count": 0});
ems.parForEach(0, maxNKeys, function (keyN) {
    var key = wordCounts.index2key(keyN);
    if (key) {
        //  Perform an insertion sort of the new key into the biggest_counts
        //  list, deleting the last (smallest) element to preserve length.
        var keyCount = wordCounts.read(key);
        var idx = local_sort_len - 1;
        while (idx >= 0  &&  biggest_counts[idx].count < keyCount) {
            idx -= 1;
        }
        var newBiggest = {"key": key, "count": keyCount};
        if (idx < 0) {
            biggest_counts = [newBiggest].concat(biggest_counts.slice(0, biggest_counts.length - 1));
        } else if (idx >= local_sort_len) {
            // Not on the list
        } else {
            var left = biggest_counts.slice(0, idx + 1);
            var right = biggest_counts.slice(idx + 1);
            biggest_counts = left.concat([newBiggest].concat(right)).slice(0, -1);
        }
    }
});

//  Concatenate all the partial (one per process) lists into one list
stats.writeXF('most_frequent', []);
ems.barrier();
stats.writeEF('most_frequent', stats.readFE('most_frequent').concat(biggest_counts));
ems.barrier();


//  Sort & print the list of words, only one process is needed
ems.master(function() {
    biggest_counts = stats.readFF('most_frequent');
    //  Sort the list by word frequency
    biggest_counts.sort(function (a, b) {
        return b.count - a.count;
    });

    //  Print only the first "local_sort_len" items -- assume the worst case
    //  of all the largest counts are discovered by a single process, the next
    //  largest after "local_sort_len" is no longer on any list.
    console.log("Most frequently appearing terms:");
    for (var index = 0;  index < local_sort_len;  index += 1) {
        console.log(index + ': ' + biggest_counts[index].key + "   " + biggest_counts[index].count);
    }
});
