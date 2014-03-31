/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.7   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
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
//  Usage:   node wordCount.js <number of threads>
//    Executes in bulk synchronous parallel mode
var ems = require('ems')(parseInt(process.argv[2]))
var fs  = require('fs')

//-------------------------------------------------------------------
//  Timer functions
function timerStart(){ return new Date().getTime() }
function timerStop(timer, nOps, label, myID) {
    function fmtNumber(n) {
	var s = '                       ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
	if(n < 1) return n
	else    { return s.substr(s.length - 15, s.length)  }
    }
    var now = new Date().getTime()
    var opsPerSec = (nOps*1000000) / ((now - timer) *1000)
    if(typeof myID === undefined  ||  myID === 0) {
        console.log(fmtNumber(nOps) + label + fmtNumber(Math.floor(opsPerSec).toString()) + " ops/sec")
    }
}

//-------------------------------------------------------------------
//  Allocate the word count dictionary
var maxNKeys    = 10000000
var wordCounts   = ems.new( {
    dimensions : [ maxNKeys ],  // Maximum # of different keys the array can store
    heapSize   : maxNKeys * 10, // 10 bytes of storage per key, used for key (dictionary word) itself
    useMap     : true,          // Use a key-index mapping, not integer indexes
    setFEtags  : 'full',        // Initial full/empty state of array elements
    dataFill   : 0              // Initial value of new keys
} )


//-------------------------------------------------------------------
//  Use a key-value mapped array to store execution statistics
var nStats      = 2
var stats       = ems.new( {
    dimensions : [ nStats ],    // Maximum number of stats that can be stored
    heapSize   : nStats*20,     // 20 bytes per entry for key and data values
    useMap     : true,          // Use a key-index mapping, not integer indexes
    setFEtags  : 'full',        // Initial full/empty state of array elements
    dataFill   : 0              // Initial value of new keys
} )



//-------------------------------------------------------------------
//  Program main entry point 
var dir          = fs.readdirSync('/Users/mogill/Src/Data/Gutenberg/all/');
var splitPattern = new RegExp(/[ \n,\.\\/_\-\<\>:\;\!\@\#\$\%\&\*\(\)=\[\]|\"\'\{\}\?\—]/)


//-------------------------------------------------------------------
//  Loop over the files in parallel, counting words
var totalTime    = timerStart()
ems.parForEach(0, 200,  function(bufNum) {  // First 200 docs, not entire collection
    var fileTimer = timerStart() 
    var text = fs.readFileSync('/Users/mogill/Src/Data/Gutenberg/all/' + dir[bufNum], 'utf8', "r")
    var words = text.replace(/[\n\r]/g,' ').toLowerCase().split(splitPattern)
    words.forEach( function(word, wordN) {
	if(word.length < 15  &&  word != "") {
	    wordCounts.faa(word, 1)
	}
    } )
    //  Accumulate word and data counters, print progress
    stats.faa('nWords', words.length)
    stats.faa('nBytesRead', text.length)
    //  Note:  This diagnostic only prints on node 0, so not all iterations produce output
    timerStop(fileTimer, words.length, " words in file " + dir[bufNum] + " processed at ", ems.myID)
} )


timerStop(totalTime, stats.read('nWords'), " Words parsed ", ems.myID)
timerStop(totalTime, stats.read('nBytesRead'), " bytes read   ", ems.myID)
