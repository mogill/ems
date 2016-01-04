/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.0.0   |
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
//  Parallel Loop -- context initialization
//
void EMSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int32_t *bufInt32 = (int32_t *) emsBuf;

    int start = 0;
    int end = 0;
    int minChunk = 0;

    if (info.Length() != 4) {
        Nan::ThrowError("EMSloopInit: Wrong number of args");
        return;
    }

    start = info[0]->ToInteger()->Value();
    end = info[1]->ToInteger()->Value();
    const char *schedule = JS_ARG_TO_CSTR(info[2]);
    minChunk = info[3]->ToInteger()->Value();

    bufInt32[EMS_LOOP_IDX] = start;
    bufInt32[EMS_LOOP_START] = start;
    bufInt32[EMS_LOOP_END] = end;
    if (strcmp(schedule, "guided") == 0) {
        bufInt32[EMS_LOOP_CHUNKSZ] = ((end - start) / 2) / bufInt32[EMS_CB_NTHREADS];
        if (bufInt32[EMS_LOOP_CHUNKSZ] < minChunk) bufInt32[EMS_LOOP_CHUNKSZ] = minChunk;
        bufInt32[EMS_LOOP_MINCHUNK] = minChunk;
        bufInt32[EMS_LOOP_SCHED] = EMS_SCHED_GUIDED;
    } else {
        if (strcmp(schedule, "dynamic") == 0) {
            bufInt32[EMS_LOOP_CHUNKSZ] = 1;
            bufInt32[EMS_LOOP_MINCHUNK] = 1;
            bufInt32[EMS_LOOP_SCHED] = EMS_SCHED_DYNAMIC;
        } else {
            Nan::ThrowError("EMSloopInit: Unknown schedule type");
            return;
        }
    }
    info.GetReturnValue().Set(Nan::New(EMS_LOOP_IDX));
}


//==================================================================
//  Determine the current block of iterations to assign to an
//  an idle thread
//  JQM TODO BUG  -- convert to 64 bit using  fe tags
//
void EMSloopChunk(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int32_t *bufInt32 = (int32_t *) emsBuf;

    int chunkSize = bufInt32[EMS_LOOP_CHUNKSZ];
    int start = __sync_fetch_and_add(&(bufInt32[EMS_LOOP_IDX]), chunkSize);
    int end = start + chunkSize;

    if (start > bufInt32[EMS_LOOP_END]) end = 0;
    if (end > bufInt32[EMS_LOOP_END]) end = bufInt32[EMS_LOOP_END];

    v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
    retObj->Set(Nan::New("start").ToLocalChecked(), Nan::New(start));
    retObj->Set(Nan::New("end").ToLocalChecked(), Nan::New(end));

    if (bufInt32[EMS_LOOP_SCHED] == EMS_SCHED_GUIDED) {
        //  Compute the size of the chunk the next thread should use
        int newSz = ((bufInt32[EMS_LOOP_END] - start) / 2) / bufInt32[EMS_CB_NTHREADS];
        if (newSz < bufInt32[EMS_LOOP_MINCHUNK]) newSz = bufInt32[EMS_LOOP_MINCHUNK];
        bufInt32[EMS_LOOP_CHUNKSZ] = newSz;
    }
    info.GetReturnValue().Set(retObj);
}

