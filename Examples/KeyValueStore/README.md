# Key-Value Store Example

This example implements a simple REST server, the API is a 1-to-1 
mapping of REST commands to EMS commands, 
effectively making the EMS shared memory model
an object store service over the network.
The RESTful API inherits all the atomicity
and persistence properties of the underlying EMS array.

## Parallelism & Deadlocks

The HTTP web interface used is Node's built-in HTTP module which
implements a serial event loop to process HTTP requests one at
time in a synchronous (blocking) fashion.
The execution time of EMS operations is similar to any other variable reference,
making EMS most efficient as a synchronous programming model,
however, because a `readFE` request may block the Node event loop
clients susceptible to deadlock.
A `writeEF` request that would unblock execution cannot handled 
by the single-threaded Node HTTP server that is blocked on a
`readFE`, effectively deadlocking the system.

The two potential solutions, asynchronous execution and multi-processing, 
are complementary forms of parallelism that may be used
to address the deadlock problem:

- __Asynchronous Execution__:<br>
  Blocking calls are turned into continuations that will be scheduled
  for execution again later.
  The number of in-flight operations before potential deadlock is equal
  to the number of continuations the runtime implements.
  <br>
  This is not presently implemented, and would require the addition
   of new non-blocking variants of EMS functions, such as 
   `nbReadFE()` and `nbWriteEF()`.
- __Multi-Processing__:<br>
  Events are distributed across multiple single-threaded event loops.
  Idle threads are assigned new events as they arrive, and each event
   loop completes an event before beginning a new one.  The number of
   in-flight operations before potential deadlock is equal to the number
   of processes.
   <br>
   This is implemented using Node's built-in [Cluster](https://nodejs.org/api/cluster.html) module.
    

## Starting the EMS Server process

Using Node's built-in `cluster` module that forks multiple Node.js
processes, we implement the multiple event loops needed for multi-processing.
Node's built-in `http` module provides the serial HTTP event loop which is
 then parallelized using the `cluster` module.
 `cluster` manages all child process creation, destruction, and communication.
 
 Starting the web server from the command line:
 ```bash
node kv_store.js [# processes]
```

If the number of processes is not specified, 8 are started.

## Manually Sending Commands using CURL

An EMS array is presented as a Key-Value store with EMS memory operations
presented as a REST interface.
An ordinary Node.js Cluster is started to demonstrate parallelism outside of EMS.

```bash
node kv_store.js 8   # Start 8 servers, logs will appear on this terminal
```

```bash
dunlin> curl localhost:11080/writeXF?foo=Hello   # Unconditionally write "Hello" to the key "foo" and mark Full
dunlin> curl localhost:11080/faa?foo=+World!   # Fetch And Add returns original value: "Hello"
"Hello"
dunlin> curl localhost:11080/readFF?foo  # Read Full and leave Full, final value is "Hello World!"
"Hello World!"
dunlin>
```


## Producer-Consumer

<table>
<tr>
<td width="50%">

```bash
dunlin> curl localhost:11080/readFE?bar  # EMS memory initialized as empty, this blocks
```
</td>
<td>

```bash
dunlin> curl localhost:11080/readFF?foo  # Read Full and leave Full, final value is "Hello World!"
```
</td>
</tr>
</table>
