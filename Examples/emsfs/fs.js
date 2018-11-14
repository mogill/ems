
const {
    EMSFS_INIT,
    EMSFS_FILE,
    EMSFS_SIZE,
    EMSFS_DIMENSIONS,
    EMSFS_N_THREADS,
    // EMSFS_PIN_THREADS,
} = process.env

const fs = require('fs')
const ems = require('../..')(EMSFS_N_THREADS)
const path = require('path')
let asar = function (){
    let asar_module = require('asar')
    fs.readFileSync = readFileSyncEnhanced
    asar = function() {
        return asar_module
    }
    return asar_module
}

const shared = ems.new({
    useExisting: EMSFS_INIT !== 'true',
    filename: EMSFS_FILE,
    heapSize: EMSFS_SIZE|0,
    dimensions: EMSFS_DIMENSIONS|0,
    useMap: true,
})

fs.shared = shared

shared.write('buf', Buffer.from('xxxxx'))

// ensure browserWindows do not clear shared mem file
process.env.EMSFS_INIT = 'false'

// patch
const readFileSync = fs.readFileSync
const read_opts = {
    encoding: null,
    flag: 'r'
}
const _get_read_opts_by_type = {
    'undefined'(opts) {
        read_opts.encoding = null
        read_opts.flag = 'r'
        return read_opts
    },
    'string'(opts) {
        read_opts.encoding = opts
        read_opts.flag = 'r'
        return read_opts
    },
    'object'(opts) {
        opts.encoding || (opts.encoding = null)
        opts.flags || (opts.flag = 'r')
        return opts
    }
}
fs.readFileSync = readFileSyncEnhanced

function readFileSyncEnhanced(file, _opts) {
    const opts = _get_read_opts_by_type[typeof _opts, _opts]
    return _is_mem_file(file)
        ? readFileMem(file, opts)
        : _read_file_sync(file, opts)
}

const _mem_file_exts = {
    '.json':true, '.js':true, '.css':true, '.html':true
}
function _is_mem_file(file) {
    const ext = path.extname(file)
    return _mem_file_exts[ext]
}

function _read_file_sync(file, opts) {
    const asar_index = file.indexOf('.asar/')
    if (asar_index >>> 31) {
        return readFileSync(file, opts)
    }
    const data =  asar().extractFile(
        file.substr(0, asar_index) + '.asar',
        file.substr(asar_index+6)
    )
    return data
}

// install
fs.readFileMem = readFileMem
function readFileMem(file, opts) {
    return shared.read(file) || (
        writeFileMem(file, _read_file_sync(file, opts))
    )
}

// patch
const default_write_string_opts = {
    encoding: 'utf8',
    mode: 0o666,
    flag: 'w'
}
const writeFileSync = fs.writeFileSync
fs.writeFileSync = function(file, data, opts) {
    return typeof data === 'string'
        ? writeFileMem(file, data, opts || default_write_string_opts)
        : writeFileSync(file, data, opts)
}

// install
fs.writeFileMem = writeFileMem
function writeFileMem(file, data, opts) {
    const string = (data.byteLength !== undefined)
        ? data.toString('utf8')
        : data
    shared.write(file, string)
    return string
}

module.exports = fs