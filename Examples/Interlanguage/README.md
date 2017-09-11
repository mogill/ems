# Inter-Language Persistent Shared Memory

In *The Future™*,  memory will have new capabilities that
programmers will make decisions about how best to use:
- __Non-Volatility__ - No more "loading" files into variables in memory,
  just use the same variables as last time.
- __Sharing__ - Directly sharing objects between programs instead of
  communicating them through network messages.
- __Selectable Latency__ - Expensive fast memory for frequently accessed variables,
  cheaper slower memory for less frequently used data.
- __Correctness__ - Very inexpensive bulk memory might give approximate values instead
  of the exact value stored.
- __Location__ - Allocate memory in another another rack or data center
- __Mirroring__ - Automatic fail-over to backup memory  
  
EMS presently supports the first two and is ready
to incorporate new memory capabilities as they become available.

## Sharing Objects Between Programs

Sharing EMS data between programs written in different languages 
is no different than sharing data between programs in the same language,
or sharing data between same program run multiple times.



### Javascript Semantics in Other Languages

Every language has it's own semantics and storage sequences for it's data structures,
EMS makes the various representations interoperable using Javascript's semantics.
That is, if your Python program relies on horrifying semantics like:

```python
>>> a = [1,2,3]
>>> a + "abc"
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
TypeError: can only concatenate list (not "str") to list
>>> a += "abc"
>>> a
[1, 2, 3, 'a', 'b', 'c']
>>>
``` 

You will be horrified to learn EMS will instead follow Javascript's horrifying semantics:

```javascript
> a = [1,2,3]
[ 1, 2, 3 ]
> a + 'abc'
'1,2,3abc'
> a += 'abc'
'1,2,3abc'
>
```

You may be nonplussed to learn this is *The Future™*.
I, too, was horrified and baffled at this discovery. 
Alas, this is an inevitable truth of *The Future™*.


### Possible Orders for Starting Processes

Two processes sharing memory may start in any order, meaning programs
must take care when initializing to not overwrite another program which
may also be trying to initialize, or detect when it is necessary to
recover from a previous error.

| Persistent EMS Array File  | (Re-)Initialize | Use |
| :------------- |:-------------:| :-----:|
| Already exists      | A one-time initialization process truncates and creates the EMS array. The first program to start must create the EMS file with the `useExisting : false` attribute | Any number of processes may attach to the EMS array in any order using the attribute  `useExisting : true` |
| Does not yet exist  |   Subsequent programs attach to the new EMS file with the `useExisting : true` attribute | N/A |


# Running the Example
The programs `interlanguage.py` and `interlanguage.js` are complementary
to each other, and must be started in the correct order:

__First start the Javascript program, then start the Python.__

```bash
dunlin> node interlanguage.js
JS Started, waiting for Python to start...
        [ EXECUTION STALLS UNTIL PYTHON PROGRAM STARTS ]
Hello from Python
...
``` 

Second, start the Python program
```bash
dunlin> python interlanguage.py
Start this program after the JS program.
If this Python appears hung and does not respond to ^C, also try ^\ and ^Z
----------------------------------------------------------------------------
Hello  from Javascript
...
```
Due to a well-intentioned signal mask in the Python runtime meant to
provide resiliency in the face of a failing native plug-in module,
sometimes Python will appear hung when it is blocked on an EMS operation.
In these situations an alternative way to kill the Python interpreter 
is with SIGQUIT (Control-\ from most shells), or back-grounding the
job with Control-Z and sending the job a kill signal. 

 
### What the Example is Doing
- Each process writes a hello message to an EMS variable, marking it full.
- Each process reads the variable written by the other process, blocking until it has been written
- A simple point-to-point barrier is demonstrated by handshaking
- Illustrate some of the differences between Python and Javascript nested objects
- Both JS and Py enter a loop that increments a shared counter
- JS checks the results to make sure every iteration occurred once
- Illustrate some quirks of language design in JS and Py
- JS leaves a shared counter running in the background via `setInterval`
- Python enters a loop that occasionally reads the shared counter, showing it being updated
- Python over-writes the counter with `stop`
- JS notices the counter has been replaced with `stop` and exits.



### Interactive Example
Illustration of sharing data between the Node.js and Python 
done interactively from the REPLs.
 
<img src="http://synsem.com/images/ems_js_py.gif" />
