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
#ifndef EMSPROJ_NODEJS_H
#define EMSPROJ_NODEJS_H
#include <node.h>
#include <v8.h>
#include "nan.h"

#define ADD_FUNC_TO_V8_OBJ(obj, func_name, func) \
    { \
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(func); \
        v8::Local<v8::Function> fn = tpl->GetFunction(); \
        fn->SetName(Nan::New(func_name).ToLocalChecked()); \
        obj->Set(Nan::New(func_name).ToLocalChecked(), tpl->GetFunction()); \
    }

//==================================================================
//  Determine the EMS type of a V8 argument
#define NanObjToEMStype(arg, stringIsJSON)                       \
(                                                               \
   arg->IsInt32()                     ? EMS_TYPE_INTEGER :      \
   arg->IsNumber()                    ? EMS_TYPE_FLOAT   :      \
   (arg->IsString() && !stringIsJSON) ? EMS_TYPE_STRING  :      \
   (arg->IsString() &&  stringIsJSON) ? EMS_TYPE_JSON  :        \
   arg->IsBoolean()                   ? EMS_TYPE_BOOLEAN :      \
   arg->IsUndefined()                 ? EMS_TYPE_UNDEFINED:     \
   arg->IsUint32()                    ? EMS_TYPE_INTEGER : EMS_TYPE_INVALID   \
)


#define NAN_OBJ_TO_EMS_VAL(funcname, info, localValue, argString, stringIsJSON) { \
    localValue.type = NanObjToEMStype(info, stringIsJSON); \
    switch (localValue.type) { \
        case EMS_TYPE_BOOLEAN: \
            localValue.value = (void *) info->ToBoolean()->Value(); \
            break; \
        case EMS_TYPE_INTEGER: \
            localValue.value = (void *) info->ToInteger()->Value(); \
            break; \
        case EMS_TYPE_FLOAT: { \
            ulong_double alias; \
            alias.d = info->ToNumber()->Value(); \
            localValue.value = (void *) alias.u64; \
        } \
            break; \
        case EMS_TYPE_JSON: \
        case EMS_TYPE_STRING: { \
            argString = std::string(*Nan::Utf8String(info)); \
            localValue.value = (void *) argString.c_str(); \
            localValue.length = argString.length(); \
        } \
            break; \
        case EMS_TYPE_UNDEFINED: \
            localValue.value = (void *) 0xbeeff00d; \
            break; \
        default: \
            Nan::ThrowTypeError(#funcname " ERROR: Invalid value type"); \
            return; \
    } \
}


#define EMS_TO_V8_RETURNVALUE(returnValue, info, do_free) { \
    switch(returnValue.type) { \
        case EMS_TYPE_BOOLEAN: { \
            bool retBool = (bool) returnValue.value; \
            info.GetReturnValue().Set(Nan::New(retBool)); \
        } \
            break; \
        case EMS_TYPE_INTEGER: { \
            int32_t retInt = ((int64_t) returnValue.value) & 0xffffffff;  /* TODO: Bug -- only 32 bits of 64? */ \
            info.GetReturnValue().Set(Nan::New(retInt)); \
        } \
            break; \
        case EMS_TYPE_FLOAT: { \
            ulong_double alias; \
            alias.u64 = (uint64_t) returnValue.value; \
            info.GetReturnValue().Set(Nan::New(alias.d)); \
        } \
            break; \
        case EMS_TYPE_JSON: { \
            v8::Local<v8::Object> retObj = Nan::New<v8::Object>(/* Bogus Missing Arg Warning Here */); \
            retObj->Set(Nan::New("data").ToLocalChecked(), \
                        Nan::New((char *) returnValue.value).ToLocalChecked()); \
            info.GetReturnValue().Set(retObj); \
            if (do_free) free(returnValue.value);  /* Allocated in ems.cc (cas, faa, pop, dequeue ...) */ \
        } \
            break; \
        case EMS_TYPE_STRING: { \
            info.GetReturnValue().Set(Nan::New((char *) returnValue.value).ToLocalChecked()); \
            if (do_free) free(returnValue.value);  /* Allocated in ems.cc (cas, faa, pop, dequeue...) */ \
        } \
            break; \
        case EMS_TYPE_UNDEFINED: { \
            info.GetReturnValue().Set(Nan::Undefined()); \
        } \
            break; \
        default: \
            Nan::ThrowTypeError("EMS ERROR: EMS_TO_V8_RETURNVALUE: Invalid type of data read from memory"); \
    } \
}

//==================================================================
//  Macro to declare and unwrap the EMS buffer, used to access the
//  EMSarray object metadata
#define JS_ARG_TO_OBJ(arg) v8::Handle<v8::Object>::Cast(arg)
#define JS_PROP_TO_VALUE(obj, property) JS_ARG_TO_OBJ(obj)->Get(Nan::New(property).ToLocalChecked())
#define JS_PROP_TO_INT(obj, property) (JS_PROP_TO_VALUE(obj, property)->ToInteger()->Value())

#define NODE_MMAPID_DECL() \
    const int mmapID = JS_PROP_TO_INT(info.This(), "mmapID")

#define NODE_INFO_DECL(nodejs_funcname, retval, valueString, value, expectedNargs, infoArgN, stringIsJSON) \
    std::string valueString; \
    EMSvalueType value; \
    if (infoArgN >= info.Length()) { \
        Nan::ThrowError(#nodejs_funcname ": Called with wrong number of arguments."); \
        return retval; \
    } \
    NAN_OBJ_TO_EMS_VAL(nodejs_funcname, info[infoArgN], value, valueString, stringIsJSON)

#define NODE_WRITE(nodejs_funcname, ems_funcname) \
    bool stringIsJSON = false; \
    NODE_MMAPID_DECL(); \
    if (info.Length() == 3) { \
        stringIsJSON = info[2]->ToBoolean()->Value(); \
    } else { \
        if (info.Length() != 2) { \
            Nan::ThrowError(#nodejs_funcname ": Called with wrong number of args."); \
            return; \
        } \
    } \
    \
    NODE_INFO_DECL(nodejs_funcname, , keyString, key, 2, 0, false); \
    NODE_INFO_DECL(nodejs_funcname, , valueString, value, 2, 1, stringIsJSON); \
    \
    bool returnValue = ems_funcname (mmapID, &key, &value); \
    info.GetReturnValue().Set(Nan::New(returnValue));

#define NODE_PUSH_ENQUEUE(nodejs_funcname, ems_funcname) \
    bool stringIsJSON = false; \
    NODE_MMAPID_DECL(); \
    if (info.Length() == 2) { \
        stringIsJSON = info[1]->ToBoolean()->Value(); \
    } else { \
        if (info.Length() != 1) { \
            Nan::ThrowError(#nodejs_funcname ": Called with wrong number of args."); \
            return; \
        } \
    } \
    NODE_INFO_DECL(nodejs_funcname, , valueString, value, 1, 0, stringIsJSON); \
    int returnValue = ems_funcname (mmapID, &value); \
    info.GetReturnValue().Set(Nan::New(returnValue));

#define NODE_VALUE_DECL(nodejs_funcname, retval) \
    NODE_INFO_DECL(nodejs_funcname, retval, valueString, value, 1, 1, stringIsJSON)

#define NODE_KEY_DECL(nodejs_funcname, retval) \
    NODE_MMAPID_DECL(); \
    std::string keyString; \
    EMSvalueType key; \
    if (info.Length() < 1) { \
        Nan::ThrowError(#nodejs_funcname ": Called with wrong number of args."); \
        return retval; \
    } \
    NAN_OBJ_TO_EMS_VAL(nodejs_funcname, info[0], key, keyString, false)

#define NODE_READ(nodejs_funcname, ems_funcname, retval) \
    NODE_KEY_DECL(nodejs_funcname, retval) \
    bool errval = ems_funcname (mmapID, &key, &returnValue); \
    if(errval == false) { \
        Nan::ThrowError(#nodejs_funcname ": Unable to read (no return value) from EMS."); \
        return retval; \
    } \
    EMS_TO_V8_RETURNVALUE(returnValue, info, false)  /* Bogus Missing argument warning */


void NodeJScriticalEnter(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJScriticalExit(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSbarrier(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSsingleTask(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJScas(const Nan::FunctionCallbackInfo<v8::Value> &info);
void NodeJSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSpush(const Nan::FunctionCallbackInfo<v8::Value> &info);
void NodeJSpop(const Nan::FunctionCallbackInfo<v8::Value> &info);
void NodeJSenqueue(const Nan::FunctionCallbackInfo<v8::Value> &info);
void NodeJSdequeue(const Nan::FunctionCallbackInfo<v8::Value> &info);
void NodeJSloopInit(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSloopChunk(const Nan::FunctionCallbackInfo<v8::Value>& info);
//--------------------------------------------------------------
void NodeJSread(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSsync(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSindex2key(const Nan::FunctionCallbackInfo<v8::Value>& info);
void NodeJSdestroy(const Nan::FunctionCallbackInfo<v8::Value>& info);

#endif //EMSPROJ__H
