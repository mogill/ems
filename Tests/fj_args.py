#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
 +-----------------------------------------------------------------------------+
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
 +-----------------------------------------------------------------------------+
"""
import sys
import time
sys.path.append("../Python/ems/")

nprocs = 2
nelem = 1000
global_str = "The Global String"   # Hardcoded later

import ems
ems.initialize(nprocs, True, 'fj', '/tmp/fj_main.ems')


def fj_test(a, b, c, taskN=None):
    global global_str, nprocs, ems
    global_str = "The new globstrrrrrr"
    # import ems
    # ems.initialize(nprocs, True, 'fj', '/tmp/fj_main.ems')
    ems.diag("FJ0")
    assert taskN == None
    ems.diag("FJ1")
    ems.barrier()
    ems.diag("FJ2")
    # assert(typeof local_str === "undefined", "The local string did not stay local");
    # ems.diag("global_str=" + global_str + " a =" + a + "  b=" + b + "  c=" + c)
    ems.diag("global_str=" + global_str + " a =" + a + "  b=" + b + "  c=" + c)
    ems.diag("FJ3")
    assert a == "The Global String"   # Hardcoded due to no closures
    assert b == 'two'
    assert c == 'three'
    ems.diag("FJ4")
    # global_str += "Updated by process " + str(ems.myID)

ems.diag("Entering first parallel region")
ems.parallel(fj_test, global_str, 'two', 'three', 'taaaaaasdasdasda')
ems.diag("globstr=" + global_str)

ems.diag("This side")
time.sleep(ems.myID/2)
ems.parallel(ems.barrier)
ems.diag("That side")
