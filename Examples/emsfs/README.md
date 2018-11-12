# memfs

rough experiment with ems shared mem sync file system in js,example is an electron app but would work in node.js.

for production electron apps, initial load times & responsiveness bottlenecks at the top-level require statements needed before first load & paints, especially with webpack (ugh) in electron.  this mitigates I/O hit by patching syncronous fs APIs to use on-demand shared memory cache of file content.  benefit won't fully materialize until used by several pages within a flow & said patches install before internal electron requires initialize the main nodejs proc, browserWindows, workers and any other v8 js sandbox.  

currently, just opens 2 windows of the same page, one loaded via vanilla fs or asar, the other via persistable shared memory fs.  With zero optimization effots and on a single page load, emsfs already can boot from persisted shared mem with little to no perf delta.  

## Usage

`npm i`

init ems mem & run app:

`npm run start`

reuse persisted ems mem & run app:

`npm run restart`

## Roadmap

- [x] writeFileSync
- [x] readFileSync
- [x] write to mem on first read
- [x] asar support
- [x] load typical page, something like React & make it pretty
- [ ] multiple pages
- [ ] statSync
- [ ] emsfs packager api (a beter asar) just with persisted ems - file
- [ ] file updates
- [ ] patch electron.asar in node_modules/electron to ensure all paths can be intercepted
- [ ] benchmarks
- [ ] add buffer support to ems & bench impact
- [ ] e2e fs compat tests
