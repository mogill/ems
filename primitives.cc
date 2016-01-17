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
//  Push onto stack
//
void EMSpush(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t newTag;
    int stringIsJSON = false;

    if (info.Length() == 2) {
        stringIsJSON = info[1]->ToBoolean()->Value();
    }

    // Wait until the stack top is full, then mark it busy while updating the stack
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int32_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)];  // TODO BUG: Truncating the full 64b range
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
    if (idx == bufInt64[EMScbData(EMS_ARR_NELEM)] - 1) {
        Nan::ThrowError("EMSpush: Ran out of stack entries");
        return;
    }

    //  Wait until the target memory at the top of the stack is empty
    newTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_TAG_EMPTY, EMS_TAG_BUSY, EMS_TAG_ANY);
    newTag.tags.rw = 0;
    newTag.tags.type = EMSv8toEMStype(info[0], stringIsJSON);
    newTag.tags.fe = EMS_TAG_FULL;

    //  Write the value onto the stack
    switch (newTag.tags.type) {
        case EMS_TYPE_BOOLEAN:
            bufInt64[EMSdataData(idx)] = (int64_t) info[0]->ToBoolean()->Value();
            break;
        case EMS_TYPE_INTEGER:
            bufInt64[EMSdataData(idx)] = (int64_t) info[0]->ToInteger()->Value();
            break;
        case EMS_TYPE_FLOAT:
            bufDouble[EMSdataData(idx)] = (double) info[0]->ToNumber()->Value();
            break;
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            std::string argString(*Nan::Utf8String(info[0]));
            const char *arg_c_str = argString.c_str();
            int64_t textOffset;
            EMS_ALLOC(textOffset, argString.length() + 1, "EMSpush: out of memory to store string", );
            bufInt64[EMSdataData(idx)] = textOffset;
            strcpy(EMSheapPtr(textOffset), arg_c_str);
        }
            break;
        case EMS_TYPE_UNDEFINED:
            bufInt64[EMSdataData(idx)] = 0xdeadbeef;
            break;
        default:
            Nan::ThrowTypeError("EMSpush: Unknown arg type");
            return;
    }

    //  Mark the data on the stack as FULL
    bufTags[EMSdataTag(idx)].byte = newTag.byte;

    //  Push is complete, Mark the stack pointer as full
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;

    info.GetReturnValue().Set(Nan::New(idx));
    return;
}


//==================================================================
//  Pop data from stack
//
void EMSpop(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t dataTag;

    //  Wait until the stack pointer is full and mark it empty while pop is performed
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]--;
    int64_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)];
    if (idx < 0) {
        //  Stack is empty, return undefined
        bufInt64[EMScbData(EMS_ARR_STACKTOP)] = 0;
        bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
        info.GetReturnValue().Set(Nan::Undefined());
        return;
    }

    //  Wait until the data pointed to by the stack pointer is full, then mark it
    //  busy while it is copied, and set it to EMPTY when finished
    dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    switch (dataTag.tags.type) {
        case EMS_TYPE_BOOLEAN: {
            bool retBool = bufInt64[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retBool));
            return;
        }
        case EMS_TYPE_INTEGER: {
            int32_t retInt = bufInt64[EMSdataData(idx)]; // TODO: BUG 64b truncation again
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retInt));
            return;
        }
        case EMS_TYPE_FLOAT: {
            double retFloat = bufDouble[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retFloat));
            return;
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            if (dataTag.tags.type == EMS_TYPE_JSON) {
                v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
                retObj->Set(Nan::New("data").ToLocalChecked(),
                            // v8::String::NewFromUtf8(isolate, EMSheapPtr(bufInt64[EMSdataData(idx)])));
                            Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                EMS_FREE(bufInt64[EMSdataData(idx)]);
                info.GetReturnValue().Set(retObj);
                return;
            } else {
                // info.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, EMSheapPtr(bufInt64[EMSdataData(idx)])));
                info.GetReturnValue().Set(Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                EMS_FREE(bufInt64[EMSdataData(idx)]);
                return;
            }
        }
        case EMS_TYPE_UNDEFINED: {
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::Undefined());
            return;
        }
        default:
            Nan::ThrowTypeError("EMSpop unknown type");
            return;
    }
}


//==================================================================
//  Enqueue data
//  Heap top and bottom are monotonically increasing, but the index
//  returned is a circular buffer.
//
void EMSenqueue(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    int stringIsJSON = false;
    if (info.Length() == 2) {
        stringIsJSON = info[1]->ToBoolean()->Value();
    }

    //  Wait until the heap top is full, and mark it busy while data is enqueued
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int32_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)] % bufInt64[EMScbData(EMS_ARR_NELEM)];  // TODO: BUG  This could be trucated
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
    if (bufInt64[EMScbData(EMS_ARR_STACKTOP)] - bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] >
        bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        Nan::ThrowError("EMSenqueue: Ran out of stack entries");
        return;
    }

    //  Wait for data pointed to by heap top to be empty, then set to Full while it is filled
    bufTags[EMSdataTag(idx)].tags.rw = 0;
    bufTags[EMSdataTag(idx)].tags.type = EMSv8toEMStype(info[0], stringIsJSON);
    switch (bufTags[EMSdataTag(idx)].tags.type) {
        case EMS_TYPE_BOOLEAN:
            bufInt64[EMSdataData(idx)] = (int64_t) info[0]->ToBoolean()->Value();
            break;
        case EMS_TYPE_INTEGER:
            bufInt64[EMSdataData(idx)] = (int64_t) info[0]->ToInteger()->Value();
            break;
        case EMS_TYPE_FLOAT:
            bufDouble[EMSdataData(idx)] = (double) info[0]->ToNumber()->Value();
            break;
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            std::string argString(*Nan::Utf8String(info[0]));
            const char *arg_c_str = argString.c_str();
            int64_t textOffset;
            EMS_ALLOC(textOffset, argString.length() + 1, "EMSenqueue: out of memory to store string", );
            bufInt64[EMSdataData(idx)] = textOffset;
            strcpy(EMSheapPtr(textOffset), arg_c_str);
        }
            break;
        case EMS_TYPE_UNDEFINED:
            bufInt64[EMSdataData(idx)] = 0xdeadbeef;
            break;
        default:
            Nan::ThrowTypeError("xEMSwrite: Unknown arg type");
            return;
    }

    //  Set the tag on the data to FULL
    bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_FULL;

    //  Enqueue is complete, set the tag on the heap to to FULL
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
    info.GetReturnValue().Set(Nan::New(idx));
    return;
}


//==================================================================
//  Dequeue
void EMSdequeue(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t dataTag;

    //  Wait for bottom of heap pointer to be full, and mark it busy while data is dequeued
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int64_t idx = bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] % bufInt64[EMScbData(EMS_ARR_NELEM)];
    //  If Queue is empty, return undefined
    if (bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] >= bufInt64[EMScbData(EMS_ARR_STACKTOP)]) {
        bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] = bufInt64[EMScbData(EMS_ARR_STACKTOP)];
        bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
        info.GetReturnValue().Set(Nan::Undefined());
        return;
    }

    bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)]++;
    //  Wait for the data pointed to by the bottom of the heap to be full,
    //  then mark busy while copying it, and finally set it to empty when done
    dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    dataTag.tags.fe = EMS_TAG_EMPTY;
    switch (dataTag.tags.type) {
        case EMS_TYPE_BOOLEAN: {
            bool retBool = bufInt64[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retBool));
            return;
        }
        case EMS_TYPE_INTEGER: {
            int32_t retInt = bufInt64[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retInt));
            return;
        }
        case EMS_TYPE_FLOAT: {
            double retFloat = bufDouble[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::New(retFloat));
            return;
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            if (dataTag.tags.type == EMS_TYPE_JSON) {
                v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
                retObj->Set(Nan::New("data").ToLocalChecked(),
                            Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                EMS_FREE(bufInt64[EMSdataData(idx)]);
                info.GetReturnValue().Set(retObj);
                return;
            } else {
                // info.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, EMSheapPtr(bufInt64[EMSdataData(idx)])));
                info.GetReturnValue().Set(Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                EMS_FREE(bufInt64[EMSdataData(idx)]);
                return;
            }
        }
        case EMS_TYPE_UNDEFINED: {
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            info.GetReturnValue().Set(Nan::Undefined());
            return;
        }
        default:
            Nan::ThrowTypeError("EMSdequeue unknown type");
            return;
    }
}
