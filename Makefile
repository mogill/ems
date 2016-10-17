all: help_notice py3 node tests

help:
	@echo "         Extended Memory Semantics  --  Build Targets"
	@echo "==========================================================="
	@echo ""
	@echo "    all             Build all targets, run all tests"
	@echo "    node            Build only Node.js"
	@echo "    py3             Build only Python3"
	@echo "    test            Run both Node.js and Py3 tests"
	@echo "    test[_js|_py3]  Run only Node.js, or only Py3 tests, respectively"
	@echo ""
	@echo "    clean           Remove all files that can be regenerated"
	@echo "    clean[_js|_py3] Remove Node.js or Py3 files that can be regenerated, respectively"

help_notice:
	@echo "=== \"make help\" for list of targets"

test: test_js test_py3

test_js: node
	npm test

test_py3: py3
	(cd Tests; ./py_api.py)

node: build/Release/ems.node

build/Release/ems.node:
	node-gyp rebuild

py3:
	(cd Python; ./setup.py build --build-temp=./ install )

clean: clean_js clean_py3

clean_js:
	$(RM) -rf build

clean_py3:
	$(RM) -rf Python/build
