/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.0   |
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
var ems = require('ems')(parseInt(process.argv[2]), false)

var arrLen = 40000
var a = ems.new(arrLen, arrLen * 40)
var map = ems.new( {
    dimensions : [ arrLen ],
    heapSize  : arrLen * 5,
    useMap: true, 
    useExisting: false,
    setFEtags : 'full',
    dataFill : 0
} )


var data = [false, true, 1234, 987.654321, 'hello', undefined]

for(var old=0;  old < data.length;  old++ ) {
    for(var newIdx=0;  newIdx < data.length;  newIdx++ ) {
	a.writeXF(ems.myID, data[old])
	var js = data[old]
	js += data[newIdx]
	var oldVal = a.faa(ems.myID, data[newIdx])
	var newVal = a.readFF(ems.myID)

	if( !((newVal === js)  ||  (isNaN(oldVal)  &&  isNaN(newVal) )  ||
	      (isNaN(newVal)  &&   data[newIdx] === undefined) ) )
	    ems.diag('FAA: old=' + data[old] + '   new=' + data[newIdx] + 
		     '  oldVal='+oldVal+'/'+typeof oldVal + '   newVal=' + 
		     newVal + '/'+typeof newVal+ '  js='+js + '/'+typeof js)
    }
}
ems.barrier()
var id = (ems.myID + 1) % ems.nNodes;
for(var memIdx=0;  memIdx < data.length;  memIdx++ ) {
    for(var oldIdx=0;  oldIdx < data.length;  oldIdx++ ) {
	for(var newIdx=0;  newIdx < data.length;  newIdx++ ) {
	    a.writeXF(id, data[memIdx])
	    var oldVal = a.cas(id, data[oldIdx], data[newIdx])
	    var newVal = a.readFF(id)

	    var js = data[memIdx]
	    if(js === data[oldIdx]) { js = data[newIdx] }

	    if(js !== newVal) {
		ems.diag('CAS: mem=' + newVal + '  dataold='+ data[oldIdx] + '  datanew='+data[newIdx] +'   old=' + oldVal + 
			 '  readback='+ newVal + '   js='+js)
	    }
	}
    }
}

ems.parForEach(0, arrLen, function(idx) {
    a.writeXF(idx, undefined)
    a.faa(idx, undefined) 
    a.faa(idx, 'bye byte') 
    a.faa(idx, ems.myID) 
    a.faa(idx, 0.1) 
    a.faa(idx, false) 
    if( a.readFF(idx) != 'nanbye byte' + ems.myID + '0.100000false' ) {
	ems.diag('Failed match ='+ a.read(idx))
    }
} )


//-----------------------------------------------------------

ems.parForEach(0, arrLen, function(idx) {
    a.writeXF(idx, 0)
} )

var nTimes = 1500
ems.parForEach(0, arrLen, function(previdx) {
    for(var i = 0;  i < nTimes;  i++) {
	a.faa((previdx + i) % arrLen, 1)
    }
} )

ems.parForEach(0, arrLen, function(idx) {
    a.faa(idx, 0.1)
    a.faa(idx, 'fun!')
    a.faa(idx, false) 
    if( a.readFE(idx) != nTimes + '.100000fun!false' ) {
	ems.diag('Failed match ='+ a.read(idx) + '   idx='+idx)
    }
} )

