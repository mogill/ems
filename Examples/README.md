# Extended Memory Semantics (EMS) Examples
#### Table of Contents
* [Simple Loop Benchmarks](#Simple-Loop-Benchmarks)
* [Key-Value Store](#kv_store.js)
* [Work Queues](#Work-Queues)
* [Parallel Web Server](#web_server.js)
* [Word Counting](#Word-Count)
* [Implied EMS Operations](#harmony_proxies.js)
* [Inter-Language Programming](#Inter-language-Programming)

## Simple Loop Benchmarks

Click here for a __[Detailed Description of the EMS<nolink>.js STREAMS implementation](https://github.com/SyntheticSemantics/ems/tree/master/Examples/STREAMS)__.

The original [STREAMS](https://www.cs.virginia.edu/stream/)
benchmark attempts to measure the _Speed Not To Exceed_ 
bandwidth of a CPU to it's attached RAM by performing large
numbers of simple operations on an array of integers or floats.
This version of STREAMS for EMS<nolink>.js measures the rate of different atomic operations on
shared integer data, it is implemented twice using two different parallelization schemes:
Bulk Synchronous Parallel (BSP), and Fork-Join (FJ).

Experimental results are from the BSP version
 on an AWS `c4.8xlarge (132 ECUs, 36 vCPUs, 2.9 GHz, Intel Xeon E5-2666v3, 60 GiB memory`.

<center><img src="../Docs/streams.svg" type="image/svg+xml" height="360px">
</center>


## Parallelism Within a Single HTTP Request
Click here for the [Web Server Parallelism Example](https://github.com/SyntheticSemantics/ems/tree/master/Examples/WebServer)
<img src="http://synsem.com/images/ems/parWebServer.svg" type="image/svg+xml">
EMS is a memory model and has no daemon process to monitor a network interface
for operations to execute.
This example builds on a standard HTTP server to implement
shared memory parallelism within a single web request.

## EMS as a Key Value Store
Click here for the [EMS Key-Value Store Example](https://github.com/SyntheticSemantics/ems/tree/master/Examples/KeyValueStore)

An EMS array is used as a Key-Value store with a 1-to-1 mapping of REST operations 
to EMS memory operations.
The KV server is implemented using the Node.js built-in `Cluster` module to
demonstrate parallelism sources outside of EMS.


## Work Queues
The parameters of the experiment are defined in the `initializeSharedData()` function.
An experiment creates several new EMS arrays and a work queue of 
randomly generated transactions that update one or more of the arrays.
Parallel processes dequeue the work and perform atomic updates on 
the new EMS arrays.  Finally, the experiment performs checksums to ensure
all the transactions were performed properly.

```javascript
    arrLen = 1000000;         // Maximum number of elements the EMS array can contain
    heapSize = 100000;        // Amount of persistent memory to reserve for transactions
    nTransactions = 1000000;  // Number of transactions to perform in the experiment
    nTables = 6;              // Number of EMS arrays to perform transactions across
    maxNops = 5;              // Maximum number of EMS arrays to update during a transaction
```

### `concurrent_Q_and_TM.js`
```bash
node concurrent_Q_and_TM.js 4   # 1 process enqueues then processes work, 3 processes perform work
````
All processes are started, one enqueues transactions while the others dequeue 
and perform the work.  When all the work has been enqueued, that process also
begins dequeing and performing work.

<img style="vertical-align:text-top;" src="../Docs/tm_from_q.svg" />

### `workQ_and_TM.js`
```bash
node workQ_and_TM.js 4   # Enqueue all the work, all 4 processes deqeue and perform work
````
All the transactions are enqueued, then all the processes begin 
to dequeue and perform work until no work remains in the queue.



## [Word Counting](#Word-Count)
Map-Reduce is often demonstrated using word counting because each document can
be processed in parallel, and the results of each document's dictionary reduced
into a single dictionary.
In contrast with Map-Reduce implementations using a 
dictionary per document, the EMS implementation 
maintains a single shared dictionary, eliminating the need for a reduce phase.

The word counts are sorted in parallel and the most frequently appearing words
are printed with their counts.


### Word Counting Using Atomic Operations
A parallel word counting program is implemented in Python and Javascript.
A shared dictionary is used by all the tasks, updates are made using
atomic operations.  Specifically, `Compare And Swap (CAS)` is used to
initialize a new word from undefined to a count of 1.  If this CAS
fails, the word already exists in the dictionary and the operation
is retried using `Fetch and Add`, atomically incrementing the count.

### Forming the "Bag of Words" with Word Counts
```javascript
var file_list = fs.readdirSync(doc_path);
var splitPattern = new RegExp(/[ \n,\.\\/_\-\<\>:\;\!\@\#\$\%\&\*\(\)=\[\]|\"\'\{\}\?\—]/);
// Iterate over all the files in parallel
ems.parForEach(0, file_list.length, function (fileNum) {
    var text = fs.readFileSync(doc_path + file_list[fileNum], 'utf8', "r");
    var words = text.replace(/[\n\r]/g, ' ').toLowerCase().split(splitPattern);
    //  Sequentially iterate over all the words in one document
    words.forEach(function (word) {
        wordCounts.faa(word, 1); //  Atomically increment the count of times this word was seen
    });
});
```

### Sorting the Word Count list by Frequency
Parallel sorting of the word count is implemented as a multi-step process:

1. The bag of words is processed by all the processess, each process
   building an ordered list of the most common words it encounters
2. The partial lists from all the processes are concatenated into a single list
3. The list of the most common words seen is sorted and truncated

```javascript
var biggest_counts = new Array(local_sort_len).fill({"key": 0, "count": 0});
ems.parForEach(0, maxNKeys, function (keyN) {
    var key = wordCounts.index2key(keyN);
    if (key) {
        //  Perform an insertion sort of the new key into the biggest_counts
        //  list, deleting the last (smallest) element to preserve length.
        var keyCount = wordCounts.read(key);
        var idx = local_sort_len - 1;
        while (idx >= 0  &&  biggest_counts[idx].count < keyCount) { idx -= 1; }
        var newBiggest = {"key": key, "count": keyCount};
        if (idx < 0) {
            biggest_counts = [newBiggest].concat(biggest_counts.slice(0, biggest_counts.length - 1));
        } else if (idx < local_sort_len) {
            var left = biggest_counts.slice(0, idx + 1);
            var right = biggest_counts.slice(idx + 1);
            biggest_counts = left.concat([newBiggest].concat(right)).slice(0, -1);
        }
    }
});
//  Concatenate all the partial (one per process) lists into one list
stats.writeXF('most_frequent', []);  // Initialize the list
ems.barrier();  // Wait for all procs to have finished initialization
stats.writeEF('most_frequent', stats.readFE('most_frequent').concat(biggest_counts));
ems.barrier();  // Wait for all procs to have finished merge
ems.master(function() { //  Sort & print the list of words, only one process is needed
    biggest_counts = stats.readFF('most_frequent');
    biggest_counts.sort(function (a, b) { return b.count - a.count; });
    //  Print only the first "local_sort_len" items -- assume the worst case
    //  of all the largest counts are discovered by a single process
    console.log("Most frequently appearing terms:");
    for (var index = 0;  index < local_sort_len;  index += 1) {
        console.log(index + ': ' + biggest_counts[index].key + "   " + biggest_counts[index].count);
    }
});
```

### Word Count Performance
This section reports the results of running the Word Count example on
documents from Project Gutenberg.
2,981,712,952 words in several languages were parsed, totaling 12,664,852,220 bytes of text.

<img height="300px" src="../Docs/wordcount.svg" />



## Inter-Language Programming
[Inter-Language Shared Memory Programming](https://github.com/SyntheticSemantics/ems/tree/master/Examples/Interlanguage)

The programs `interlanguage.js` and `interlanguage.py` demonstrate sharing
objects between Javascript and Python.
A variety of synchronization and execution models are shown.


### Example
<img src="../Docs/ems_js_py.gif" />

###### Copyright (C)2017 Jace A Mogill
