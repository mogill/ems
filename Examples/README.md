# EMS Examples
#### Table of Contents
##### Node.js
* [Simple Loop Benchmarks](#Simple\ Loop\ Benchmarks)
* [Work Queues](#Work\ Queues)
* [Key-Value Store](#kv_store.js)
* [Web Server](#web_server.js)
* [Word Counting](#wordCount.js )

##### Python
* [Parallel Loops](#Parallel\ Loops)


## Simple Loop Benchmarks
The original [STREAMS](https://www.cs.virginia.edu/stream/)
benchmark measures the _Speed Not To Exceed_ 
bandwidth of a CPU to it's attached RAM by performing large
numbers of simple operations.  This version for
EMS<nolink>.js measures the rate of different atomic operations on
shared integer data and is implemented using two different parallelization schemes:
Bulk Synchronous Parallel (BSP), and Fork-Join.

Experimental results are from an AWS
`c4.8xlarge (132 ECUs, 36 vCPUs, 2.9 GHz, Intel Xeon E5-2666v3, 60 GiB memory`.

<center><img src="http://synsem.com/images/ems/streams.svg" type="image/svg+xml" height="300px">
</center>

### streams_bulk_sync_parallel.js
```bash
node streams_bulk_sync_parallel.js 6   # Use 6 processes
```
BSP parallelism starts all the processes at the program's main entry point
and all statements are executed, barriers are used to synchronize execution
between loops.

### streams_fork_join.js
```bash
node streams_fork_join.js 4  # Use 4 processes
```
FJ parallel execution begins with a single master thread which creates
new threads as needed (ie: to execute a loop) which exit when there is
no work left, at which point the master thread joins 


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

### concurrent_Q_and_TM.js
```bash
node concurrent_Q_and_TM.js 4   # 1 process enqueues then processes work, 3 processes perform work
````
All processes are started, one enqueues transactions while the others dequeue 
and perform the work.  When all the work has been enqueued, that process also
begins dequeing and performing work.

### workQ_and_TM.js
```bash
node workQ_and_TM.js 4   # Enqueue all the work, all 4 processes deqeue and perform work
````
All the transactions are enqueued, then all the processes begin 
to dequeue and perform work until no work remains in the queue.




## kv_store.js
An EMS array is presented as a Key-Value store with EMS memory operations
presented as a REST interface.
An ordinary Node.js Cluster is started to demonstrate parallelism outside of EMS.

```bash
node kv_store.js 2   # Start the server, logs will appear on this terminal
```

```bash
curl localhost:8080/writeXF?foo=Hello   # Unconditionally write "Hello" to the key "foo" and mark Full
curl localhost:8080/faa?foo=+World!   # Fetch And Add returns original value: "Hello"
curl localhost:8080/readFF?foo  # Read Full and leave Full, final value is "Hello World!"
```


## web_server.js
A parallel region is created for each request received by the web server,
handling the single request with multiple processes.

```bash
curl http://localhost:8080/?foo=data1   # Insert ephemeral data1 with the key "foo"
curl http://localhost:8080/persist?foo=data2   # Previous data was not persistent, this is a new persistent foo
curl http://localhost:8080/existing/persist?foo=data3   # This data is appended to the already persisted data
curl http://localhost:8080/existing/persist/unique?foo=nowhere  # A GUID is generated for this operation
curl http://localhost:8080/existing/persist/reset?foo=data5  # Persisted data is appended to, the response is issued, and the data is deleted
curl http://localhost:8080/existing?foo=final  # Persisted reset data is appended to
```

## wordCount<nolink>.js
### Word Counting Using Atomic Operations
Map-Reduce is often demonstrated using word counting because each document can
be processed in parallel, and the results of each document's dictionary reduced
into a single dictionary.  This EMS implementation also
iterates over documents in parallel, but it maintains a single shared dictionary
across processes, atomically incrementing the count of each word found.
The final word counts are sorted and the most frequently appearing words
are printed with their counts.

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

<img height="300px" src="http://synsem.com/images/ems/wordcount.svg" />


## Python
### Parallel Loops
