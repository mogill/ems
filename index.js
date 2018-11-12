
process.env.EMS_MODULE_FILE = __filename

// EMS_BINDINGS_FILE defaults to electron's build if in electron
process.env.EMS_BINDINGS_FILE || (
    process.env.EMS_BINDINGS_FILE = is_electron()
        ? '../dist/electron/Release/ems.node'
        : '../dist/nodejs/Release/ems.node'
)

module.exports = require('./nodejs/ems.js')

// https://github.com/electron/electron/issues/2288
function is_electron() {

	// Electron as node
    if (process.env.ELECTRON_RUN_AS_NODE === '1') {
        return true
    }

    // Renderer process
    if (typeof window !== 'undefined' && typeof window.process === 'object' && window.process.type === 'renderer') {
        return true;
    }

    // Main process
    if (typeof process !== 'undefined' && typeof process.versions === 'object' && !!process.versions.electron) {
        return true;
    }

    // Detect the user agent when the `nodeIntegration` option is set to true
    if (typeof navigator === 'object' && typeof navigator.userAgent === 'string' && navigator.userAgent.indexOf('Electron') >= 0) {
        return true;
    }

    return false;
}
