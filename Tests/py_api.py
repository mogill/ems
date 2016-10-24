# -*- coding: utf-8 -*-
"""
 +-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.1   |
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
 +-----------------------------------------------------------------------------+
"""
import sys
import os
import time
sys.path.append("../Python/ems/")  # Path to EMS Python Module
import ems

nprocs = 12
nelem = 1000
ems.initialize(nprocs, False, 'bsp', '/tmp/pyems_foo_test.ems')
time.sleep(ems.myID/5)
assert ems.barrier()


various_consts = ["簣ひょ갤妣ゃキェぱ覣.𐤦ぴ盥", -99999, 1 << 35, 9821.3456, "bar", False]
unmapped_fname = '/tmp/py_unmapped.ems'
#  TODO: 3D array
unmapped = ems.new(nelem * nprocs, nprocs * nelem * 100, unmapped_fname)
ems.diag("=============== INIT COMPLETE ======================")


def sum(left, right):
    if type(left) == int:
        return right + left
    if type(left) == float:
        return right * left
    else:
        return str(right) + str(left)


def target(idx):
    return idx + (ems.myID * nelem)


for const in various_consts:
    for idx in range(nelem):
        unmapped.writeXF(target(idx), sum(const, idx))

    for idx in range(nelem):
        val = unmapped.readFE(target(idx))
        if sum(const, idx) != val:
            ems.diag("I'm lost... sum=%s   val=%s   idx=%d  target=%d" % (sum(const, idx), val, idx, target(idx)))
        assert sum(const, idx) == val
ems.barrier()
ems.diag("Starting mapped tests")
arrLen = 1000
mapped_fname = '/tmp/py_mapped.ems'
mapped = ems.new({
    'dimensions': [arrLen],
    'heapSize': arrLen * 200,
    'useMap': True,
    'useExisting': False,
    'filename': mapped_fname,
    'doSetFEtags': True,
    'setFEtags': 'empty',
    'dataFill': None
})

arrayElem = ['abcd', True, 1234.567, False, {'x': 'xxx', 'y': 'yyyyy'}, 987, None, [10, 11, 12, 13]]
objElem = {'a': 1, 'b': 321.653, 'x': {'nesta': False, 'nestb': None}, 'c': 'asdasd'}

teststr = 'magic!!string?!'

mapped.writeEF(teststr, ems.myID)
assert ems.myID == mapped.readFE(teststr)
# ems.diag("First mapped write passed")

mapped.writeEF(teststr + "XXX", ems.myID * 100)
assert ems.myID * 100 == mapped.readFE(teststr + "XXX")
# ems.diag("Second mapped write passed")

mapped.writeEF(ems.myID, teststr)
# ems.diag("Wrote the test string")
assert teststr == mapped.readFE(ems.myID)

# Check read/writing JSON
mapped.writeEF(ems.myID, objElem)
tmp = mapped.readFE(ems.myID)
# ems.diag("ObjElem READback" + str(tmp))
assert objElem == tmp

mapped.writeEF(ems.myID, arrayElem)
tmp = mapped.readFE(ems.myID)
# ems.diag("READback:" + str(mapped.readFE(ems.myID)))
assert arrayElem == tmp
tmp = mapped.read(ems.myID)
assert arrayElem == tmp

mapped.write(ems.myID, 'dummy')
assert 'dummy' == mapped.read(ems.myID)

# ==========================================================================
def parfun(idx):
    i = mapped.faa('index sum', idx - start + 1)
    c = mapped.faa('counter', 1)
    timeout = 10000
    while (timeout > 0) and mapped.cas('cas test', None, 'something') is not None:
        timeout -= 1
    assert timeout > 0
    assert 'something' == mapped.cas('cas test', 'something', None)
    # ems.diag("PARFUN:  idx(%d)  i(%d)   c(%d)" % (idx, i, c))


def triangular(max = 10):
    for top in range(1, max):
        x = 0
        for idx in range(1, top+1):
            x += idx
        print("top=", top, "  x=", x, "  calc=", (top * (top + 1)) / 2)
        assert x == (top * (top + 1)) / 2


def check_tri(msg):
    index_sum = mapped.readFF('index sum')
    count = mapped.readFF('counter')
    # ems.diag(msg + ": sum=%d  expected=%d" % (index_sum, (niters * (niters + 1))/2))
    assert index_sum == trisum
    assert count == niters
    reset_tri()


def reset_tri():
    ems.barrier()
    mapped.writeXF('index sum', 0)
    mapped.writeXF('counter', 0)
    mapped.writeXF('cas test', None)
    ems.barrier()

unmapped.writeXF(ems.myID, 1000+ems.myID)
ems.single(triangular)
idx = (ems.myID+1) % nprocs
assert unmapped.readFF(idx) == 1000+idx

# Scheduling needs proper testing other than visual inspection
start = 100
end = 120
niters = end - start
trisum = (niters * (niters + 1))/2

reset_tri()
ems.parForEach(start, end, parfun)
check_tri("default")
ems.parForEach(start, end, parfun, 'static')
check_tri("static")
ems.parForEach(start, end, parfun, 'guided')
check_tri("guided")
ems.parForEach(start, end, parfun, 'dynamic')
check_tri("dynamic")

# ==========================================================================
unmapped.writeXF(0, True)
unmapped.writeXF(1, 0)
mapped.writeXF('one', 0)
ems.barrier()
tm_data = [[unmapped, 1], [mapped, 'one'], [unmapped, 0, True]]
for idx in range(niters):
    transaction = ems.tmStart(tm_data)
    unmapped_tmp = unmapped.read(1)
    mapped_tmp = mapped.read('one')
    assert unmapped.read(0)
    unmapped.write(1, unmapped_tmp + 1)
    mapped.write('one', mapped_tmp + 1)
    ems.tmEnd(transaction, True)

ems.barrier()
assert unmapped.readFF(1) == nprocs * niters
assert mapped.readFF('one') == nprocs * niters
ems.barrier()


# ==========================================================================


def plus_one():
    tmp = unmapped.read(0)
    unmapped.write(0, tmp + 1)

ems.barrier()
unmapped.writeXF(0, 0)
ems.barrier()
ems.critical(plus_one)
readback = unmapped.readFF(0)
# ems.diag("Initial Readback before critical: %d" % readback)
if readback == nprocs:
    unmapped.writeXF(0, -1234)
ems.barrier()
readback = unmapped.readFF(0)
# ems.diag("Readback of critical: %d" % readback)
assert readback == -1234


# ==========================================================================
def check_master():
    assert ems.myID == 0

ems.master(check_master)


# ==========================================================================
def check_single():
    global idx
    oldval = mapped.cas(idx, None, 'Wheee!!!')
    if oldval is None:
        assert mapped.faa('count', 1) == 0

idx = 987.654
ems.barrier()
mapped.writeXF(idx, None)
mapped.writeXF('count', 0)
ems.barrier()
ems.single(check_single)
assert mapped.readFF(idx) == 'Wheee!!!'
assert mapped.readFF('count') == 1


# ==========================================================================
keys = [mapped.index2key(idx) for idx in range(arrLen)]
# print("KEYS!", keys)
assert 'count' in keys
assert 'counter' in keys
assert teststr in keys
assert teststr+'XXX' in keys


# ==========================================================================
stack = ems.new({
    'dimensions': [arrLen],
    'heapSize': arrLen * 100,
    'useExisting': False,
    'filename': '/tmp/stack_test.ems',
    'doSetFEtags': True,
    'setFEtags': 'empty',
    'dataFill': None
})

for elem in arrayElem:
    assert stack.push(elem) >= 0

for elem in arrayElem:
    assert stack.pop() in arrayElem

ems.barrier()
assert stack.pop() is None
ems.barrier()


for elem in arrayElem:
    assert stack.enqueue(elem) >= 0

for elem in arrayElem:
    assert stack.dequeue() in arrayElem

ems.barrier()
assert stack.dequeue() is None
ems.barrier()

# ==========================================================================
# Fancy array syntax
mapped.writeXF(-1234, 'zero')
mapped[-1234].writeXF(0)
idx = 'fancy' + str(ems.myID)
val = 123 + ems.myID
rw_val = 12345 + ems.myID
rw_idx = 'rw test' + str(ems.myID)
mapped[rw_idx].writeXF(rw_val)
mapped[idx].writeXF(val)
# print("Fancyread", ems.myID, mapped[idx].read(), mapped[idx].xread)
assert val == mapped[idx].read()
assert val == mapped[idx].readFF()
assert val == mapped[idx].readFE()
val *= 200
mapped[idx].writeEF(val)
assert val == mapped[idx].readFF()
ems.barrier()
assert rw_val == mapped[rw_idx].readRW()
assert mapped[rw_idx].releaseRW() >= 0
ems.barrier()
val *= 200
mapped[idx].write(val)
assert val == mapped[idx].read()
assert 0 <= mapped[-1234].faa(1) < nprocs
if ems.myID == 0:
    mapped[-1234].cas(nprocs-1, True)
assert mapped[-1234].read()

mapped[idx].writeXE(123)
mapped[idx].setTag('full')

if ems.myID == 0:
    mapped.writeXF('syntax_test', 123)
    assert mapped.readFE('syntax_test') == 123
    assert mapped['syntax_test'].read() == 123
    assert mapped.syntax_test == 123

    mapped['syntax_test2'] = 234
    mapped['syntax_test2'].setTag('full')
    # assert mapped.readFE('syntax_test2') == 234
    mapped['syntax_test3'].writeXF(234)
    assert mapped.readFE('syntax_test2') == 234

ems.barrier()


'''
assert mapped.fancy0 == 123
assert unmapped[0] == -123

ems.barrier()
if ems.myID == 0:
    mapped.fizz = 'asd'
ems.barrier()
assert mapped.fizz == 'asd'
'''


# ==========================================================================

unmapped.destroy(True)
# ems.diag("UNMAPPING:" + unmapped_fname + "  Exist?" + os.path.exists(unmapped_fname))
assert not os.path.exists(unmapped_fname)
mapped.destroy(False)
assert os.path.exists(mapped_fname)

mapped = ems.new({
    'dimensions': [arrLen],
    'heapSize': arrLen * 200,
    'useMap': True,
    'useExisting': True,
    'filename': mapped_fname,
    'doSetFEtags': False,
    'doDataFill': False
})
assert mapped.readFF(idx) == 123
