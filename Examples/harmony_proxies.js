/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2016, Jace A Mogill.  All rights reserved.                   |
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
"use strict";
var ems = require("ems")(1, false);
var assert = require("assert");

// The Proxy object is defined by Reflect
var Reflect = require("harmony-reflect");

// Setter/Getter methods for the proxy object
// If the target is built into EMS use the built-in object,
// otherwise read/write the EMS value without honoring the Full/Empty tag
var proxyHandler = {
    get: function(target, name) {
        if (name in target) {
            return target[name];
        } else {
            return target.read(name);
        }
    },
    set: function(target, name, value) {
        target.write(name, value);
    }
};

// Create a temporary EMS space to demonstrate read/write operations
var emsData = ems.new({
    dimensions: 1000,
    heapSize: 2000,
    useMap: true,
    useExisting: false
});
// Wrao the object with a proxy
emsData = new Proxy(emsData, proxyHandler);

// The object is now accessible as an object.
emsData["foo"] = 123;
assert(emsData["foo"] === 123);
assert(emsData.foo === 123);

emsData.bar = 321;
assert(emsData["bar"] === 321);
assert(emsData.bar === 321);

assert(emsData.readFE("bar") === 321);
assert(emsData.bar === 321);
emsData.writeEF("bar", "last write");
assert(emsData.readFE("bar") === "last write");
assert(emsData.bar === "last write");

console.log("All operations passed.");
