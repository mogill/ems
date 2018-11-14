
# enable extended glob patterns
shopt -s extglob

__DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NODE_GYP_BIN="$__DIR"/node_modules/.bin/node-gyp


function test_js () {
	exe="$1"
	echo "exe $exe"
	nthreads=8
	cd Tests
	rm -f EMSthreadStub.js
	for test in `ls *.js */*.js`; do
		$exe $test $nthreads && \
		err=$? && \
		echo $test \": ERROR=\" $err && \
		[ $err -ne 0 ] && exit 1 || echo "" \

	done

}

function test_node () {
	test_js node
}

function test_electron () {
	eval $(electron_pkg_env)
	# only run electron tests if electron module in scope
	[ ! -f "$electron_path" ] && exit 1
	export ELECTRON_RUN_AS_NODE=1
	test_js "$electron_path";
}

function gyp_rebuild_node () {
	mkdir -p dist/nodejs
	node-gyp rebuild --build-dir=dist/nodejs
	mv -f build/* dist/nodejs
	rm -rf build
}


function gyp_rebuild_electron_if_needed () {

	# If electron installed in npm context, derive node-gyp config exports
	electron_path="" && eval $(electron_pkg_env)
	[ ! -f "$electron_path" ] && \
		echo "☞ No Electron to build" && \
		exit 1

	# Architecture of Electron, ia32 or x64.
	export npm_config_arch=x64
	export npm_config_target_arch=x64

	# Download headers for Electron.
	export npm_config_disturl=https://atom.io/download/electron

	# Tell node-pre-gyp that we are building for Electron.
	export npm_config_runtime=electron

	# Tell node-pre-gyp to build module from source code.
	export npm_config_build_from_source=true

	# Electron's version. which can be found at node_modules/electron/package.json my version is 1.7.9
	export npm_config_target=$electron_version

	# For electron forks or nightlies, to find headers for Electron's node, add Make.sh with:
	#       export node_headers_dir=PATH_TO_ELECTRON_SRC/src/out/Release/gen/node_headers
	[ -f "$electron_path"/Make.sh ] && \
			source "$electron_path"/Make.sh

	# Explicitly redudance var exports & command-line flags due to flaky node-gyp surface area
	cl_flags=""
	[ -z "$node_headers_dir" ] && \
		export npm_config_nodedir="$node_headers_dir"
		cl_flags="--nodedir=$npm_config_nodedir $cl_flags"
	cl_flags=" --build-dir=dist/electron --target=$npm_config_target  --runtime=$npm_config_runtime --build_from_source=$npm_config_build_from_source --arch=$npm_config_arch --target_arch=$npm_config_target_arch --disturl=$npm_config_disturl $cl_flags"

	mkdir -p dist/electron

	# Install all dependencies with isolated cache
	# HOME=~/.electron-gyp-"$electron_version" npm install

	$NODE_GYP_BIN rebuild $cl_flags

	mv -f build/* dist/electron
	rm -rf build
}

function electron_pkg_env () {
	node -e "$(cat << \
_______________________________________________________________________________
		const out = (chunk) => process.stdout.write(chunk+'\n');
		const err = (chunk) => process.stderr.write(chunk);
		const q = (str) => '\"' + str + '\"'
		try {
			const path = require('electron')
			const {version} = require('electron/package.json');
			out( 'export electron_path=' + q(path) );
			out( 'export electron_version=' + q(version) );
		} catch(e) {
			err(e.toString())
		}
_______________________________________________________________________________
	)"
}

