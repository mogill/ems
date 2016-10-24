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
from distutils.core import setup, Extension
import sys

# Path to C library source code
src_path = '../src/'

# OS Specific link flags
link_args = []
if sys.platform == "linux" or sys.platform == "linux2":
    link_args.append("-lrt")
elif sys.platform == "darwin":
    pass
else:
    pass

setup(
    name="ems",
    version="1.4.0",
    py_modules=["ems"],
    setup_requires=["cffi>=1.0.0"],
    install_requires=["cffi>=1.0.0"],

    # Author details
    author='Jace A Mogill',
    author_email='mogill@synsem.com',

    description='Extended Memory Semantics (EMS) for Python',
    license='BSD',

    # The project's main homepage.
    url='https://github.com/SyntheticSemantics/ems',

    ext_modules=[Extension('libems.so',
                           [src_path + filename for filename in
                               ['collectives.cc', 'ems.cc', 'ems_alloc.cc', 'loops.cc', 'primitives.cc', 'rmw.cc']],
                           extra_link_args=link_args
                           )],
    long_description='Persistent Shared Memory and Parallel Programming Model',
    keywords=["non volatile memory",
              "NVM",
              "NVMe",
              "multithreading",
              "multithreaded",
              "parallel",
              "parallelism",
              "concurrency",
              "shared memory",
              "multicore",
              "manycore",
              "transactional memory",
              "TM",
              "persistent memory",
              "pmem",
              "Extended Memory Semantics",
              "EMS"]
)
