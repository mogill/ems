
process.once('loaded', () => {
    const fs = require('fs')
    const Module = require('module')

    Module._extensions['.css'] = function(module, filename) {
        var content = fs.readFileSync(filename, 'utf8');
        const style = document.createElement('style')
        style.innerHTML = content
        document.head.append(style)
    };
})
