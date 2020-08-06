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
#ifndef EMSPROJ_NODEJS_H
#define EMSPROJ_NODEJS_H
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include "napi.h"

#define QUOTE_ARG(x) #x
#define QUOTE(x) QUOTE_ARG(x)

#define THROW_ERROR(error) {                                             \
        Napi::Error::New(env, error)                                     \
            .ThrowAsJavaScriptException();                               \
        return env.Null();                                               \
    }

#define THROW_TYPE_ERROR(error) {                                        \
        Napi::TypeError::New(env, error)                                 \
            .ThrowAsJavaScriptException();                               \
        return env.Null();                                               \
    }


#define ADD_FUNC_TO_NAPI_OBJ(obj, func_name, func) \
    { \
        Napi::Function fn = Napi::Function::New(env, func, func_name); \
        obj.Set(Napi::Value::From(env, func_name), fn);     \
    }

#define IS_INTEGER(x) ((double)(int64_t)(x) == (double)x)
//==================================================================
//  Determine the EMS type of a Napi argument
#define NapiObjToEMStype(arg, stringIsJSON)                          \
(                                                                    \
   arg.IsNumber() ?                                                  \
           (IS_INTEGER(arg.As<Napi::Number>()) ? EMS_TYPE_INTEGER :  \
                                                 EMS_TYPE_FLOAT ) :  \
   (arg.IsString() && !stringIsJSON) ? EMS_TYPE_STRING  :            \
   (arg.IsString() &&  stringIsJSON) ? EMS_TYPE_JSON  :              \
   arg.IsBoolean()                   ? EMS_TYPE_BOOLEAN :            \
   arg.IsUndefined()                 ? EMS_TYPE_UNDEFINED:           \
                                       EMS_TYPE_INVALID              \
)


#define SOURCE_LOCATION __FILE__ ":" QUOTE(__LINE__)

#define STACK_ALLOC_AND_CHECK_KEY_ARG                               \
    NODE_MMAPID_DECL;                                               \
    EMSvalueType key = EMS_VALUE_TYPE_INITIALIZER;                  \
    if (info.Length() < 1) {                                        \
        Napi::Error::New(env, SOURCE_LOCATION ": missing key argument.").ThrowAsJavaScriptException(); \
    } else {                                                        \
        NAPI_OBJ_2_EMS_OBJ(info[0], key, false);                    \
    }


#define STACK_ALLOC_AND_CHECK_VALUE_ARG(argNum)                         \
    bool stringIsJSON = false;                                          \
    EMSvalueType value = EMS_VALUE_TYPE_INITIALIZER;                    \
    if (info.Length() == argNum + 2) {                                  \
        stringIsJSON = info[argNum + 1].As<Napi::Boolean>();            \
    }                                                                   \
    if (info.Length() < argNum + 1) {                                   \
        Napi::Error::New(env, SOURCE_LOCATION ": ERROR, wrong number of arguments for value").ThrowAsJavaScriptException(); \
    } else {                                                            \
        NAPI_OBJ_2_EMS_OBJ(info[argNum], value, stringIsJSON);          \
    }

#define NODE_MMAPID_DECL \
    const int mmapID = (int) info.This().As<Napi::Object>()             \
                                 .Get("mmapID").As<Napi::Number>()


Napi::Value NodeJScriticalEnter(const Napi::CallbackInfo& info);
Napi::Value NodeJScriticalExit(const Napi::CallbackInfo& info);
Napi::Value NodeJSbarrier(const Napi::CallbackInfo& info);
Napi::Value NodeJSsingleTask(const Napi::CallbackInfo& info);
Napi::Value NodeJScas(const Napi::CallbackInfo& info);
Napi::Value NodeJSfaa(const Napi::CallbackInfo& info);
Napi::Value NodeJSpush(const Napi::CallbackInfo& info);
Napi::Value NodeJSpop(const Napi::CallbackInfo& info);
Napi::Value NodeJSenqueue(const Napi::CallbackInfo& info);
Napi::Value NodeJSdequeue(const Napi::CallbackInfo& info);
Napi::Value NodeJSloopInit(const Napi::CallbackInfo& info);
Napi::Value NodeJSloopChunk(const Napi::CallbackInfo& info);
//--------------------------------------------------------------
Napi::Value NodeJSread(const Napi::CallbackInfo& info);
Napi::Value NodeJSreadRW(const Napi::CallbackInfo& info);
Napi::Value NodeJSreleaseRW(const Napi::CallbackInfo& info);
Napi::Value NodeJSreadFE(const Napi::CallbackInfo& info);
Napi::Value NodeJSreadFF(const Napi::CallbackInfo& info);
Napi::Value NodeJSwrite(const Napi::CallbackInfo& info);
Napi::Value NodeJSwriteEF(const Napi::CallbackInfo& info);
Napi::Value NodeJSwriteXF(const Napi::CallbackInfo& info);
Napi::Value NodeJSwriteXE(const Napi::CallbackInfo& info);
Napi::Value NodeJSsetTag(const Napi::CallbackInfo& info);
Napi::Value NodeJSsync(const Napi::CallbackInfo& info);
Napi::Value NodeJSindex2key(const Napi::CallbackInfo& info);
Napi::Value NodeJSdestroy(const Napi::CallbackInfo& info);

#endif //EMSPROJ__H
