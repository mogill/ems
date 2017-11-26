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
#ifndef EMSPROJ_NODEJS_H
#define EMSPROJ_NODEJS_H
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <node.h>
#include <v8.h>
#include "nan.h"

#define QUOTE_ARG(x) #x
#define QUOTE(x) QUOTE_ARG(x)

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




//==================================================================
//  Macro to declare and unwrap the EMS buffer, used to access the
//  EMSarray object metadata
#define JS_ARG_TO_OBJ(arg) v8::Handle<v8::Object>::Cast(arg)
#define JS_PROP_TO_VALUE(obj, property) JS_ARG_TO_OBJ(obj)->Get(Nan::New(property).ToLocalChecked())
#define JS_PROP_TO_INT(obj, property) (JS_PROP_TO_VALUE(obj, property)->ToInteger()->Value())

#define SOURCE_LOCATION __FILE__ ":" QUOTE(__LINE__)

#define STACK_ALLOC_AND_CHECK_KEY_ARG                               \
    NODE_MMAPID_DECL;                                               \
    EMSvalueType key = EMS_VALUE_TYPE_INITIALIZER;                  \
    if (info.Length() < 1) {                                        \
        Nan::ThrowError(SOURCE_LOCATION ": missing key argument."); \
        return;                                                     \
    } else {                                                        \
        NAN_OBJ_2_EMS_OBJ(info[0], key, false);                     \
    }


#define STACK_ALLOC_AND_CHECK_VALUE_ARG(argNum)         \
    bool stringIsJSON = false;                                          \
    EMSvalueType value = EMS_VALUE_TYPE_INITIALIZER;                    \
    if (info.Length() == argNum + 2) {                                \
        stringIsJSON = info[argNum + 1]->ToBoolean()->Value();        \
    }                                                                   \
    if (info.Length() < argNum + 1) {                                 \
        Nan::ThrowError(SOURCE_LOCATION ": ERROR, wrong number of arguments for value"); \
        return;                                                         \
    } else {                                                            \
        NAN_OBJ_2_EMS_OBJ(info[argNum], value, stringIsJSON); \
    }

#define NODE_MMAPID_DECL \
    const int mmapID = JS_PROP_TO_INT(info.This(), "mmapID")


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
