/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.5.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2017, Jace A Mogill.  All rights reserved.              |
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
#include "../src/ems_types.h"

/**
 * Convert a NAN object to an EMS object stored on the stack
 * @param nanValue Source NAN object
 * @param emsValue Target EMS object
 * @param stringIsJSON You know who
 * @return True if successful converting
 */
#define NAN_OBJ_2_EMS_OBJ(nanValue, emsValue, stringIsJSON) {           \
        emsValue.type = NanObjToEMStype(nanValue, stringIsJSON);        \
        switch (emsValue.type) {                                        \
        case EMS_TYPE_BOOLEAN:                                          \
            emsValue.value = (void *) nanValue->ToBoolean(v8::Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked()->Value();   \
            break;                                                      \
        case EMS_TYPE_INTEGER:                                          \
            emsValue.value = (void *) nanValue->ToInteger(v8::Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked()->Value();   \
            break;                                                      \
        case EMS_TYPE_FLOAT: {                                          \
            EMSulong_double alias = {.d = nanValue->ToNumber(v8::Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked()->Value()}; \
            emsValue.value = (void *) alias.u64;                        \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_BUFFER: {                                         \
            v8::Local<v8::Object> buffer = nanValue -> ToObject();          \
            /*Nan::MaybeLocal<v8::Object> buffer = nanValue -> ToObject(v8::Isolate::GetCurrent()->GetCurrentContext()).ToLocalChecked();                  */ \
            if (!node::Buffer::HasInstance(buffer)) {                        \
                Nan::ThrowTypeError(QUOTE(__FUNCTION__) " ERROR: EMS_TYPE_BUFFER: Invalid buffer"); \
                return;                                                 \
            }                                                               \
            size_t byteLength = node::Buffer::Length(buffer);               \
            emsValue.length = byteLength;                                   \
            emsValue.value = alloca(byteLength);                            \
            if (!emsValue.value) {                                     \
                Nan::ThrowTypeError(QUOTE(__FUNCTION__) " ERROR: EMS_TYPE_BUFFER: Unable to allocate scratch memory for serialized value"); \
                return;                                                 \
            }                                                           \
            memcpy(emsValue.value, node::Buffer::Data(buffer), byteLength); \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_JSON:                                             \
        case EMS_TYPE_STRING: {                                         \
            emsValue.length = strlen(*Nan::Utf8String(nanValue)) + 1;  /* +1 for trailing NULL */ \
            emsValue.value = alloca(emsValue.length);                   \
            if (!emsValue.value) {                                      \
                Nan::ThrowTypeError(QUOTE(__FUNCTION__) " ERROR: Unable to allocate scratch memory for serialized value"); \
                return;                                                 \
            }                                                           \
            memcpy(emsValue.value, *Nan::Utf8String(nanValue), emsValue.length); \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_UNDEFINED:                                        \
            emsValue.value = (void *) 0xbeeff00d;                       \
            break;                                                      \
        default:                                                        \
            Nan::ThrowTypeError(QUOTE(__FUNCTION__) " ERROR: Invalid value type"); \
            return;                                                     \
        }                                                               \
    }


static void inline
buffer_delete_callback(char* data, void* hint) {
    free(data);
}

/**
 * Convert an EMS object into a NAN object
 * @param emsValue Source EMS object
 * @param v8Value Target NAN object
 * @return True if successful
 */
static bool inline
ems2v8ReturnValue(EMSvalueType *emsValue,
                  Nan::ReturnValue<v8::Value> v8Value) {
    switch(emsValue->type) {
        case EMS_TYPE_BOOLEAN: {
            bool retBool = (bool) emsValue->value;
            v8Value.Set(
                Nan::New(retBool)
            );
        }
            break;
        case EMS_TYPE_INTEGER: {
            int32_t retInt = ((int64_t) emsValue->value) & 0xffffffff;  /* TODO: Bug -- only 32 bits of 64? */
            v8Value.Set(
                Nan::New(retInt)
            );
        }
            break;
        case EMS_TYPE_FLOAT: {
            EMSulong_double alias = {.u64 = (uint64_t) emsValue->value};
            v8Value.Set(
                Nan::New(alias.d)
            );
        }
            break;
        case EMS_TYPE_BUFFER: {
            // Nan::MaybeLocal<v8::Object> buffer = Nan::CopyBuffer(
            v8Value.Set(
                Nan::CopyBuffer( /* implicit memcpy */
                    (char *) emsValue->value,
                    emsValue->length
                ).ToLocalChecked()
            );
            break;
        }
        case EMS_TYPE_JSON: {
            v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
            retObj->Set(Nan::New("data").ToLocalChecked(),
                        Nan::New((char *) emsValue->value).ToLocalChecked());
            v8Value.Set(retObj);
        }
            break;
        case EMS_TYPE_STRING: {
            v8Value.Set(
                Nan::New((char *) emsValue->value).ToLocalChecked()
            );
        }
            break;
        case EMS_TYPE_UNDEFINED: {
            v8Value.Set(
                Nan::Undefined()
            );
        }
            break;
        default:
            Nan::ThrowTypeError("ems2v8ReturnValue - ERROR: Invalid type of data read from memory");
            return false;
    }
    return true;
}


void NodeJScriticalEnter(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    int64_t timeout;
    if(info.Length() == 1) {
        timeout = info[0]->ToInteger()->Value();
    } else {
        Nan::ThrowError("NodeJScriticalEner: invalid or missing timeout duration");
        return;
    }
    int timeRemaining = EMScriticalEnter(mmapID, (int) timeout);
    if (timeRemaining <= 0) {
        Nan::ThrowError("NodeJScriticalEnter: Unable to enter critical region before timeout");
    } else {
        info.GetReturnValue().Set(Nan::New(timeRemaining));
    }
}


void NodeJScriticalExit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    bool success = EMScriticalExit(mmapID);
    if (!success) {
        Nan::ThrowError("NodeJScriticalExit: critical region mutex lost while locked?!");
    } else {
        info.GetReturnValue().Set(Nan::New(success));
    }
}


void NodeJSbarrier(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    int timeout;
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
        info.GetReturnValue().Set(Nan::New(timeRemaining));
    }
}


void NodeJSsingleTask(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    bool did_work = EMSsingleTask(mmapID);
    info.GetReturnValue().Set(Nan::New(did_work));
}


void NodeJScas(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    EMSvalueType oldVal = EMS_VALUE_TYPE_INITIALIZER;
    EMSvalueType newVal = EMS_VALUE_TYPE_INITIALIZER;
    if (info.Length() != 3) {
        Nan::ThrowError(SOURCE_LOCATION ": Called with wrong number of args.");
        return;
    }
    NAN_OBJ_2_EMS_OBJ(info[1],  oldVal, false);
    NAN_OBJ_2_EMS_OBJ(info[2],  newVal, false);
    if (!EMScas(mmapID, &key, &oldVal, &newVal, &returnValue)) {
        Nan::ThrowError("NodeJScas: Failed to get a valid old value");
        return;
    }
    ems2v8ReturnValue(&returnValue, info.GetReturnValue());
}


void NodeJSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool success = EMSfaa(mmapID, &key, &value, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSfaa: Failed to get a valid old value");
        return;
    }
    ems2v8ReturnValue(&returnValue, info.GetReturnValue());
}


void NodeJSpush(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(0);
    int returnValue = EMSpush(mmapID, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSpop(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    NODE_MMAPID_DECL;
    bool success = EMSpop(mmapID, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSpop: Failed to pop a value off the stack");
        return;
    }
    ems2v8ReturnValue(&returnValue, info.GetReturnValue());
}


void NodeJSenqueue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(0);
    int returnValue = EMSenqueue(mmapID, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSdequeue(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    NODE_MMAPID_DECL;
    bool success = EMSdequeue(mmapID, &returnValue);
    if (!success) {
        Nan::ThrowError("NodeJSdequeue: Failed to dequeue a value");
        return;
    }
    ems2v8ReturnValue(&returnValue, info.GetReturnValue());
}


void NodeJSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
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
    NODE_MMAPID_DECL;
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
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if(!EMSread(mmapID, &key, &returnValue)) {
        Nan::ThrowError(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        ems2v8ReturnValue(&returnValue, info.GetReturnValue());
    }
}


void NodeJSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if(!EMSreadFE(mmapID, &key, &returnValue)) {
        Nan::ThrowError(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        ems2v8ReturnValue(&returnValue, info.GetReturnValue());
    }
}


void NodeJSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if(!EMSreadFF(mmapID, &key, &returnValue)) {
        Nan::ThrowError(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        ems2v8ReturnValue(&returnValue, info.GetReturnValue());
    }
}


void NodeJSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info)  {
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if(!EMSreadRW(mmapID, &key, &returnValue)) {
        Nan::ThrowError(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        ems2v8ReturnValue(&returnValue, info.GetReturnValue());
    }
}


void NodeJSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    int nReadersActive = EMSreleaseRW(mmapID, &key);
    if (nReadersActive < 0) {
        Nan::ThrowError("NodeJSreleaseRW: Invalid index for key, or index key in bad state");
    } else {
        info.GetReturnValue().Set(Nan::New(nReadersActive));
    }
}

// ====================================================

void NodeJSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwrite(mmapID, &key, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteEF(mmapID, &key, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteXF(mmapID, &key, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteXE(mmapID, &key, &value);
    info.GetReturnValue().Set(Nan::New(returnValue));
}


void NodeJSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1); // Bool -- is full
    bool success = EMSsetTag(mmapID, &key, (bool)value.value);
    if(success) {
        info.GetReturnValue().Set(Nan::New(true));
    } else {
        Nan::ThrowError("NodeJSsetTag: Invalid key, unable to set tag");
    }
}


void NodeJSsync(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    fprintf(stderr, "NodeJSsync: WARNING: sync is not implemented\n");
    bool success = EMSsync(mmapID);
    info.GetReturnValue().Set(Nan::New(success));
}


void NodeJSindex2key(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
    EMSvalueType key = EMS_VALUE_TYPE_INITIALIZER;
    int idx = (int32_t)  info[0]->ToInteger()->Value();  // TODO: This is just 32bit, should be size_t
    if( !EMSindex2key(mmapID, idx, &key) ) {
        fprintf(stderr, "NodeJSindex2key: Error converting index (%d) to key\n", idx);
    }
    ems2v8ReturnValue(&key, info.GetReturnValue());
}


void NodeJSdestroy(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    NODE_MMAPID_DECL;
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
    EMSvalueType fillData = EMS_VALUE_TYPE_INITIALIZER;

    //  Parse all the arguments
    int64_t nElements  = info[0]->ToInteger()->Value();
    size_t  heapSize   = (size_t) info[1]->ToInteger()->Value();
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
    int32_t nThreads   = (int32_t) info[13]->ToInteger()->Value();
    int32_t pctMLock   = (int32_t) info[14]->ToInteger()->Value();

    if(doDataFill) {
        NAN_OBJ_2_EMS_OBJ(info[8], fillData, fillIsJSON);
        // if (doDataFill  &&  (fillData.type == EMS_TYPE_BUFFER)) {
        if (fillData.type == EMS_TYPE_BUFFER) {
            // Copy the default values to the heap because nanObj2EMSval copies them to the stack
            void *valueOnStack = fillData.value;
            fillData.value = malloc(fillData.length);
            if (fillData.value == NULL) {
                Nan::ThrowError("NodeJSinitialize: failed to allocate the buffer default value's storage on the heap");
                return;
            }
            memcpy(fillData.value, valueOnStack, fillData.length);
        }
        // else if (doDataFill  &&  (fillData.type == EMS_TYPE_JSON  ||  fillData.type == EMS_TYPE_STRING)) {
        else if (fillData.type == EMS_TYPE_JSON  ||  fillData.type == EMS_TYPE_STRING) {
            // Copy the default values to the heap because nanObj2EMSval copies them to the stack
            void *valueOnStack = fillData.value;
            fillData.value = malloc(fillData.length + 1);
            if (fillData.value == NULL) {
                Nan::ThrowError("NodeJSinitialize: failed to allocate the default value's storage on the heap");
                return;
            }
            memcpy(fillData.value, valueOnStack, fillData.length + 1);
        }
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
