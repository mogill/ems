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
//  Fetch and Add Atomic Memory Operation
//  Returns a+b where a is data in EMS memory and b is an argument
//
void EMSfaa(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    EMStag_t *bufTags = (EMStag_t *) emsBuf;

    if (info.Length() == 2) {
        int64_t idx = EMSwriteIndexMap(info);
        volatile int64_t *bufInt64 = (int64_t *) emsBuf;
        volatile double *bufDouble = (double *) emsBuf;
        char *bufChar = (char *) emsBuf;
        EMStag_t oldTag;

        if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
            Nan::ThrowError("EMSfaa: index out of bounds");
            return;
        }

        // Wait until the data is FULL, mark it busy while FAA is performed
        oldTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
        oldTag.tags.fe = EMS_TAG_FULL;  // When written back, mark FULL
        int argType = EMSv8toEMStype(info[1], false);  // Never add to an object, treat as string
        switch (oldTag.tags.type) {
            case EMS_TYPE_BOOLEAN: {    //  Bool + _______
                bool retBool = bufInt64[EMSdataData(idx)];  // Read original value in memory
                switch (argType) {
                    case EMS_TYPE_INTEGER:   //  Bool + Int
                        bufInt64[EMSdataData(idx)] += info[1]->ToInteger()->Value();
                        oldTag.tags.type = EMS_TYPE_INTEGER;
                        break;
                    case EMS_TYPE_FLOAT:     //  Bool + Float
                        bufDouble[EMSdataData(idx)] =
                                (double) bufInt64[EMSdataData(idx)] + info[1]->ToNumber()->Value();
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                        break;
                    case EMS_TYPE_UNDEFINED: //  Bool + undefined
                        bufDouble[EMSdataData(idx)] = NAN;
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                        break;
                    case EMS_TYPE_BOOLEAN:   //  Bool + Bool
                        bufInt64[EMSdataData(idx)] += info[1]->ToBoolean()->Value();
                        oldTag.tags.type = EMS_TYPE_INTEGER;
                        break;
                    case EMS_TYPE_STRING: {   //  Bool + string
                        const char *argString = JS_ARG_TO_CSTR(info[1]);
                        int64_t len = strlen(argString) + 1 + 5;  //  String length + Terminating null + 'false'
                        int64_t textOffset;
                        EMS_ALLOC(textOffset, len, "EMSfaa(bool+string): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%s",
                                bufInt64[EMSdataData(idx)] ? "true" : "false", argString);
                        bufInt64[EMSdataData(idx)] = textOffset;
                        oldTag.tags.type = EMS_TYPE_STRING;
                    }
                        break;
                    default:
                        Nan::ThrowError("EMSfaa: Data is BOOL, but FAA arg type is unknown");
                        return;

                }
                //  Write the new type and set the tag to Full, then return the original value
                bufTags[EMSdataTag(idx)].byte = oldTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                info.GetReturnValue().Set(Nan::New(retBool));
                return;
            }  // End of:  Bool + ___

            case EMS_TYPE_INTEGER: {
                int32_t retInt = bufInt64[EMSdataData(idx)];  // Read original value in memory
                switch (argType) {
                    case EMS_TYPE_INTEGER: {  // Int + int
                        int64_t memInt = bufInt64[EMSdataData(idx)];
                        // TODO: Magic max int promotion to float
                        if (memInt >= (1 << 30)) {  // Possible integer overflow, convert to float
                            bufDouble[EMSdataData(idx)] =
                                    (double) bufInt64[EMSdataData(idx)] + (double) (info[1]->ToInteger()->Value());
                            oldTag.tags.type = EMS_TYPE_FLOAT;
                        } else { //  Did not overflow to flow, still an integer
                            bufInt64[EMSdataData(idx)] += info[1]->ToInteger()->Value();
                        }
                    }
                        break;
                    case EMS_TYPE_FLOAT:     // Int + float
                        bufDouble[EMSdataData(idx)] =
                                (double) bufInt64[EMSdataData(idx)] + info[1]->ToNumber()->Value();
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                        break;
                    case EMS_TYPE_UNDEFINED: // Int + undefined
                        bufDouble[EMSdataData(idx)] = NAN;
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                        break;
                    case EMS_TYPE_BOOLEAN:   // Int + bool
                        bufInt64[EMSdataData(idx)] += info[1]->ToBoolean()->Value();
                        break;
                    case EMS_TYPE_STRING: {   // int + string
                        const char *argString = JS_ARG_TO_CSTR(info[1]);
                        int64_t len = strlen(argString) + 1 + MAX_NUMBER2STR_LEN;
                        int64_t textOffset;
                        EMS_ALLOC(textOffset, len, "EMSfaa(int+string): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%lld%s", (long long int) bufInt64[EMSdataData(idx)],
                                argString);
                        bufInt64[EMSdataData(idx)] = textOffset;
                        oldTag.tags.type = EMS_TYPE_STRING;
                    }
                        break;
                    default:
                        Nan::ThrowError("EMSfaa: Data is INT, but FAA arg type is unknown");
                        return;
                }
                //  Write the new type and set the tag to Full, then return the original value
                bufTags[EMSdataTag(idx)].byte = oldTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                info.GetReturnValue().Set(Nan::New(retInt));
                return;
            }  // End of: Integer + ____

            case EMS_TYPE_FLOAT: {
                double retDbl = bufDouble[EMSdataData(idx)];
                switch (argType) {
                    case EMS_TYPE_INTEGER:   // Float + int
                        bufDouble[EMSdataData(idx)] += (double) info[1]->ToInteger()->Value();
                        break;
                    case EMS_TYPE_FLOAT:     // Float + float
                        bufDouble[EMSdataData(idx)] += info[1]->ToNumber()->Value();
                        break;
                    case EMS_TYPE_BOOLEAN:   // Float + boolean
                        bufDouble[EMSdataData(idx)] += (double) info[1]->ToInteger()->Value();
                        break;
                    case EMS_TYPE_STRING: {   // Float + string
                        const char *argString = JS_ARG_TO_CSTR(info[1]);
                        int64_t len = strlen(argString) + 1 + MAX_NUMBER2STR_LEN;
                        int64_t textOffset;
                        EMS_ALLOC(textOffset, len, "EMSfaa(float+string): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%lf%s", bufDouble[EMSdataData(idx)], argString);
                        bufInt64[EMSdataData(idx)] = textOffset;
                        oldTag.tags.type = EMS_TYPE_STRING;
                    }
                        break;
                    case EMS_TYPE_UNDEFINED: // Float + Undefined
                        bufDouble[EMSdataData(idx)] = NAN;
                        break;
                    default:
                        Nan::ThrowError("EMSfaa: Data is FLOAT, but arg type unknown");
                        return;
                }
                //  Write the new type and set the tag to Full, then return the original value
                bufTags[EMSdataTag(idx)].byte = oldTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                info.GetReturnValue().Set(Nan::New(retDbl));
                return;
            } //  End of: float + _______

            case EMS_TYPE_STRING: {
                info.GetReturnValue().Set(Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                int64_t textOffset;
                int64_t len;
                switch (argType) {
                    case EMS_TYPE_INTEGER: // string + int
                        len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + MAX_NUMBER2STR_LEN;
                        EMS_ALLOC(textOffset, len, "EMSfaa(string+int): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%lld",
                                EMSheapPtr(bufInt64[EMSdataData(idx)]),
                                (long long int) info[1]->ToInteger()->Value());
                        break;
                    case EMS_TYPE_FLOAT:   // string + dbl
                        len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + MAX_NUMBER2STR_LEN;
                        EMS_ALLOC(textOffset, len, "EMSfaa(string+dbl): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%lf",
                                EMSheapPtr(bufInt64[EMSdataData(idx)]),
                                info[1]->ToNumber()->Value());
                        break;
                    case EMS_TYPE_STRING: { // string + string
                        const char *argString = JS_ARG_TO_CSTR(info[1]);
                        // v8::String::Utf8Value argString(args[1]);
                        len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + strlen(argString);
                        EMS_ALLOC(textOffset, len, "EMSfaa(string+string): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%s",
                                EMSheapPtr(bufInt64[EMSdataData(idx)]),
                                argString);
                    }
                        break;
                    case EMS_TYPE_BOOLEAN:   // string + bool
                        static char strTrue[] = "true";
                        static char strFalse[] = "false";
                        char *tfString;
                        if (info[1]->ToBoolean()->Value()) tfString = strTrue;
                        else tfString = strFalse;
                        len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + strlen(tfString);
                        EMS_ALLOC(textOffset, len, "EMSfaa(string+bool): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%s",
                                EMSheapPtr(bufInt64[EMSdataData(idx)]),
                                tfString);
                        break;
                    case EMS_TYPE_UNDEFINED: // string + undefined
                        len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + strlen("undefined");
                        EMS_ALLOC(textOffset, len, "EMSfaa(string+bool): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "%s%s",
                                EMSheapPtr(bufInt64[EMSdataData(idx)]),
                                "undefined");
                        break;
                    default:
                        Nan::ThrowError("EMSfaa(string+?): Unknown data type");
                        return;
                }
                EMS_FREE(bufInt64[EMSdataData(idx)]);
                bufInt64[EMSdataData(idx)] = textOffset;
                oldTag.tags.type = EMS_TYPE_STRING;
                //  Write the new type and set the tag to Full, then return the original value
                bufTags[EMSdataTag(idx)].byte = oldTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                // return value was set at the top of this block
                return;
            }  // End of: String + __________

            case EMS_TYPE_UNDEFINED: {
                switch (argType) {  // Undefined + Int, dloat, bool, or undef
                    case EMS_TYPE_INTEGER:
                    case EMS_TYPE_FLOAT:
                    case EMS_TYPE_BOOLEAN:
                    case EMS_TYPE_UNDEFINED:
                        bufDouble[EMSdataData(idx)] = NAN;
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                        break;
                    case EMS_TYPE_STRING: { // Undefined + string
                        const char *argString = JS_ARG_TO_CSTR(info[1]);
                        int64_t len = strlen(argString) + 1 + strlen("NaN");
                        int64_t textOffset;
                        // TODO: BUG Looks like another memory leak
                        EMS_ALLOC(textOffset, len, "EMSfaa(undef+String): out of memory to store string", );
                        sprintf(EMSheapPtr(textOffset), "NaN%s", argString);
                        bufInt64[EMSdataData(idx)] = textOffset;
                        oldTag.tags.type = EMS_TYPE_UNDEFINED;
                    }
                        break;
                    default:
                        Nan::ThrowError("EMSfaa(Undefined+___: Unknown stored data type");
                        return;
                }
                //  Write the new type and set the tag to Full, then return the original value
                bufTags[EMSdataTag(idx)].byte = oldTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                info.GetReturnValue().Set(Nan::Undefined());
                return;
            }
            default:
                Nan::ThrowError("EMSfaa(?+___: Unknown stored data type");
                return;
        }
    }
    Nan::ThrowError("EMSfaa: Wrong number of arguments");
    return;
}


//==================================================================
//  Atomic Compare and Swap
//
void EMScas(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");

    if (info.Length() >= 3) {
        // int64_t idx = EMSwriteIndexMap(info);
        int64_t idx = EMSreadIndexMap(info);
        volatile int64_t *bufInt64 = (int64_t *) emsBuf;
        volatile double *bufDouble = (double *) emsBuf;
        char * bufChar = (char *) emsBuf;
        volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
        EMStag_t newTag;
        bool boolMemVal = false;
        int32_t intMemVal = -1;
        double floatMemVal = 0.0;
        int64_t textOffset;
        char stringMemVal[MAX_KEY_LEN];
        const char *stringNewVal;
        const char *stringOldVal;
        int swapped = false;

        if ((!EMSisMapped  &&  idx < 0) || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
            Nan::ThrowError("EMScas: index out of bounds");
            return;
        }

        int oldType = EMSv8toEMStype(info[1], false);  // Never CAS an object, treat as string
        int newType;
        if (info.Length() == 4) {
            newType = EMSv8toEMStype(info[2], info[3]->ToBoolean()->Value());
        } else {
            newType = EMSv8toEMStype(info[2], false);
        }

    retry_on_undefined:
        int memType;
        if(EMSisMapped  &&  idx < 0) {
            memType = EMS_TYPE_UNDEFINED;
        } else {
            //  Wait for the memory to be Full, then mark it Busy while CAS works
            EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
            memType = bufTags[EMSdataTag(idx)].tags.type;
        }

        //  Read the value in memory
        switch (memType) {
            case EMS_TYPE_UNDEFINED:
                break;
            case EMS_TYPE_BOOLEAN:
                boolMemVal = bufInt64[EMSdataData(idx)];
                break;
            case EMS_TYPE_INTEGER:
                intMemVal = bufInt64[EMSdataData(idx)];
                break;
            case EMS_TYPE_FLOAT:
                floatMemVal = bufDouble[EMSdataData(idx)];
                break;
            case EMS_TYPE_JSON:
            case EMS_TYPE_STRING:
                strncpy(stringMemVal, EMSheapPtr(bufInt64[EMSdataData(idx)]), MAX_KEY_LEN);
                break;
            default:
                Nan::ThrowError("EMScas: memType not recognized");
                return;
        }

        //  Compare the value in memory the the "old" CAS value
        if (oldType == memType) {
            //  Allocate on Write: If this memory was undefined (ie: unallocated),
            //  allocate the index map, store the undefined, and start over again.
            if(EMSisMapped  &&  idx < 0) {
                idx = EMSwriteIndexMap(info);
                if (idx < 0) {
                    Nan::ThrowError("EMScas: Not able to allocate map on CAS of undefined data");
                    return;
                }
                bufInt64[EMSdataData(idx)] = 0xcafebabe;
                newTag.tags.fe = EMS_TAG_FULL;
                newTag.tags.rw = 0;
                newTag.tags.type = EMS_TYPE_UNDEFINED;
                bufTags[EMSdataTag(idx)].byte = newTag.byte;
                goto retry_on_undefined;
            }
            switch (memType) {
                case EMS_TYPE_UNDEFINED:
                    swapped = true;
                    break;
                case EMS_TYPE_BOOLEAN:
                    if (boolMemVal == info[1]->ToBoolean()->Value()) swapped = true;
                    break;
                case EMS_TYPE_INTEGER:
                    if (intMemVal == info[1]->ToInteger()->Value()) swapped = true;
                    break;
                case EMS_TYPE_FLOAT:
                    if (floatMemVal == info[1]->ToNumber()->Value()) swapped = true;
                    break;
                case EMS_TYPE_JSON:
                case EMS_TYPE_STRING:
                    stringOldVal = JS_ARG_TO_CSTR(info[1]);
                    if (strncmp(stringMemVal, stringOldVal, MAX_KEY_LEN) == 0) {
                        swapped = true;
                    }
                    break;
                default:
                    Nan::ThrowError("EMScas: oldTag not recognized");
                    return;
            }
        }

        //  If memory==old then write the new value
        newTag.tags.fe = EMS_TAG_FULL;
        newTag.tags.rw = 0;
        newTag.tags.type = memType;
        if (swapped) {
            newTag.tags.type = newType;
            switch (newType) {
                case EMS_TYPE_UNDEFINED:
                    bufInt64[EMSdataData(idx)] = 0xdeadbeef;  // info[2]->ToBoolean()->Value();
                    break;
                case EMS_TYPE_BOOLEAN:
                    bufInt64[EMSdataData(idx)] = (int64_t) info[2]->ToBoolean()->Value();
                    break;
                case EMS_TYPE_INTEGER:
                    bufInt64[EMSdataData(idx)] = info[2]->ToInteger()->Value();
                    break;
                case EMS_TYPE_FLOAT:
                    bufDouble[EMSdataData(idx)] = info[2]->ToNumber()->Value();
                    break;
                case EMS_TYPE_JSON:
                case EMS_TYPE_STRING: {
                    if (memType == EMS_TYPE_STRING) EMS_FREE(bufInt64[EMSdataData(idx)]);
                    stringNewVal = JS_ARG_TO_CSTR(info[2]);
                    int len = strlen(stringNewVal);
                    EMS_ALLOC(textOffset, len + 1, "EMScas(string): out of memory to store string", );
                    strcpy(EMSheapPtr(textOffset), stringNewVal);
                    bufInt64[EMSdataData(idx)] = textOffset;
                }
                    break;
                default:
                    Nan::ThrowError("EMScas(): Unrecognized new type");
                    return;
            }
        }

        //  Set the tag back to Full and return the original value
        bufTags[EMSdataTag(idx)].byte = newTag.byte;
        //  If there is a map, set the map's tag back to full
        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
        switch (memType) {
            case EMS_TYPE_UNDEFINED:
                info.GetReturnValue().Set(Nan::Undefined());
                return;
            case EMS_TYPE_BOOLEAN:
                info.GetReturnValue().Set(Nan::New(boolMemVal));
                return;
            case EMS_TYPE_INTEGER:
                info.GetReturnValue().Set(Nan::New(intMemVal));
                return;
            case EMS_TYPE_FLOAT:
                info.GetReturnValue().Set(Nan::New(floatMemVal));
                return;
            case EMS_TYPE_JSON:
            case EMS_TYPE_STRING:
                info.GetReturnValue().Set(Nan::New(stringMemVal).ToLocalChecked());
                return;
            default:
                Nan::ThrowError("EMScas(): Unrecognized mem type");
                return;
        }
    } else {
        Nan::ThrowError("EMS_CASnumber wrong number of arguments");
        return;
    }
}


