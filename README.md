# Extended Memory Semantics (EMS)

EMS makes possible shared memory multithreaded parallelism in Node.js (and soon Python).

### <em>EMS is targeted at problems too large for one core, but too small for a scalable cluster.</em>

A modern multicore server has 16-32 cores and over 200GB of memory,
equivalent to an entire rack of systems from a few years ago.
As a consequence, jobs formerly requiring a Map-Reduce cluster
can now be performed entirely in shared memory on a single server
without using distributed programming.


## Types of Concurrency
<center>
  <table >
    <tr>
      <td>
EMS complements an application's 
asynchronous and distributed concurrency with
multithreading, transactional memory, and 
other fine-grained synchronization capabilities.
<br><br>
EMS supports Fork-Join and Bulk Synchronous Parallel (BSP) execution models.
Fork-Join execution begins with a single thread that creates and ends
parallel regions as needed, whereas BSP execution begins with each
thread entering the program at <code>main</code> and executing all
the statements.
<br><br>
The same underlying EMS primitives (stacks, queues, transactions, 
atomic read-modify-write operations, etc.)
are used in both execution modes.
            </td>
            <td width="50%">
              <center>
  <center>
  <img height="350px" style="margin-left: 80px;" src="http://synsem.com/EMS.js/typesOfParallelism.svg" type="image/svg+xml"  />
  </center>
              </center>
            </td>
    </tr>
  </table>
</center>


## Examples
### Word Counting Using Atomic Operations
The typical Map-Reduce example of word counting, 
iterating over documents in parallel and atomically incrementing the count of each
word found.

```javascript
var ems  = require('ems')(process.argv[2])  // Command line argument for # of threads to use

//  Allocate the EMS shared memory holding the word count dictionary
var maxNKeys    = 10000000
var wordCounts   = ems.new( {
    dimensions : [ maxNKeys ],  // Maximum # of different keys the array can store
    heapSize   : maxNKeys * 10, // 10 bytes of storage per key, used to store dictionary word itself
    useMap     : true,          // Use a key-index mapping, not integer indexes
    setFEtags  : 'full',        // Initial full/empty state of array elements
    dataFill   : 0              // Initial value of new keys
} )

//  Get the list of files to process
var dir          = fs.readdirSync('/Data/Gutenberg/all/');
var splitPattern = new RegExp(/[ \n,\.\\\/_\-\<\>:\;\!\@\#\$\%\&\*\(\)=\[\]|\{\}\?\—]/)

//  Parallel loop over documents
ems.parForEach(0, dir.length,  function(docNum) {
    var text = fs.readFileSync('/Data/Gutenberg/all/' + dir[docNum], 'utf8', "r")    
    var words = text.replace(/[\n\r]/g,' ').toLowerCase().split(splitPattern)
    words.forEach( function(word) {
        wordCounts.faa(word, 1)  // Atomic Fetch-and-Add updates word count
    } )
} )
```

<P>
  <img style="vertical-align:text-top;" height="250px" src="http://synsem.com/EMS.js/wordCount.png" />
  Word Count of documents from Project Gutenberg in a variety of languages.  Average document was
  about 250kb in length.
  The performance of this program using an Amazon EC2 instance:<br>
  <code>cr1.8xlarge: 244 GiB memory, 88 EC2 Compute Units, 
240 GB of local instance storage, 64-bit platform, 10 Gigabit Ethernet</code>
  <br>
</P>


### Transactional Memory
Using EMS Transactional Memory to atomically update
two account balances while simultaneously preventing updates to the user's 
customer records.

```javascript
var ems = require('ems')(process.argv[2])        // Initialize EMS
var customers = ems.new(...)                     // Allocate EMS memory for customer records
var accounts  = ems.new(...)                     // Allocate accounts
...
// Start a transaction involving Bob and Sue
var transaction= ems.tmStart( [ [customers, 'Bob Smith', true],  // Read-only:  Bob's customer record
                                [customers, 'Sue Jones', true],  // Read-only:  Sue's customer record
                                [accounts, 'Bob Smith'],         // Read-Write: Bob's account balance
                                [accounts, 'Sue Jones'] ] )      // Read-Write: Sue's account balance
                                
// Transfer the payment and update the balances
var bobsBalance = accounts.read('Bob Smith')                // Read the balance of Bob's account
accounts.write('Bob Smith', bobsBalance - paymentAmount)    // Deduct the payment and write the new balance back
var suesBalance = accounts.read('Sue Jones')                // Read the balance of Sue's account
accounts.write('Sue Jones', suesBalance + paymentAmount)    // Add the payment to Sue's account

// Commit the transaction or abort it if NSF
if(balance > paymentAmount) {                               // Test for overdraft
    ems.tmEnd(transaction, true)                            // Sufficient funds, commit transaction
} else {
    ems.tmEnd(transaction, false)                           // Not Sufficient Funds, roll back transaction
}
```

<P>
  <img style="vertical-align:text-top;" height="250px" src="http://synsem.com/EMS.js/TMfromLoop.png" />
  <img style="vertical-align:text-top;" height="250px" src="http://synsem.com/EMS.js/TMfromQ.png" />
  <br>
  The performance of this program using an Amazon EC2 instance:<br>
  <code>cr1.8xlarge: 244 GiB memory, 88 EC2 Compute Units, 
240 GB of local instance storage, 64-bit platform, 10 Gigabit Ethernet</code>
  <br>
</P>





## Synchronization as a Property of the Data, Not a Duty for Tasks

EMS internally stores tags that are used for synchronization of
user data, allowing synchronization to happen independently of
the number or kind of processes accessing the data.  The tags
can be thought of as being in one of three states, <em>Empty,
Full,</em> or <em>Read-Only</em>, and the EMS intrinsic functions
enforce atomic access through automatic state transitions.

The EMS array may be indexed directly using an integer, or using a key-index
mapping from any primitive type.  When a map is used, the key and data
  itself are updated atomically.
  

  <center>
  <table >
    <tr>
      <td>
    <center>
      <img style="width:350px; "
	   src="http://synsem.com/EMS.js/memLayoutLogical.svg" type="image/svg+xml" />
      <em>    <br><br> 
    EMS memory is an array of JSON primitive values
        (Number, Boolean, String, or Undefined) accessed using atomic
        operators and/or transactional memory.  Safe parallel access
        is managed by passing through multiple gates: First mapping a
        key to an index, then accessing user data protected by EMS
        tags, and completing the whole operation atomically.
      </center>
    </em>
    </td>
    <td width="50%">
      <center>
  <img style="height:270px; "
   src="http://synsem.com/EMS.js/fsmSimple.svg" type="image/svg+xml" />
    <em>    <br><br> EMS Data Tag Transitions & Atomic operations:
    F=Full, E=Empty, X=Don't Care, RW=Readers-Writer lock (# of current readers)
    CAS=Compare-and-Swap, FAA=Fetch-and-Add</em>
      </center>
    </td>
    </tr>
  </table>
  </center>  

### Less is More
Because all systems are already multicore, 
multithreading requires no additional equipment, system permissions,
or application services, making it easy to get started.
The reduced complexity of
lightweight threads communicating through shared memory
is reflected in a rapid code-debug cycle for ad-hoc application development.


## More Technical Information

For a more complete description of the principles of operation,
[visit the EMS web site.](http://synsem.com/EMS.js/)

[ Complete API reference ](http://synsem.com/EMS.js/reference.html)
  
<br>
<center>
  <img src="http://synsem.com/EMS.js/blockDiagram.svg" type="image/svg+xml" height="300px" style="vertical-align:text-top;"/>
</center>


## Installation

### Install via npm
EMS is available as a NPM Package.  EMS itself has no external dependencies,
but does require compiling native C++ functions using <code>node-gyp</code>,
which is also available as a NPM (<code>sudo npm install -g node-gyp</code>).

The native C parts of EMS depend on other NPM packages to compile and load.
Specifically, the Foreign Function Interface (ffi), C-to-V8 symbol renaming (bindings),
and the native addon abstraction layer (nan) are also required to compile EMS.

```sh
npm install ems
```

### Install via GitHub
Download the source code.  Be sure to compile the native code with <code>node-gyp</code>.

```sh
git clone https://github.com/SyntheticSemantics/ems.git
cd ems/ems/
npm install bindings ffi nan
node-gyp configure
node-gyp build
```

To make this EMS development build the one used by the examples,
set up a local node_modules that is symbolically linked to the current build.
Replace "Examples" with "Tests" to execute tests from the development build.
```sh
cd ../Examples/
mkdir node_modules
cd node_modules/
ln -s ../../../ems/ ems
cd ../
```


### Run Some Examples
On a Mac and some Linux distributions programs will "just work", but
some Linux distributions restrict access to shared memory.  The
quick workaround is to run jobs as root, a long-term solution will
vary with Linux distribution.

<bold>

Run an example processes 8 processes:
```sh
cd Examples
node concurrent_Q_and_TM.js 8
```

Running all the tests with 16 processes:
```sh
cd Tests;
rm EMS
for test in `ls *js`; do  node $test 16; done
```


## Platforms Supported
As of 2016-01-03, Mac/Darwin and Linux are supported.  A windows port pull request is welcomed!


## Roadmap
EMS 1.0 uses Nan for long-term Node.js support, and in the near
future a Python API will also be available.

EMS 2.0 is in the planning phase, the new API will more tightly integrate with the
language making atomic operations on persistent memory
more transparent, and simplifying integration with legacy applications.
The types of persistent and Non-Volatile Memory (NVM) EMS was designed
for, such as [pmem.io](http://pmem.io/),
will be introduced into servers in the next few years, we hope to 
leverage their work and future-proof EMS against architectural shifts.

## License
BSD, other commercial and open source licenses are available.

## Links
[Visit the EMS web site](http://synsem.com/EMS.js/)

[Download the NPM](https://www.npmjs.org/package/ems)

[Get the source at GitHub](https://github.com/SyntheticSemantics/ems)

## About
Jace A Mogill specializes in FPGA/Software Co-Design, most recently
designing and implementing hardware accelerators for Python & Javascript.
He has over 20 years experience optimizing software for distributed, multi-core, and 
hybrid computer architectures.
