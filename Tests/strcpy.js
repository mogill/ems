'use strict';
var ems = require('ems')(parseInt(process.argv[2]));
var assert = require('assert');
var fs = require('fs');
var maxlen = 10000000;
var stats = ems.new({
    filename: '/tmp/EMS_strlen',
    dimensions: [10],
    heapSize: (2 * maxlen) + 1 + 10,  // 1 = Null,  10 = length of key
    useMap: true,          // Use a key-index mapping, not integer indexes
    setFEtags: 'full',     // Initial full/empty state of array elements
    doDataFill: true,      // Initialize data values
    dataFill: 0            // Initial value of new keys
});

if (ems.myID === 0) { stats.writeXE('test_str', 123); }
ems.barrier();

for (var len=2;  len < maxlen;  len = Math.floor(len * 1.5) ) {
    if (ems.myID === 0) { console.log("Len = " + len); }
    var str = "";
    for (var idx = 0;  idx < len;  idx += 1) { str += "x"; }
    stats.writeEF('test_str', str);
    var readback = stats.readFE('test_str');
    assert(readback === str, 'Mismatched string.  Expected len ' + str.length + ' got ' + readback.length);
    ems.barrier();
}
