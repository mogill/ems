/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.8   |
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

module.exports = {
    //-------------------------------------------------------------------
    //  Timer functions
    timerStart : function(){ return new Date().getTime() },
    timerStop : function(timer, nOps, label, myID) {
	var now = new Date().getTime()
        var opsPerSec = (nOps*1000000) / ((now - timer) *1000)
	if(typeof myID === undefined  ||  myID === 0) {
            console.log(fmtNumber(nOps) + label + fmtNumber(Math.floor(opsPerSec).toString()) + " ops/sec")
	}
    },

    //---------------------------------------------------------------------------
    //  Generate a random integer within a range (inclusive) from 'low' to 'high'
    randomInRange : function(low, high) {
	return( Math.floor((Math.random() * (high - low)) + low) ) 
    }
}



function fmtNumber(n) {
    var s = '                       ' + n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",")
    if(n < 1) return n
    else    { return s.substr(s.length - 15, s.length)  }
}



//---------------------------------------------------------------------------
//  Return an array with only unique elements
Array.prototype.unique = function () {
    return this.filter( function (value, index, self) { 
	return self.indexOf(value) === index;
    } )
}

//---------------------------------------------------------------------------
//  Randomize the order of data in an array
Array.prototype.shuffle = function() {
    for(var j, x, i = this.length; i; j = Math.floor(Math.random() * i), x = this[--i], this[i] = this[j], this[j] = x);
    return this;
}

