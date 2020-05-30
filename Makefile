#make
# *-----------------------------------------------------------------------------+
# |  Extended Memory Semantics (EMS)                            Version 1.6.0   |
# |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
# +-----------------------------------------------------------------------------+
# |  Copyright (c) 2016, Jace A Mogill.  All rights reserved.                   |
# |                                                                             |
# | Redistribution and use in source and binary forms, with or without          |
# | modification, are permitted provided that the following conditions are met: |
# |    * Redistributions of source code must retain the above copyright         |
# |      notice, this list of conditions and the following disclaimer.          |
# |    * Redistributions in binary form must reproduce the above copyright      |
# |      notice, this list of conditions and the following disclaimer in the    |
# |      documentation and/or other materials provided with the distribution.   |
# |    * Neither the name of the Synthetic Semantics nor the names of its       |
# |      contributors may be used to endorse or promote products derived        |
# |      from this software without specific prior written permission.          |
# |                                                                             |
# |    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS      |
# |    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT        |
# |    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR    |
# |    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SYNTHETIC         |
# |    SEMANTICS LLC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,   |
# |    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,      |
# |    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR       |
# |    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF   |
# |    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     |
# |    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS       |
# |    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.             |
# |                                                                             |
# +-----------------------------------------------------------------------------*
all: py3 node tests help_notice 

help:
	@echo "         Extended Memory Semantics  --  Build Targets"
	@echo "==========================================================="
	@echo "    all                       Build all targets, run all tests"
	@echo "    node                      Build only Node.js"
	@echo "    py                        Build both Python 2 and 3"
	@echo " "
	@echo "    py[2|3]                   Build only Python2 or 3"
	@echo "    test                      Run both Node.js and Py tests"
	@echo "    test[_js|_py|_py2|_py3]   Run only Node.js, or only Py tests, respectively"
	@echo "    clean                     Remove all files that can be regenerated"
	@echo "    clean[_js|_py|_py2|_py3]  Remove Node.js or Py files that can be regenerated"

help_notice:
	@echo "=== \"make help\" for list of targets"

tests: test_js test_py


test_js: node
	npm test

test_py: test_py2 test_py3

test_py3: py3
	(cd Tests; python3 ./py_api.py)

test_py2: py2
	(cd Tests; python ./py_api.py)

node: build/Release/ems.node

build/Release/ems.node:
	npm install
	(cd node_modules;  /bin/rm -f ems; ln -s ../ ./ems)

py: py2 py3

py3:
	(cd Python; sudo rm -rf Python/build Python/ems.egg-info Python/dist; sudo python3 ./setup.py build --build-temp=./ install)

py2:
	(cd Python; sudo rm -rf Python/build Python/ems.egg-info Python/dist; sudo python ./setup.py build --build-temp=./ install)

clean: clean_js clean_py3 clean_py2

clean_js:
	$(RM) -rf build

clean_py3:
	$(RM) -rf Python/build Python/py3ems/build /usr/local/lib/python*/dist-packages/*ems* ~/Library/Python/*/lib/python/site-packages/*ems* ~/Library/Python/*/lib/python/site-packages/__pycache__/*ems* /Library/Frameworks/Python.framework/Versions/*/lib/python*/site-packages/*ems*

clean_py2:
	$(RM) -rf Python/build Python/py2ems/build /usr/local/lib/python*/dist-packages/*ems* ~/Library/Python/*/lib/python/site-packages/*ems* ~/Library/Python/*/lib/python/site-packages/__pycache__/*ems* /Library/Python/*/site-packages/*ems*
