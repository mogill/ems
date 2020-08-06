/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.6.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
 |  Copyright (c) 2015-2017, Jace A Mogill.  All rights reserved.              |
 |                                                                             |
 |  Updated to replace NAN with N-API                                          |
 |  Copyright (c) 2019 Aleksander J Budzynowski.                               |
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
 * Convert a NAPI object to an EMS object stored on the stack
 * @param napiValue Source Napi object
 * @param emsValue Target EMS object
 * @param stringIsJSON You know who
 * @return True if successful converting
 */
#define NAPI_OBJ_2_EMS_OBJ(napiValue, emsValue, stringIsJSON) {         \
        emsValue.type = NapiObjToEMStype(napiValue, stringIsJSON);      \
        switch (emsValue.type) {                                        \
        case EMS_TYPE_BOOLEAN: {                                        \
            bool tmp = napiValue.As<Napi::Boolean>();                   \
            emsValue.value = (void *) tmp;                              \
            break;                                                      \
        }                                                               \
        case EMS_TYPE_INTEGER: {                                        \
            int64_t tmp = napiValue.As<Napi::Number>();                 \
            emsValue.value = (void *) tmp;                              \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_FLOAT: {                                          \
            EMSulong_double alias = {.d = napiValue.As<Napi::Number>()};\
            emsValue.value = (void *) alias.u64;                        \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_JSON:                                             \
        case EMS_TYPE_STRING: {                                         \
            std::string s = napiValue.As<Napi::String>().Utf8Value();   \
            emsValue.length = s.length() + 1; /* +1 for trailing NUL */ \
            emsValue.value = alloca(emsValue.length);                   \
            if (!emsValue.value) {                                      \
                THROW_TYPE_ERROR(QUOTE(__FUNCTION__) " ERROR: Unable to allocate scratch memory for serialized value");\
            }                                                           \
            memcpy(emsValue.value, s.c_str(), emsValue.length);         \
        }                                                               \
            break;                                                      \
        case EMS_TYPE_UNDEFINED:                                        \
            emsValue.value = (void *) 0xbeeff00d;                       \
            break;                                                      \
        default:                                                        \
            THROW_TYPE_ERROR(QUOTE(__FUNCTION__) " ERROR: Invalid value type");\
        }                                                               \
    }


/**
 * Convert an EMS object into a Napi object
 * @param env Napi Env object
 * @param emsValue Source EMS object
 * @return converted value
 */
static Napi::Value inline
ems2napiReturnValue(Napi::Env env, EMSvalueType *emsValue) {
    switch(emsValue->type) {
        case EMS_TYPE_BOOLEAN: {
            return Napi::Value::From(env, (bool) emsValue->value);
        }
            break;
        case EMS_TYPE_INTEGER: {
            int32_t retInt = ((int64_t) emsValue->value) & 0xffffffff;  /* TODO: Bug -- only 32 bits of 64? */
            return Napi::Number::New(env, retInt);
        }
            break;
        case EMS_TYPE_FLOAT: {
            EMSulong_double alias = {.u64 = (uint64_t) emsValue->value};
            return Napi::Number::New(env, alias.d);
        }
            break;
        case EMS_TYPE_JSON: {
            Napi::Object retObj = Napi::Object::New(env);
            retObj.Set("data", Napi::String::New(env, (char *) emsValue->value));
            return retObj;
        }
            break;
        case EMS_TYPE_STRING: {
            return Napi::String::New(env, (char *) emsValue->value);
        }
            break;
        case EMS_TYPE_UNDEFINED: {
            return env.Undefined();
        }
            break;
        default:
            THROW_TYPE_ERROR("ems2napiReturnValue - ERROR: Invalid type of data read from memory");
    }
}


Napi::Value NodeJScriticalEnter(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    int64_t timeout;
    if (info.Length() == 1) {
        timeout = info[0].As<Napi::Number>();
    } else {
        THROW_ERROR("NodeJScriticalEner: invalid or missing timeout duration");
    }
    int timeRemaining = EMScriticalEnter(mmapID, (int) timeout);
    if (timeRemaining <= 0) {
        THROW_ERROR("NodeJScriticalEnter: Unable to enter critical region before timeout");
    } else {
        return Napi::Value::From(env, timeRemaining);
    }
}


Napi::Value NodeJScriticalExit(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    bool success = EMScriticalExit(mmapID);
    if (!success) {
        THROW_ERROR("NodeJScriticalExit: critical region mutex lost while locked?!");
    } else {
        return Napi::Value::From(env, success);
    }
}


Napi::Value NodeJSbarrier(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    int timeout;
    if (info.Length() == 1) {
        timeout = info[0].As<Napi::Number>();
    } else {
        THROW_ERROR("NodeJSbarrier: invalid or missing timeout duration");
    }
    int timeRemaining = EMSbarrier(mmapID, timeout);
    if (timeRemaining <= 0) {
        THROW_ERROR("NodeJSbarrer: Failed to sync at barrier");
    } else {
        return Napi::Value::From(env, timeRemaining);
    }
}


Napi::Value NodeJSsingleTask(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    bool did_work = EMSsingleTask(mmapID);
    return Napi::Boolean::New(env, did_work);
}


Napi::Value NodeJScas(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    EMSvalueType oldVal = EMS_VALUE_TYPE_INITIALIZER;
    EMSvalueType newVal = EMS_VALUE_TYPE_INITIALIZER;
    if (info.Length() != 3) {
        THROW_ERROR(SOURCE_LOCATION ": Called with wrong number of args.");
    }
    NAPI_OBJ_2_EMS_OBJ(info[1],  oldVal, false);
    NAPI_OBJ_2_EMS_OBJ(info[2],  newVal, false);
    if (!EMScas(mmapID, &key, &oldVal, &newVal, &returnValue)) {
        THROW_ERROR("NodeJScas: Failed to get a valid old value");
    }
    return ems2napiReturnValue(env, &returnValue);
}


Napi::Value NodeJSfaa(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool success = EMSfaa(mmapID, &key, &value, &returnValue);
    if (!success) {
        THROW_ERROR("NodeJSfaa: Failed to get a valid old value");
    }
    return ems2napiReturnValue(env, &returnValue);
}


Napi::Value NodeJSpush(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(0);
    int returnValue = EMSpush(mmapID, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSpop(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    NODE_MMAPID_DECL;
    bool success = EMSpop(mmapID, &returnValue);
    if (!success) {
        THROW_ERROR("NodeJSpop: Failed to pop a value off the stack");
    }
    return ems2napiReturnValue(env, &returnValue);
}


Napi::Value NodeJSenqueue(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(0);
    int returnValue = EMSenqueue(mmapID, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSdequeue(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    NODE_MMAPID_DECL;
    bool success = EMSdequeue(mmapID, &returnValue);
    if (!success) {
        THROW_ERROR("NodeJSdequeue: Failed to dequeue a value");
    }
    return ems2napiReturnValue(env, &returnValue);
}


Napi::Value NodeJSloopInit(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    if (info.Length() != 4) {
        THROW_ERROR("NodeJSloopInit: Wrong number of args");
    }

    int32_t start = (int32_t) info[0].As<Napi::Number>();
    int32_t end = (int32_t) info[1].As<Napi::Number>();
    int schedule_mode;
    std::string sched_string = info[2].As<Napi::String>().Utf8Value();
    if (sched_string.compare("guided") == 0) {
        schedule_mode = EMS_SCHED_GUIDED;
    } else {
        if (sched_string.compare("dynamic") == 0) {
            schedule_mode = EMS_SCHED_DYNAMIC;
        } else {
            THROW_ERROR("NodeJSloopInit: Unknown/invalid schedule mode");
        }
    }
    int32_t minChunk = (int32_t) info[3].As<Napi::Number>();

    bool success = EMSloopInit(mmapID, start, end, minChunk, schedule_mode);
    if (!success) {
        THROW_ERROR("NodeJSloopInit: Unknown failure to initalize loop");
    } else {
        return Napi::Value::From(env, success);
    }
}


Napi::Value NodeJSloopChunk(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    if (info.Length() != 0) {
        THROW_ERROR("NodeJSloopChunk: Arguments provided, but none accepted");
    }
    int32_t start, end;
    EMSloopChunk(mmapID, &start, &end);  // Unusued return value

    Napi::Object retObj = Napi::Object::New(env);
    retObj.Set("start", Napi::Value::From(env, start));
    retObj.Set("end", Napi::Value::From(env, end));
    return retObj;
}


//--------------------------------------------------------------
Napi::Value NodeJSread(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if (!EMSread(mmapID, &key, &returnValue)) {
        THROW_ERROR(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        return ems2napiReturnValue(env, &returnValue);
    }
}


Napi::Value NodeJSreadFE(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if (!EMSreadFE(mmapID, &key, &returnValue)) {
        THROW_ERROR(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        return ems2napiReturnValue(env, &returnValue);
    }
}


Napi::Value NodeJSreadFF(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if (!EMSreadFF(mmapID, &key, &returnValue)) {
        THROW_ERROR(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        return ems2napiReturnValue(env, &returnValue);
    }
}


Napi::Value NodeJSreadRW(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    EMSvalueType returnValue = EMS_VALUE_TYPE_INITIALIZER;
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    if (!EMSreadRW(mmapID, &key, &returnValue)) {
        THROW_ERROR(QUOTE(__FUNCTION__) ": Unable to read (no return value) from EMS.");
    } else {
        return ems2napiReturnValue(env, &returnValue);
    }
}


Napi::Value NodeJSreleaseRW(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    int nReadersActive = EMSreleaseRW(mmapID, &key);
    if (nReadersActive < 0) {
        THROW_ERROR("NodeJSreleaseRW: Invalid index for key, or index key in bad state");
    } else {
        return Napi::Value::From(env, nReadersActive);
    }
}

// ====================================================

Napi::Value NodeJSwrite(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwrite(mmapID, &key, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSwriteEF(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteEF(mmapID, &key, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSwriteXF(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteXF(mmapID, &key, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSwriteXE(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1);
    bool returnValue = EMSwriteXE(mmapID, &key, &value);
    return Napi::Value::From(env, returnValue);
}


Napi::Value NodeJSsetTag(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    STACK_ALLOC_AND_CHECK_KEY_ARG;
    STACK_ALLOC_AND_CHECK_VALUE_ARG(1); // Bool -- is full
    bool success = EMSsetTag(mmapID, &key, (bool)value.value);
    if (success) {
        return Napi::Value::From(env, true);
    } else {
        THROW_ERROR("NodeJSsetTag: Invalid key, unable to set tag");
    }
}


Napi::Value NodeJSsync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    fprintf(stderr, "NodeJSsync: WARNING: sync is not implemented\n");
    bool success = EMSsync(mmapID);
    return Napi::Value::From(env, success);
}


Napi::Value NodeJSindex2key(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    EMSvalueType key = EMS_VALUE_TYPE_INITIALIZER;
    int idx = (size_t) (int64_t)  info[0].As<Napi::Number>();
    if ( !EMSindex2key(mmapID, idx, &key) ) {
        fprintf(stderr, "NodeJSindex2key: Error converting index (%d) to key\n", idx);
    }
    return ems2napiReturnValue(env, &key);
}


Napi::Value NodeJSdestroy(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    NODE_MMAPID_DECL;
    bool do_unlink = info[0].As<Napi::Boolean>();
    bool success = EMSdestroy(mmapID, do_unlink);
    if (success) {
        return Napi::Value::From(env, true);
    } else {
        THROW_ERROR("NodeJSdestroy: Failed to destroy EMS array");
    }
};


//==================================================================
//  EMS Entry Point:   Allocate and initialize the EMS domain memory
Napi::Value NodeJSinitialize(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() != 15) {
        THROW_ERROR("NodeJSinitialize: Incorrect number of arguments");
    }
    EMSvalueType fillData = EMS_VALUE_TYPE_INITIALIZER;

    //  Parse all the arguments
    int64_t nElements  = info[0].As<Napi::Number>();
    size_t  heapSize   = (int64_t)  info[1].As<Napi::Number>();
    bool useMap        = info[2].As<Napi::Boolean>();
    std::string filestr(info[3].As<Napi::String>().Utf8Value());
    const char *filename = filestr.c_str();
    bool persist       = info[4].As<Napi::Boolean>();
    bool useExisting   = info[5].As<Napi::Boolean>();
    bool doDataFill    = info[6].As<Napi::Boolean>();
    bool fillIsJSON    = info[7].As<Napi::Boolean>();
    // 8 = Data Fill type TBD during fill
    bool doSetFEtags   = info[9].As<Napi::Boolean>();
    bool setFEtags     = info[10].As<Napi::Boolean>();
    EMSmyID            = (int)  info[11].As<Napi::Number>();
    bool pinThreads    = info[12].As<Napi::Boolean>();
    int32_t nThreads   = info[13].As<Napi::Number>();
    int32_t pctMLock   = info[14].As<Napi::Number>();

    if (doDataFill) {
        NAPI_OBJ_2_EMS_OBJ(info[8], fillData, fillIsJSON);
        if (doDataFill  &&  (fillData.type == EMS_TYPE_JSON  ||  fillData.type == EMS_TYPE_STRING)) {
            // Copy the default values to the heap because napiObj2EMSval copies them to the stack
            void *valueOnStack = fillData.value;
            fillData.value = malloc(fillData.length + 1);
            if (fillData.value == NULL) {
                THROW_ERROR("NodeJSinitialize: failed to allocate the default value's storage on the heap");
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

    if (emsBufN < 0) {
        THROW_ERROR("NodeJSinitialize: failed to initialize EMS array");
    }

    // ========================================================================================
    Napi::Object obj = Napi::Object::New(env);
    obj.Set(Napi::String::New(env, "mmapID"), Napi::Value::From(env, emsBufN));
    ADD_FUNC_TO_NAPI_OBJ(obj, "faa", NodeJSfaa);
    ADD_FUNC_TO_NAPI_OBJ(obj, "cas", NodeJScas);
    ADD_FUNC_TO_NAPI_OBJ(obj, "read", NodeJSread);
    ADD_FUNC_TO_NAPI_OBJ(obj, "write", NodeJSwrite);
    ADD_FUNC_TO_NAPI_OBJ(obj, "readRW", NodeJSreadRW);
    ADD_FUNC_TO_NAPI_OBJ(obj, "releaseRW", NodeJSreleaseRW);
    ADD_FUNC_TO_NAPI_OBJ(obj, "readFE", NodeJSreadFE);
    ADD_FUNC_TO_NAPI_OBJ(obj, "readFF", NodeJSreadFF);
    ADD_FUNC_TO_NAPI_OBJ(obj, "setTag", NodeJSsetTag);
    ADD_FUNC_TO_NAPI_OBJ(obj, "writeEF", NodeJSwriteEF);
    ADD_FUNC_TO_NAPI_OBJ(obj, "writeXF", NodeJSwriteXF);
    ADD_FUNC_TO_NAPI_OBJ(obj, "writeXE", NodeJSwriteXE);
    ADD_FUNC_TO_NAPI_OBJ(obj, "push", NodeJSpush);
    ADD_FUNC_TO_NAPI_OBJ(obj, "pop", NodeJSpop);
    ADD_FUNC_TO_NAPI_OBJ(obj, "enqueue", NodeJSenqueue);
    ADD_FUNC_TO_NAPI_OBJ(obj, "dequeue", NodeJSdequeue);
    ADD_FUNC_TO_NAPI_OBJ(obj, "sync", NodeJSsync);
    ADD_FUNC_TO_NAPI_OBJ(obj, "index2key", NodeJSindex2key);
    ADD_FUNC_TO_NAPI_OBJ(obj, "destroy", NodeJSdestroy);
    return obj;
}


//---------------------------------------------------------------
static Napi::Object RegisterModule(Napi::Env env, Napi::Object exports) {
    ADD_FUNC_TO_NAPI_OBJ(exports, "initialize", NodeJSinitialize);
    ADD_FUNC_TO_NAPI_OBJ(exports, "barrier", NodeJSbarrier);
    ADD_FUNC_TO_NAPI_OBJ(exports, "singleTask", NodeJSsingleTask);
    ADD_FUNC_TO_NAPI_OBJ(exports, "criticalEnter", NodeJScriticalEnter);
    ADD_FUNC_TO_NAPI_OBJ(exports, "criticalExit", NodeJScriticalExit);
    ADD_FUNC_TO_NAPI_OBJ(exports, "loopInit", NodeJSloopInit);
    ADD_FUNC_TO_NAPI_OBJ(exports, "loopChunk", NodeJSloopChunk);
    return exports;
}


NODE_API_MODULE(ems, RegisterModule);
