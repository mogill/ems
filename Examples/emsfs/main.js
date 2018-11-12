require('./fs.js');
const path = require('path');
const {app, session, BrowserWindow} = require('electron');

(async function() {
    await app.whenReady()

    session.defaultSession.setPreloads([
        path.join(__dirname, 'app/preload.js')
    ])

    const win_emsfs = bench_win({
        x: 250, y:150,
        title: 'emsfs',
        webPreferences:{
            preload: path.join(__dirname, 'fs.js')
        }
    })

    const win_fs = bench_win({
        x: 500, y: 400,
        title: 'fs',
        webPreferences:{}
    })

    function bench_win(opts) {
        opts.vibrancy = 'dark'
        opts.transparent = true
        opts.frame = false
        opts.backgroundColor = '#00000000'
        opts.darkTheme = true
        opts.titleBarStyle = 'hiddenInset'
        opts.webPreferences.nodeIntegration = true
        const win = new BrowserWindow(opts)
        win.loadURL('file://'+path.join(__dirname, 'app/index.html?title='+opts.title))
        return win
    }
})()
