/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.4.0   |
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
#include "nodejs.h"
#include "../src/ems.h"


void NodeJScriticalEnter(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    int timeout = INT32_MAX;
    if(info.Length() == 1) {
        timeout = info[0]->ToInteger()->Value();
    } else {
        Nan::ThrowError("NodeJScriticalEner: invalid or missing timeout duration");
        return;
    }
    int timeRemaining = EMScriticalEnter(mmapID, timeout);
    if (timeRemaining <= 0) {
        Nan::ThrowError("NodeJScriticalEnter: Unable to enter critical region before timeout");
    } else {
        info.GetReturnValue().Set(Nan::New(true));
    }
}


void NodeJScriticalExit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    bool success = EMScriticalExit(mmapID);
    if (!success) {
        Nan::ThrowError("NodeJScriticalExit: critical region mutex lost while locked?!");
    } else {
        info.GetReturnValue().Set(Nan::New(success));
    }
}


void NodeJSbarrier(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    int timeout = INT32_MAX;
    if(info.Length() == 1) {
        timeout = info[0]->ToInteger()->Value();
    } else {
        Nan::ThrowError("NodeJSbarrier: invalid or missing timeout duration");
        return;
    }
    int timeRemaining = EMSbarrier(mmapID, timeout);
    if (timeRemaining <= 0) {
        Nan::ThrowError("NodeJSbarrer: Failed to sync at barrier");
    } else {
        info.GetReturnValue().Set(Nan::New(true));
    }
}


void NodeJSsingleTask(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    bool did_work = EMSsingleTask(mmapID);
    info.GetReturnValue().Set(Nan::New(did_work));
}


void NodeJScas(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_KEY_DECL(NodeJScas, );
    NODE_INFO_DECL(NodeJScas, , oldValString, oldVal, 3, 1, false);
    NODE_INFO_DECL(NodeJScas, , newValString, newVal, 3, 2, false);
    bool success = EMScas(mmapID, &key, &oldVal, &newVal, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJScas: Failed to get a valid old value");
        return;
    }
    EMS_TO_V8_RETURNVALUE(returnValue, info, true);
}


void NodeJSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    bool stringIsJSON = false;
    EMSvalueType returnValue;
    NODE_KEY_DECL(NodeJSfaa, );
    NODE_VALUE_DECL(NodeJSfaa, );
    bool success = EMSfaa(mmapID, &key, &value, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSfaa: Failed to get a valid old value");
        return;
    }
    EMS_TO_V8_RETURNVALUE(returnValue, info, true);
}


void NodeJSpush(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_PUSH_ENQUEUE(NodeJSpush, EMSpush);
}


void NodeJSpop(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_MMAPID_DECL();
    bool success = EMSpop(mmapID, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSpop: Failed to pop a value off the stack");
        return;
    }
    EMS_TO_V8_RETURNVALUE(returnValue, info, true);
}


void NodeJSenqueue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_PUSH_ENQUEUE(NodeJSenqueue, EMSenqueue);
}


void NodeJSdequeue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_MMAPID_DECL();
    bool success = EMSdequeue(mmapID, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSdequeue: Failed to dequeue a value");
        return;
    }
    EMS_TO_V8_RETURNVALUE(returnValue, info, true);
}


void NodeJSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    if (info.Length() != 4) {
        Nan::ThrowError("NodeJSloopInit: Wrong number of args");
        return;
    }

    int32_t start = (int32_t) info[0]->ToInteger()->Value();
    int32_t end = (int32_t)info[1]->ToInteger()->Value();
    int schedule_mode;
    std::string sched_string(*Nan::Utf8String(info[2]));
    if (sched_string.compare("guided") == 0) {
        schedule_mode = EMS_SCHED_GUIDED;
    } else {
        if (sched_string.compare("dynamic") == 0) {
            schedule_mode = EMS_SCHED_DYNAMIC;
        } else {
            Nan::ThrowError("NodeJSloopInit: Unknown/invalid schedule mode");
            return;
        }
    }
    int32_t minChunk = (int32_t) info[3]->ToInteger()->Value();

    bool success = EMSloopInit(mmapID, start, end, minChunk, schedule_mode);
    if (!success) {
        Nan::ThrowError("NodeJSloopInit: Unknown failure to initalize loop");
    } else {
        info.GetReturnValue().Set(Nan::New(success));
    }
}


void NodeJSloopChunk(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    if (info.Length() != 0) {
        Nan::ThrowError("NodeJSloopChunk: Arguments provided, but none accepted");
        return;
    }
    int32_t start, end;
    EMSloopChunk(mmapID, &start, &end);  // Unusued return value

    v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
    retObj->Set(Nan::New("start").ToLocalChecked(), Nan::New(start));
    retObj->Set(Nan::New("end").ToLocalChecked(), Nan::New(end));
    info.GetReturnValue().Set(retObj);
}


//--------------------------------------------------------------
void NodeJSread(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_READ(NodeJSread, EMSread, ); /* Bogus Missing argument warning */
}


void NodeJSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_READ(NodeJSreadFE, EMSreadFE, ); /* Bogus Missing argument warning */
}


void NodeJSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue;
    NODE_READ(NodeJSreadFF, EMSreadFF, ); /* Bogus Missing argument warning */
}


void NodeJSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info)  {
    EMSvalueType returnValue;
    NODE_READ(NodeJSreadRW, EMSreadRW, ); /* Bogus Missing argument warning */
}


void NodeJSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_KEY_DECL(NodeJSreleaseRW, );
    int nReadersActive = EMSreleaseRW(mmapID, &key);
    if (nReadersActive < 0) {
        Nan::ThrowError("NodeJSreleaseRW: Invalid index for key, or index key in bad state");
    } else {
        info.GetReturnValue().Set(Nan::New(nReadersActive));
    }
}

// ====================================================

void NodeJSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_WRITE(NodeJSwrite, EMSwrite);
}


void NodeJSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_WRITE(NodeJSwriteEF, EMSwriteEF);
}


void NodeJSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_WRITE(NodeJSwriteXF, EMSwriteXF);
}


void NodeJSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_WRITE(NodeJSwriteXE, EMSwriteXE);
}


void NodeJSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_KEY_DECL(NodeJSsetTag, );
    bool is_full = info[1]->ToBoolean()->Value();
    bool success = EMSsetTag(mmapID, &key, is_full);
    if(success) {
        info.GetReturnValue().Set(Nan::New(true));
    } else {
        Nan::ThrowError("NodeJSsetTag: Invalid key, unable to set tag");
    }
}


void NodeJSsync(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    fprintf(stderr, "NodeJSsync: WARNING: sync is not implemented\n");
    bool success = EMSsync(mmapID);
    info.GetReturnValue().Set(Nan::New(success));
}


void NodeJSindex2key(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    EMSvalueType key;
    int idx = (int32_t)  info[0]->ToInteger()->Value();  // TODO: This is just 32bit, should be size_t
    if( !EMSindex2key(mmapID, idx, &key) ) {
        fprintf(stderr, "NodeJSindex2key: Error converting index to key\n");
    }
    EMS_TO_V8_RETURNVALUE(key, info, false);
}


void NodeJSdestroy(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL();
    bool do_unlink = info[0]->ToBoolean()->Value();
    bool success = EMSdestroy(mmapID, do_unlink);
    if (success) {
        info.GetReturnValue().Set(Nan::New(true));
    } else {
        Nan::ThrowError("NodeJSdestroy: Failed to destroy EMS array");
    }
};


//==================================================================
//  EMS Entry Point:   Allocate and initialize the EMS domain memory
void NodeJSinitialize(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    if (info.Length() != 15) {
        Nan::ThrowError("NodeJSinitialize: Incorrect number of arguments");
        return;
    }
    EMSvalueType fillData;
    std::string  fillString;

    //  Parse all the arguments
    int64_t nElements  = info[0]->ToInteger()->Value();
    int64_t heapSize   = info[1]->ToInteger()->Value();
    bool useMap        = info[2]->ToBoolean()->Value();
    std::string filestring(*Nan::Utf8String(info[3]));
    const char *filename = filestring.c_str();
    bool persist       = info[4]->ToBoolean()->Value();
    bool useExisting   = info[5]->ToBoolean()->Value();
    bool doDataFill    = info[6]->ToBoolean()->Value();
    bool fillIsJSON    = info[7]->ToBoolean()->Value();
    // 8 = Data Fill type TBD during fill
    bool doSetFEtags   = info[9]->ToBoolean()->Value();
    bool setFEtags     = info[10]->ToBoolean()->Value();
    EMSmyID            = (int) info[11]->ToInteger()->Value();
    bool pinThreads    = info[12]->ToBoolean()->Value();
    int64_t nThreads   = info[13]->ToInteger()->Value();
    int64_t pctMLock   = info[14]->ToInteger()->Value();

    if(doDataFill) {
        NAN_OBJ_TO_EMS_VAL(NodeJSinitialize, info[8], fillData, fillString, fillIsJSON);
    }

    int emsBufN = EMSinitialize(nElements,   // 0
                                heapSize,    // 1
                                useMap,      // 2
                                filename,    // 3
                                persist,     // 4
                                useExisting, // 5
                                doDataFill,  // 6 Data Fill type TBD during fill
                                fillIsJSON,  // 7
                                &fillData,   // 8
                                doSetFEtags, // 9
                                setFEtags,   // 10
                                EMSmyID,     // 11
                                pinThreads,  // 12
                                nThreads,    // 13
                                pctMLock);   // 14

    if(emsBufN < 0) {
        Nan::ThrowError("NodeJSinitialize: failed to initialize EMS array");
        return;
    }

    // ========================================================================================
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();
    obj->Set(Nan::New("mmapID").ToLocalChecked(), Nan::New(emsBufN));
    ADD_FUNC_TO_V8_OBJ(obj, "faa", NodeJSfaa);
    ADD_FUNC_TO_V8_OBJ(obj, "cas", NodeJScas);
    ADD_FUNC_TO_V8_OBJ(obj, "read", NodeJSread);
    ADD_FUNC_TO_V8_OBJ(obj, "write", NodeJSwrite);
    ADD_FUNC_TO_V8_OBJ(obj, "readRW", NodeJSreadRW);
    ADD_FUNC_TO_V8_OBJ(obj, "releaseRW", NodeJSreleaseRW);
    ADD_FUNC_TO_V8_OBJ(obj, "readFE", NodeJSreadFE);
    ADD_FUNC_TO_V8_OBJ(obj, "readFF", NodeJSreadFF);
    ADD_FUNC_TO_V8_OBJ(obj, "setTag", NodeJSsetTag);
    ADD_FUNC_TO_V8_OBJ(obj, "writeEF", NodeJSwriteEF);
    ADD_FUNC_TO_V8_OBJ(obj, "writeXF", NodeJSwriteXF);
    ADD_FUNC_TO_V8_OBJ(obj, "writeXE", NodeJSwriteXE);
    ADD_FUNC_TO_V8_OBJ(obj, "push", NodeJSpush);
    ADD_FUNC_TO_V8_OBJ(obj, "pop", NodeJSpop);
    ADD_FUNC_TO_V8_OBJ(obj, "enqueue", NodeJSenqueue);
    ADD_FUNC_TO_V8_OBJ(obj, "dequeue", NodeJSdequeue);
    ADD_FUNC_TO_V8_OBJ(obj, "sync", NodeJSsync);
    ADD_FUNC_TO_V8_OBJ(obj, "index2key", NodeJSindex2key);
    ADD_FUNC_TO_V8_OBJ(obj, "destroy", NodeJSdestroy);
    info.GetReturnValue().Set(obj);
}


//---------------------------------------------------------------
static void RegisterModule(v8::Handle <v8::Object> target) {
    ADD_FUNC_TO_V8_OBJ(target, "initialize", NodeJSinitialize);
    ADD_FUNC_TO_V8_OBJ(target, "barrier", NodeJSbarrier);
    ADD_FUNC_TO_V8_OBJ(target, "singleTask", NodeJSsingleTask);
    ADD_FUNC_TO_V8_OBJ(target, "criticalEnter", NodeJScriticalEnter);
    ADD_FUNC_TO_V8_OBJ(target, "criticalExit", NodeJScriticalExit);
    ADD_FUNC_TO_V8_OBJ(target, "loopInit", NodeJSloopInit);
    ADD_FUNC_TO_V8_OBJ(target, "loopChunk", NodeJSloopChunk);
}


NODE_MODULE(ems, RegisterModule);
