/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.3.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2016, Jace A Mogill.  All rights reserved.              |
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
 +-----------------------------------------------------------------------------*/
#include "ems.h"


//==================================================================
//  Execute Once -- Single execution
//  OpenMP style, only first thread executes, remaining skip
bool EMSsingleTask(int mmapID) {
    void *emsBuf = emsBufs[mmapID];
    int32_t *bufInt32 = (int32_t *) emsBuf;

    // Increment the tally of threads that have reached this statement
    int retval = __sync_fetch_and_add(&bufInt32[EMS_CB_SINGLE], 1);

    //  If all threads have passed through the counter, reset fo next time
    if (retval == bufInt32[EMS_CB_NTHREADS] - 1) {
        bufInt32[EMS_CB_SINGLE] = 0;
    }

    //  Return True if this thread was first to the counter, later
    //  threads return false
    return retval == 0;
}


//==================================================================
//  Critical Region Entry --  1 thread at a time passes this barrier
//
int EMScriticalEnter(int mmapID, int timeout) {
    RESET_NAP_TIME;
    void *emsBuf = emsBufs[mmapID];
    int32_t *bufInt32 = (int32_t *) emsBuf;

    // Acquire the mutual exclusion lock
    while (!__sync_bool_compare_and_swap(&(bufInt32[EMS_CB_CRITICAL]), EMS_TAG_FULL, EMS_TAG_EMPTY)
        && timeout > 0 ) {
        NANOSLEEP;
        timeout -= 1;
    }

    return timeout;
}


//==================================================================
//  Critical Region Exit
bool EMScriticalExit(int mmapID) {
    void *emsBuf = emsBufs[mmapID];
    int32_t *bufInt32 = (int32_t *) emsBuf;

    // Test the mutual exclusion lock wasn't somehow lost
    if (bufInt32[EMS_CB_CRITICAL] != EMS_TAG_EMPTY) {
        return false;
    }

    bufInt32[EMS_CB_CRITICAL] = EMS_TAG_FULL;
    return true;
}


//==================================================================
//  Phase Based Global Thread Barrier
int EMSbarrier(int mmapID, int timeout) {
    void *emsBuf = emsBufs[mmapID];
    int32_t *bufInt32 = (int32_t *) emsBuf;

    int barPhase = bufInt32[EMS_CB_BARPHASE];    // Determine current phase of barrier
    int retval = __sync_fetch_and_add(&bufInt32[EMS_CB_NBAR0 + barPhase], -1);
    if (retval < 0) {
        fprintf(stderr, "EMSbarrier: Race condition at barrier\n");
        return false;
    }

    if (retval == 1) {
        //  This thread was the last to reach the barrier,
        //  Reset the barrier count for this phase and graduate to the next phase
        bufInt32[EMS_CB_NBAR0 + barPhase] = bufInt32[EMS_CB_NTHREADS];
        bufInt32[EMS_CB_BARPHASE] = !barPhase;
    } else {
        //  Wait for the barrier phase to change, indicating the last thread arrived
        RESET_NAP_TIME;
        while (timeout > 0  &&  barPhase == bufInt32[EMS_CB_BARPHASE]) {
            NANOSLEEP;
            timeout -= 1;
        }
    }

    return timeout;
}
