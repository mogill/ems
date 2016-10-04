"""
 +-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2016, Jace A Mogill.  All rights reserved.                   |
 |                                                                             |
 | Redistribution and use in source and binary forms, with or without          |
 | modification, are permitted provided that the following conditions are met: |
 |    * Redistributions of source code must retain the above copyright         |
 |      notice, this list of conditions and the following disclaimer.          |
 |    * Redistributions in binary form must reproduce the above copyright      |
 |      notice, this list of conditions and the following disclaimer in the    |
 |      documentation and/or other materials provided with the distribution.   |
 |    * Neither the name of the Synthetic Semantics nor the names of its       |
 |      contributors may be used to endorse or promote products derived        |
 |      from this software without specific prior written permission.          |
 |                                                                             |
 |    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS      |
 |    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT        |
 |    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR    |
 |    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SYNTHETIC         |
 |    SEMANTICS LLC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,   |
 |    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,      |
 |    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR       |
 |    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF   |
 |    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING     |
 |    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS       |
 |    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.             |
 |                                                                             |
 +-----------------------------------------------------------------------------+
"""
import socket
import inspect
import sys
import os
import math
import json
import re
import subprocess
from multiprocessing import Process, Pipe

# =======================================================================================
#   rm -f libems.so *.o; clang -dynamiclib -dynamiclib *.cc -o libems.so
#   gcc -fPIC -shared -Wl,-soname,libems.so -o libems.so *.cc -lrt
"""
sudo apt-get install python-pip
sudo apt-get install libffi-dev
sudo pip install cffi
sudo apt-get install python3-cffi
"""
from cffi import FFI
ffi = FFI()
cpp_out = subprocess.check_output(["cpp", "../src/ems_proto.h"]).decode("utf-8")
prototypes = cpp_out.split("\n")
headerLines = []
for line in prototypes:
    # Strip CPP directives and annotations
    line = re.sub("^#.*$", "", line)
    # Strip extern attribute
    line = re.sub("extern \"C\" ", "", line)
    if line is not "":
        headerLines.append(line)

# Delcare the CFFI Headers
ffi.cdef('\n'.join(headerLines))

# Find the .so and load it
libems = None
import site
package_paths = site.getsitepackages()
for package_path in package_paths:
    try:
        packages = os.listdir(package_path)
        for package in packages:
            if package == "libems" and libems is None:
                libems_path = package_path + "/libems/"
                files = os.listdir(libems_path)
                for file in files:
                    if file[-3:] == ".so":  # TODO: Guessing it's the only .so
                        fname = libems_path + file
                        libems = ffi.dlopen(fname)
                        break
    except:
        # print("This path does not exist:", package_path, "|", type (package_path))
        pass

# Do not GC the EMS values until deleted
import weakref
global_weakkeydict = weakref.WeakKeyDictionary()

# class initialize(object):
# This enumeration is copied from ems.h
TYPE_INVALID   = 0
TYPE_BOOLEAN   = 1
TYPE_STRING    = 2
TYPE_FLOAT     = 3
TYPE_INTEGER   = 4
TYPE_UNDEFINED = 5
TYPE_JSON      = 6  # Catch-all for JSON arrays and Objects

TAG_ANY     = 4  # Never stored, used for matching
TAG_RW_LOCK = 3
TAG_BUSY    = 2
TAG_EMPTY   = 1
TAG_FULL    = 0


# def emsThreadStub(taskN, nProcs, conn):
def emsThreadStub():
    global ems
    import socket
    import sys
    sys.path.append('./')
    sys.path.append("../Python/")
    import ems
    print("STUB: This is the start of it", sys.argv[2])
    ems.initialize(sys.argv[2])
    # ems.initialize(nProcs)
    taskN = int(sys.argv[2])
    sock = socket.create_connection(('localhost', 10000 + taskN)) # TODO MAGIC NUMBER
    # sock = socket.create_connection(('localhost', 10000 + taskN))  # TODO MAGIC NUMBER
    ems.diag("Reporting for duty!" + str(taskN))
    # ems.diag("Reporting for duty:" + str(taskN))
    try:
        while True:
            data = sock.recv(16000)  # TODO MAGIC
            if data == "STOP!":
                print('STUB: Stopping')
                sock.close()
                exit(0)
            try:
                exec(data)
                sock.send(bytes("STUB says: That was fun", 'utf-8'))
                # print('STUB: successful EXEC: "%s %s"' % (data, type(data)))
            except:
                sock.send(bytes("STUB says: Something went wrong", 'utf-8'))
                print('STUB: bad EXEC: "%s %s"' % (data, type(data)))
    finally:
        print('STUB:  Awww crap, everything went wrong.  closing socket')
        sock.close()


def initialize(nThreadsArg, pinThreadsArg=False, threadingType='bsp',
             contextname='/EMS_MainDomain'):
    """EMS object initialization, invoked by the require statement"""
    nThreadsArg = int(nThreadsArg)
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    if not nThreadsArg > 0:
        print("EMS: Must declare number of nodes to use.  Input:" + nThreadsArg)
        return

    if sys.argv[len(sys.argv) - 2] == 'EMS_Subtask':
        myID = int(sys.argv[len(sys.argv) - 1])
    else:
        myID = 0

    _regionN = 0
    nThreads = nThreadsArg
    pinThreads = pinThreadsArg
    threadingType = threadingType
    domainName = contextname
    c_contextname = _new_EMSval(contextname)
    c_None = _new_EMSval(None)
    # All arguments are defined -- now do the EMS initialization
    EMSmmapID = libems.EMSinitialize(0, 0, False,
                                     c_contextname,
                                     False, False,  #  4-5
                                     False, 0,  # 6-7
                                     c_None,  # 8
                                     False, TAG_FULL, myID, pinThreads, nThreads, 99)

    #  The master thread has completed initialization, other threads may now
    #  safely execute.
    if threadingType == 'bsp':
        inParallelContext = True
        if myID == 0:
            for taskN in range(1, nThreads):
                os.system('./' + (' '.join(sys.argv)) + " EMS_Subtask " + str(taskN) + ' &')
    elif threadingType == 'fj':
        inParallelContext = False
        if myID == 0:
            args = sys.argv[:]
            args[0] = './EMS_thread_stub.py'
            text_file = open(args[0], "w")
            text_file.write(inspect.getsource(emsThreadStub) + "\nemsThreadStub()\n")
            text_file.close()
            tasks = []
            for taskN in range(1, nThreads):
                """
                parent_conn, child_conn = Pipe()
                p = Process(target=emsThreadStub, args=(taskN, nThreads, child_conn))
                # tasks.append(parent_conn)
                p.start()
                """
                print("Running:", 'python3 ./' + (' '.join(args)) + " EMS_Subtask " + str(taskN) + ' &')
                os.system('python3 ./' + (' '.join(args)) + " EMS_Subtask " + str(taskN) + ' &')
                # Slave process is presently running thread_stub, now connect to it
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                server_address = ('localhost', 10000 + taskN)  # TODO MAGIC NUMBER
                print('starting up on %s port %s' % server_address)
                sock.bind(server_address)
                print("after bind")
                sock.listen(1)
                print("after listen")
                connection, client_address = sock.accept()
                print("after accept")
                tasks.append(connection)
        print("that job is started:", taskN)

    elif threadingType == 'user':
        inParallelContext = False
    else:
        print("EMS ERROR: Unknown threading model type:" + str(threadingType))
        myID = -1
        return


def _loop_chunk():
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    start = ffi.new("int32_t *", 1)
    end = ffi.new("int32_t *", 1)
    if not libems.EMSloopChunk(EMSmmapID, start, end):
        diag("_loop_chunk: ERROR -- Not a valid block of iterations")
        return False
    return {'start': start[0], 'end': end[0]}


def diag(text):
    """Print a message to the console with a prefix indicating which thread is printing
    """
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    print("EMStask " + str(myID) + ": " + text)


def parallel(func, *kargs):
    """Co-Begin a parallel region, executing the function 'func'"""
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    print("EMSparallel: starting without closures!")
    inParallelContext = True
    taskN = 1
    for conn in tasks:
        command_str = "\n" + inspect.getsource(func) + "\n"
        quoted_args = [json.dumps(arg) for arg in kargs]
        command_str += func.__name__ + "(" + ",".join(quoted_args) + ")"
        # print("commandstr:", command_str1, "||||||", command_str2)
        conn.sendall(bytes(command_str +
                           "\nems.diag(\"This is befgore the barrier\")\nems.barrier()\nems.diag(\"This is after the varrier\")\n", 'utf-8'))
        # data = conn.recv(16000)  # TODO MAGIC
        # print('user work received "%s"' % data)
        # # conn.sendall(bytes("\nprint(\"This is befgore the barrier\")\nems.barrier()\n", 'utf-8'))
        # data = conn.recv(16000)  # TODO MAGIC
        # print('barrier received "%s"' % data)
        taskN += 1
    print("Starting local copy in perallel region")
    func(*kargs)  # Perform the work on this process
    print("barrier at end of perallel region")
    barrier()
    inParallelContext = False


def parForEach(start,        # First iteration's index
               end,          # Final iteration's index
               loopBody,     # Function to execute each iteration
               scheduleType = 'guided', # Load balancing technique
               minChunk = 1  # Smallest block of iterations
               ):
    """Execute the local iterations of a decomposed loop with
    the specified scheduling.
    """
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads

    if scheduleType == 'static':  # Evenly divide the iterations across threads
        blocksz = math.floor((end - start) / nThreads) + 1
        s = (blocksz * myID) + start
        e = (blocksz * (myID + 1)) + start
        if e > end:
            e = end
        for idx in range(s, e):
            loopBody(idx)
    else:  # scheduleType == 'dynamic'  or  scheduleType == 'guided'  or default
        #  Initialize loop bounds, block size, etc.
        if scheduleType == 'guided':
            ems_sched_type = 1200  # From ems.h
        elif scheduleType == 'dynamic':
            ems_sched_type = 1201  # From ems.h
        else:
            print("EMS parForEach: Invalid scheduling type:", scheduleType)
            return
        libems.EMSloopInit(EMSmmapID, start, end, minChunk, ems_sched_type)
        #  Do not enter loop until all threads have completed initialization
        #  If the barrier is present at the loop end, this may replaced
        #  with first-thread initialization.
        barrier()
        extents = _loop_chunk()
        while extents['end'] - extents['start'] > 0:
            for idx in range(extents['start'], extents['end']):
                loopBody(idx)
            extents = _loop_chunk()

    #  Do not proceed until all iterations have completed
    #  TODO: OpenMP offers NOWAIT to skip this barrier
    barrier()


def tmStart(emsElems):
    """Start a Transaction
    Input is an array containing EMS elements to transition from
    Full to Empty:
    arr = [ [ emsArr0, idx0 ], [ emsArr1, idx1, true ], [ emsArr2, idx2 ] ]
    """
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads

    def getElemUID(item):
        # print("item=", item[0], item[1], item[0].index2key(item[1]) )
        # return (item[0].mmapID * MAX_ELEM_PER_REGION) + item[0].index2key(item[1])
        return str(item[0].mmapID * MAX_ELEM_PER_REGION) + str(item[1])

    MAX_ELEM_PER_REGION = 10000000000000

    #  Build a list of indexes of the elements to lock which will hold
    #  the elements sorted for deadlock free acquisition
    #  Sort the elements to lock according to a global ordering
    sortedElems = sorted(emsElems, key=getElemUID)

    #  Acquire the locks in the deadlock free order, saving the contents
    #  of the memory when it locked.
    #  Mark read-write data as Empty, and read-only data under a readers-writer lock
    tmHandle = []
    for elem in sortedElems:
        if len(elem) > 2:
            val = elem[0].readRW(elem[1])
            readonly = True
        else:
            val = elem[0].readFE(elem[1])
            readonly = False
        tmHandle.append([elem[0], elem[1], readonly, val])
    return tmHandle


def tmEnd(
          tmHandle,  # The returned value from tmStart
          doCommit   # Commit or Abort the transaction
          ):
    """Commit or abort a transaction
    The tmHandle contains the result from tmStart:
        [ [ EMSarray, index, isReadOnly, origValue ], ... ]
    """
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    for emsElem in tmHandle:
        if doCommit:
            if emsElem[2]:
                #  Is a read-only element
                emsElem[0].releaseRW(emsElem[1])
            else:
                #  Is read-write, keep current value and mark Full
                emsElem[0].setTag(emsElem[1], 'full')
        else:
            #  Abort the transaction
            if emsElem[2]:
                #  Is read-only element
                emsElem[0].releaseRW(emsElem[1])
            else:
                #  Write back the original value and mark full
                emsElem[0].writeEF(emsElem[1], emsElem[3])


# -------------------------------------------------
def critical(func, timeout=1000000):
    """Serialize execution through this function"""
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    libems.EMScriticalEnter(EMSmmapID, timeout)
    retObj = func()
    libems.EMScriticalExit(EMSmmapID)
    return retObj

def master(func):
    """Perform func only on thread 0"""
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    if myID == 0:
        return func()

def single(func):
    """Perform the function func once by the first thread to reach
    the function.  The final barrier is required because  a
    thread may try to execute the next single-execution region
    before other threads have finished this region, which the EMS
    runtime cannot tell apart.  Barriers are phased, so a barrier
    is used to prevent any thread from entering the next single-
    execution region before this one is complete
    """
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    retObj = None
    if libems.EMSsingleTask(EMSmmapID):
        retObj = func()
    barrier()
    return retObj

def barrier(timeout=10000):
    """Wrapper around the EMS global barrier"""
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    if inParallelContext:
        return libems.EMSbarrier(EMSmmapID, timeout)
    else:
        return True


def new(arg0=None,   # Maximum number of elements the EMS region can hold
        heapSize=0,    # #bytes of memory reserved for strings/arrays/objs/maps/etc
        filename=None):    # Optional filename for persistent EMS memory
    """Creating a new EMS memory region"""
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    fillIsJSON = False
    emsDescriptor = EMSarray(  #  Internal EMS descriptor
        nElements=1,    # Required: Maximum number of elements in array
        heapSize=0,     # Optional, default=0: Space, in bytes, for strings, maps, objects, etc.
        mlock=0,        # Optional, 0-100% of EMS memory into RAM
        useMap=False,   # Optional, default=false: Use a map from keys to indexes
        useExisting=False, # Optional, default=false: Preserve data if a file already exists
        persist=True,   # Optional, default=true: Preserve the file after threads exit
        doDataFill=False, # Optional, default=false: Data values should be initialized
        dataFill=None,  # Optional, default=false: Value to initialize data to
        dimStride=[]    # Stride factors for each dimension of multidimensal arrays
    )

    emsDescriptor.dataFill = _new_EMSval(None)

    if arg0 is None:  # Nothing passed in, assume length 1
        emsDescriptor.dimensions = [1]
    else:
        if type(arg0) == dict:  # User passed in emsArrayDescriptor
            if 'dimensions' in arg0:
                if type(arg0['dimensions']) == list:
                    emsDescriptor.dimensions = arg0['dimensions']
                else:
                    emsDescriptor.dimensions = [arg0['dimensions']]

            if 'heapSize' in arg0:
                emsDescriptor.heapSize = arg0['heapSize']

            if 'mlock' in arg0:
                emsDescriptor.mlock = arg0['mlock']

            if 'useMap' in arg0:
                emsDescriptor.useMap = arg0['useMap']

            if 'filename' in arg0:
                emsDescriptor.filename = arg0['filename']

            if 'persist' in arg0:
                emsDescriptor.persist = arg0['persist']

            if 'useExisting' in arg0:
                emsDescriptor.useExisting = arg0['useExisting']

            if 'doDataFill' in arg0:
                if arg0['doDataFill']:
                    emsDescriptor.doDataFill = True
                    emsDescriptor.dataFill = _new_EMSval(arg0['dataFill'])

            if 'doSetFEtags' in arg0:
                emsDescriptor.doSetFEtags = arg0['doSetFEtags']

            if 'setFEtags' in arg0:
                if (arg0['setFEtags'] == 'full'):
                    emsDescriptor.setFEtagsFull = True
                else:
                    emsDescriptor.setFEtagsFull = False
        else:
            if type(arg0) == list:  # User passed in multi-dimensional array
                emsDescriptor.dimensions = arg0
            else:
                if type(arg0) == int:  # User passed in scalar 1-D array length
                    emsDescriptor.dimensions = [arg0]
                else:
                    print("EMSnew: ERROR Non-integer type of arg0", str(arg0), type(arg0))

        if heapSize > 0:
            emsDescriptor.heapSize = heapSize

        if type(filename) == str:
            emsDescriptor.filename = filename

    if not emsDescriptor.dimensions:
        emsDescriptor.dimensions = [emsDescriptor.nElements]

    # Compute the stride factors for each dimension of a multidimensal array
    for dim in emsDescriptor.dimensions:
        emsDescriptor.dimStride.append(emsDescriptor.nElements)
        emsDescriptor.nElements *= dim

    # Name the region if a name wasn't given
    if not emsDescriptor.filename:
        emsDescriptor.filename = '/EMS_region_' + _regionN
        emsDescriptor.persist = False

    if emsDescriptor.useExisting:
        try:
            fh = open(emsDescriptor.filename, 'r')
            fh.close()
        except Exception as err:
            print("EMS ERROR: file " + emsDescriptor.filename + " should already exist, but does not. " + err)
            return

    # init() is first called from thread 0 to perform one-thread
    # only operations (ie: unlinking an old file, opening a new
    # file).  After thread 0 has completed initialization, other
    # threads can safely share the EMS array.
    if not emsDescriptor.useExisting  and  myID != 0:
        barrier()

    emsDescriptor.mmapID = libems.EMSinitialize(
        emsDescriptor.nElements,
        emsDescriptor.heapSize,    # 1
        emsDescriptor.useMap,
        emsDescriptor.filename.encode(),  # 3
        emsDescriptor.persist,
        emsDescriptor.useExisting,  # 5
        emsDescriptor.doDataFill,
        fillIsJSON,
        emsDescriptor.dataFill,
        emsDescriptor.doSetFEtags,
        emsDescriptor.setFEtagsFull,
        myID, pinThreads, nThreads,
        emsDescriptor.mlock
    )
    if not emsDescriptor.useExisting  and  myID == 0:
        barrier()

    _regionN += 1
    barrier()  # Wait until all processes finished initialization
    return emsDescriptor


# ================================================
def ems_thread_stub(taskN, conn):
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    myID = taskN
    while True:
        print("THread Stub waiting for message", taskN, conn)
        mesg = conn.recv()
        try:
            print("THread Stub message:", taskN, *mesg)
            mesg['func'](*mesg['args'])
        except:
            print("Had exception in the stub", taskN)


# =======================================================================================
def _new_EMSval(val):
    global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
    global global_weakkeydict
    emsval = ffi.new("EMSvalueType *")
    emsval[0].length = 0
    if type(val) == str:
        newval = ffi.new('char []', bytes(val, 'utf-8'))
        emsval[0].value = ffi.cast('void *', newval)
        emsval[0].length = len(newval)
        emsval[0].type = TYPE_STRING
        global_weakkeydict[emsval] = (emsval[0].length, emsval[0].type, emsval[0].value, newval)
    elif type(val) == int:
        emsval[0].type = TYPE_INTEGER
        emsval[0].value = ffi.cast('void *', val)
    elif type(val) == float:
        emsval[0].type = TYPE_FLOAT
        ud_tmp = ffi.new('ulong_double *')
        ud_tmp[0].d = ffi.cast('double', val)
        emsval[0].value = ffi.cast('void *', ud_tmp[0].u64)
    elif type(val) == bool:
        emsval[0].type = TYPE_BOOLEAN
        emsval[0].value = ffi.cast('void *', val)
    elif type(val) == list:
        # newval = ffi.new('char []', bytes(str(val), 'utf-8'))
        newval = ffi.new('char []', bytes(json.dumps(val), 'utf-8'))
        emsval[0].value = ffi.cast('void *', newval)
        emsval[0].length = len(newval)
        emsval[0].type = TYPE_JSON
        global_weakkeydict[emsval] = (emsval[0].length, emsval[0].type, emsval[0].value, newval)
    elif type(val) == dict:
        newval = ffi.new('char []', bytes(json.dumps(val), 'utf-8'))
        emsval[0].value = ffi.cast('void *', newval)
        emsval[0].length = len(newval)
        emsval[0].type = TYPE_JSON
        global_weakkeydict[emsval] = (emsval[0].length, emsval[0].type, emsval[0].value, newval)
    elif val is None:
        emsval[0].type = TYPE_UNDEFINED
        emsval[0].value = ffi.cast('void *', 0xdeadbeef)
    else:
        print("EMS ERROR - unknown type of value:", type(val), val)
        return None
    return emsval


# ==========================================================================================


class EMSarray(object):
    def __init__(self,
                 nElements=1,  # Required: Maximum number of elements in array
                 heapSize=0,  # Optional, default=0: Space, in bytes, for strings, maps, objects, etc.
                 mlock=0,  # Optional, 0-100% of EMS memory into RAM
                 useMap=False,  # Optional, default=false: Use a map from keys to indexes
                 useExisting=False,  # Optional, default=false: Preserve data if a file already exists
                 persist=True,  # Optional, default=true: Preserve the file after threads exit
                 doDataFill=False,  # Optional, default=false: Data values should be initialized
                 dataFill=None,  # Optional, default=false: Value to initialize data to
                 dimStride=[]  # Stride factors for each dimension of multidimensional arrays
                 ):
        global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
        # self.ems = ems
        self.nElements = nElements
        self.heapSize = heapSize
        self.mlock = mlock
        self.useMap = useMap
        self.useExisting = useExisting
        self.persist = persist
        self.doSetFEtags = True
        self.setFEtags = 'full'
        self.doDataFill = doDataFill
        self.dataFill = dataFill
        self.dimStride = dimStride
        self.dimensions = None
        self.filename = None
        self.mmapID = -1
        self.mlock = 1
        self.doSetFEtags = False # Optional, initialize full/empty tags
        self.setFEtagsFull = True # Optional, used only if doSetFEtags is true

        # set any attributes here - before initialisation
        # these remain as normal attributes
        # self.attribute = attribute
        # dict.__init__(self, {})
        self.__initialised = True
        # after initialisation, setting attributes is the same as setting an item

    def destroy(self, unlink_file):
        """Release all resources associated with an EMS memory region"""
        global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
        barrier()
        if myID == 0:
            libems.EMSdestroy(self.mmapID, unlink_file)
        barrier()

    def _returnData(self, emsval):
        global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
        if emsval[0].type == TYPE_STRING:
            return ffi.string(ffi.cast('char *', emsval[0].value)).decode('utf-8')
        elif emsval[0].type == TYPE_JSON:
            str = ffi.string(ffi.cast('char *', emsval[0].value)).decode('utf-8')
            tmp_str = "{\"data\":" + str + "}"
            return json.loads(tmp_str)['data']
        elif emsval[0].type == TYPE_INTEGER:
            return int(ffi.cast('int64_t', emsval[0].value))
        elif emsval[0].type == TYPE_FLOAT:
            ud_tmp = ffi.new('ulong_double *')
            ud_tmp[0].u64 = ffi.cast('uint64_t', emsval[0].value)
            return ud_tmp[0].d
        elif emsval[0].type == TYPE_BOOLEAN:
            return bool(int(ffi.cast('uint64_t', emsval[0].value)))
        elif emsval[0].type == TYPE_UNDEFINED:
            return None
        else:
            print("EMS ERROR - unknown type of value:", type(emsval), emsval)
            return None

    def sync(self):
        """Synchronize memory with storage"""
        global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
        return libems.EMSsync(self.mmapID)

    def index2key(self, index):
        global myID, libems, EMSmmapID, _regionN, pinThreads, domainName, inParallelContext, tasks, nThreads
        key = _new_EMSval(None)
        assert libems.EMSindex2key(self.mmapID, index, key)
        return self._returnData(key)

    # ==================================================================
    #  Wrappers around Stacks and Queues
    def push(self, value):
        nativeValue = _new_EMSval(value)
        return libems.EMSpush(self.mmapID, nativeValue)

    def pop(self):
        val = _new_EMSval(None)
        libems.EMSpop(self.mmapID, val)
        return self._returnData(val)

    def dequeue(self):
        val = _new_EMSval(None)
        libems.EMSdequeue(self.mmapID, val)
        return self._returnData(val)

    def enqueue(self, value):
        nativeValue = _new_EMSval(value)
        return libems.EMSenqueue(self.mmapID, nativeValue)

    # ==================================================================
    #  Wrappers around Primitive AMOs
    #  Translate EMS maps and multi-dimensional array indexes/keys
    #  into EMS linear addresses
    #  Apparently it is illegal to pass a native function as an argument
    def write(self, indexes, value):
        nativeIndex = _new_EMSval(self._idx(indexes))
        nativeValue = _new_EMSval(value)
        libems.EMSwrite(self.mmapID, nativeIndex, nativeValue)
        return (self.mmapID, nativeIndex, nativeValue)

    def writeEF(self, indexes, value):
        nativeIndex = _new_EMSval(self._idx(indexes))
        nativeValue = _new_EMSval(value)
        libems.EMSwriteEF(self.mmapID, nativeIndex, nativeValue)
        return (self.mmapID, nativeIndex, nativeValue)

    def writeXF(self, indexes, value):
        nativeIndex = _new_EMSval(self._idx(indexes))
        nativeValue = _new_EMSval(value)
        libems.EMSwriteXF(self.mmapID, nativeIndex, nativeValue)
        return (self.mmapID, nativeIndex, nativeValue)

    def writeXE(self, indexes, value):
        nativeIndex = _new_EMSval(self._idx(indexes))
        nativeValue = _new_EMSval(value)
        libems.EMSwriteXE(self.mmapID, nativeIndex, nativeValue)
        return (self.mmapID, nativeIndex, nativeValue)

    # ---------------------------------------------------
    def read(self, indexes):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        val = _new_EMSval(None)
        libems.EMSread(self.mmapID, emsnativeidx, val)
        return self._returnData(val)

    def readFE(self, indexes):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        val = _new_EMSval(None)
        libems.EMSreadFE(self.mmapID, emsnativeidx, val)
        return self._returnData(val)

    def readFF(self, indexes):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        val = _new_EMSval(None)
        libems.EMSreadFF(self.mmapID, emsnativeidx, val)
        return self._returnData(val)

    def readRW(self, indexes):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        val = _new_EMSval(None)
        libems.EMSreadRW(self.mmapID, emsnativeidx, val)
        return self._returnData(val)

    def releaseRW(self, indexes):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        return libems.EMSreleaseRW(self.mmapID, emsnativeidx)

    def setTag(self, indexes, fe):
        emsnativeidx = _new_EMSval(self._idx(indexes))
        return libems.EMSsetTag(self.mmapID, emsnativeidx, (fe == 'full'))

    # ---------------------------------------------------
    # Atomic RMW
    def faa(self, indexes, val):
        if type(val) == dict:
            print("EMSfaa ERROR: Cannot add an object to something")
            return val
        else:
            ems_nativeidx = _new_EMSval(self._idx(indexes))
            ems_val = _new_EMSval(val)
            ems_retval = _new_EMSval(None)
            assert libems.EMSfaa(self.mmapID, ems_nativeidx, ems_val, ems_retval)
            return self._returnData(ems_retval)

    def cas(self, indexes, oldVal, newVal):
        if type(oldVal) == dict:
            print("EMScas ERROR: Cannot compare objects, only JSON primitives")
            return None
        else:
            ems_nativeidx = _new_EMSval(self._idx(indexes))
            ems_oldval = _new_EMSval(oldVal)
            ems_newval = _new_EMSval(newVal)
            ems_retval = _new_EMSval(None)
            libems.EMScas(self.mmapID, ems_nativeidx, ems_oldval, ems_newval, ems_retval)
            return self._returnData(ems_retval)

    def _idx(self, indexes):
        idx = 0
        if type(indexes) == list:  # Is a Multidimension array: [x,y,z]
            rank = 0
            for index in indexes:
                idx += index * self.dimStride[rank]
                rank += 1
        else:
            if not type(indexes) == int  and  not self.useMap:  #  If no map, only use integers
                print("EMS ERROR: Non-integer index used, but EMS memory was not configured to use a map (useMap)",
                      indexes, type(indexes), self.useMap)
                idx = -1
            else:  # Is a mappable intrinsic type
                idx = indexes
        return idx

    def __getattr__(self, attr):
        return self.read(attr)

    def __setattr__(self, item, value):
        if not '_EMSarray__initialised' in self.__dict__  or  item in self.__dict__:
            # Ignore object initialization and normal attribute access
            return dict.__setattr__(self, item, value)
        else:
            return self.write(item, value)

    def __getitem__(self, item):
        return EMSelement(self, item)

    def __setitem__(self, key, value):
        return self.write(key, value)


# =============================================================================================

class EMSelement(object):
    def __init__(self, ems_array, index):
        self._ems_array = ems_array
        self._index = index

    def write(self, value):
        return self._ems_array.write(self._index, value)

    def writeXE(self, value):
        return self._ems_array.writeXE(self._index, value)

    def writeXF(self, value):
        return self._ems_array.writeXF(self._index, value)

    def writeEF(self, value):
        return self._ems_array.writeEF(self._index, value)

    def read(self):
        return self._ems_array.read(self._index)

    def readFF(self):
        return self._ems_array.readFF(self._index)

    def readFE(self):
        return self._ems_array.readFE(self._index)

    def releaseRW(self):
        return self._ems_array.releaseRW(self._index)

    def readRW(self):
        return self._ems_array.readRW(self._index)

    def setTag(self, fe):
        return self._ems_array.setTag(self._index, fe)

    def faa(self, value):
        return self._ems_array.faa(self._index, value)

    def cas(self, oldVal, newVal):
        return self._ems_array.cas(self._index, oldVal, newVal)
