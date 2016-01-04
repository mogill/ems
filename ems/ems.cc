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
//  Resolve External Declarations
//
int EMSmyID;   // EMS Thread ID
char *emsBufs[EMS_MAX_N_BUFS] = { NULL };

//==================================================================
//  Wrappers around memory allocator to ensure mutual exclusion
//  The buddy memory allocator is not thread safe, so this is necessary for now.
//  It can be used as the hook to wrap the non-threaded allocator with a
//  multiplexor of several independent regions.
//
//  Returns the byte offset in the EMS data space of the space allocated
//
size_t emsMutexMem_alloc(struct emsMem *heap,   // Base of EMS malloc structs
                         size_t len,            // Number of bytes to allocate
                         volatile char *mutex)  // Pointer to the mem allocator's mutex
{
    RESET_NAP_TIME;
    // Wait until we acquire the allocator's mutex
    while (!__sync_bool_compare_and_swap(mutex, EMS_TAG_EMPTY, EMS_TAG_FULL)) {
        NANOSLEEP;
    }
    size_t retval = emsMem_alloc(heap, len);
    *mutex = EMS_TAG_EMPTY;    // Unlock the allocator's mutex
    return (retval);
}


void emsMutexMem_free(struct emsMem *heap,  // Base of EMS malloc structs
                      size_t addr,          // Offset of alloc'd block in EMS memory
                      volatile char *mutex) // Pointer to the mem allocator's mutex
{
    RESET_NAP_TIME;
    // Wait until we acquire the allocator's mutex
    while (!__sync_bool_compare_and_swap(mutex, EMS_TAG_EMPTY, EMS_TAG_FULL)) {
        NANOSLEEP;
    }
    emsMem_free(heap, addr);
    *mutex = EMS_TAG_EMPTY;   // Unlock the allocator's mutex
}




#if 0
//==================================================================
//  Callback for destruction of an EMS array
//
static void EMSarrFinalize(char *data, void *hint) {
    //fprintf(stderr, "%d: EMSarrFinalize  data=%lx  hint=%lx %" PRIu64 "\n", EMSmyID, data, hint, hint);
    munmap(data, (size_t) hint);

    {
    v8::HandleScope scope;
    EMS_DECL(args);
    size_t length = buffer->handle_->GetIndexedPropertiesExternalArrayDataLength();
    
    if(-1 == munmap(emsBuf, length)) return v8::False();
    
    // JQM TODO  shoud unlink here?
    fprintf(stderr, "Unmap -- should also unlink\n");
    buffer->handle_->SetIndexedPropertiesToExternalArrayData(NULL, v8::kExternalUnsignedByteArray, 0);
    buffer->handle_->Set(length_symbol, v8::Integer::NewFromUnsigned(0));
    buffer->handle_.Dispose();
    args.This()->Set(length_symbol, v8::Integer::NewFromUnsigned(0));
    
    return v8::True();
  }

}
#endif




//==================================================================
//  Wait until the FE tag is a particular state, then transition it to the new state
//  Return new tag state
//
unsigned char EMStransitionFEtag(EMStag_t volatile *tag, unsigned char oldFE, unsigned char newFE, unsigned char oldType) {
    RESET_NAP_TIME;
    EMStag_t oldTag;           //  Desired tag value to start of the transition
    EMStag_t newTag;           //  Tag value at the end of the transition
    EMStag_t volatile memTag;  //  Tag value actually stored in memory
    memTag.byte = tag->byte;
    while (oldType == EMS_TAG_ANY || memTag.tags.type == oldType) {
        oldTag.byte = memTag.byte;  // Copy current type and RW count information
        oldTag.tags.fe = oldFE;        // Set the desired start tag state
        newTag.byte = memTag.byte;  // Copy current type and RW count information
        newTag.tags.fe = newFE;        // Set the final tag state

        //  Attempt to transition the state from old to new
        memTag.byte = __sync_val_compare_and_swap(&(tag->byte), oldTag.byte, newTag.byte);
        if (memTag.byte == oldTag.byte) {
            return (newTag.byte);
        } else {
            NANOSLEEP;
            memTag.byte = tag->byte;  // Re-load tag in case was transitioned by another thread
        }
    }
    return (memTag.byte);
}


//==================================================================
//  Hash a string into an integer
//
int64_t EMShashString(const char *key) {
    // TODO BUG MAGIC Max key length
    int charN = 0;
    uint64_t hash = 0;
    while (key[charN] != 0) {
        hash = key[charN] + (hash << 6) + (hash << 16) - hash;
        charN++;
    }
    hash *= 1191613;  // Further scramble to prevent close strings from having close indexes
    return (llabs((int64_t) hash));
}


//==================================================================
//  Find the matching map key for this argument.
//  Returns the index of the element, or -1 for no match.
//  The stored Map key value is read when full and marked busy.
//  If the data does not match, it is marked full again, but if
//  there is a match the map key is kept busy until the operation
//  on the data is complete.
//
int64_t EMSreadIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t idx = 0;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    char *bufChar = emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t mapTags;
    volatile double *bufDouble = (double *) emsBuf;
    int idxType = EMSv8toEMStype(info[0], false);
    int64_t boolArgVal = false;
    int64_t intArgVal = -1;
    double floatArgVal = 0.0;
    int nTries = 0;
    int matched = false;
    int notPresent = false;
    const char *argString = NULL;  // Assignment needed to quiet GCC uninitalized warning

    if (info.Length() == 0) {
        Nan::ThrowTypeError("EMS ERROR: EMSreadIndexMap has no arguments?");
        return -1;
    }

    switch (idxType) {
        case EMS_TYPE_BOOLEAN:
            boolArgVal = (info[0]->ToBoolean()->Value() != 0);
            idx = boolArgVal;
            break;
        case EMS_TYPE_INTEGER:
            intArgVal = info[0]->ToInteger()->Value();
            idx = intArgVal;
            break;
        case EMS_TYPE_FLOAT:
            floatArgVal = info[0]->ToNumber()->Value();
            idx = *((uint64_t *) &floatArgVal);
            break;
        case EMS_TYPE_STRING:
            argString = JS_ARG_TO_CSTR(info[0]);
            idx = EMShashString(argString);
            break;
        default:
            Nan::ThrowTypeError("EMS ERROR: EMSreadIndexMap Unknown arg type");
            return -1;
    }

    if (EMSisMapped) {
        while (nTries < MAX_OPEN_HASH_STEPS && !matched && !notPresent) {
            idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
            // Wait until the map key is FULL, mark it busy while map lookup is performed
            mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
            if (mapTags.tags.type == idxType) {
                switch (idxType) {
                    case EMS_TYPE_BOOLEAN:
                        if (boolArgVal == (bufInt64[EMSmapData(idx)] != 0)) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_INTEGER:
                        if (intArgVal == bufInt64[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_FLOAT:
                        if (floatArgVal == bufDouble[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_STRING: {
                        int64_t keyStrOffset = bufInt64[EMSmapData(idx)];
                        if (strcmp(argString, EMSheapPtr(keyStrOffset)) == 0) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        // Nothing hashed to this map index yet, so the key does not exist
                        notPresent = true;
                        bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        break;
                    default:
                        fprintf(stderr, "EMS ERROR: EMSreadIndexMap: Unknown mem type\n");
                        matched = true;
                }
            }
            if (mapTags.tags.type == EMS_TYPE_UNDEFINED) notPresent = true;
            if (!matched) {
                //  No match, set this Map entry back to full and try again
                bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                nTries++;
                idx++;
            }
        }
        if (!matched) { idx = -1; }
    } else {  // Wasn't mapped, do bounds check
        if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
            idx = -1;
        }
    }

    if (nTries >= MAX_OPEN_HASH_STEPS) {
        fprintf(stderr, "EMSreadIndexMap ran out of key mappings\n");
    }
    if (notPresent) idx = -1;
    return idx;
}


//==================================================================
//  Find the matching map key, if not present, find the
//  next available open address.
//  Reads map key when full and marks Busy to perform comparisons,
//  if it is not a match the data is marked full again, but if it does
//  match, the map key is left empty and this function
//  returns the index of an existing or available array element.
//
int64_t EMSwriteIndexMap(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");

    int64_t idx = 0;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    char *bufChar = (char *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t mapTags;
    volatile double *bufDouble = (double *) emsBuf;
    unsigned char idxType = EMSv8toEMStype(info[0], false);
    int64_t boolArgVal = false;
    int64_t intArgVal = -1;
    double floatArgVal = 0.0;
    char argString[MAX_KEY_LEN + 1];
    strncpy(argString, JS_ARG_TO_CSTR(info[0]), MAX_KEY_LEN);

    if (info.Length() == 0) {
        fprintf(stderr, "EMS ERROR: EMSwriteIndexMap has no arguments?\n");
        return (-1);
    }

    switch (idxType) {
        case EMS_TYPE_BOOLEAN:
            boolArgVal = (info[0]->ToBoolean()->Value() != 0);
            idx = boolArgVal;
            break;
        case EMS_TYPE_INTEGER:
            intArgVal = info[0]->ToInteger()->Value();
            idx = intArgVal;
            break;
        case EMS_TYPE_FLOAT:
            floatArgVal = info[0]->ToNumber()->Value();
            idx = *((int64_t *) &floatArgVal);
            break;
        case EMS_TYPE_STRING:
            idx = EMShashString(argString);
            break;
        case EMS_TYPE_UNDEFINED:
            Nan::ThrowTypeError("EMSwriteIndexMap: Undefined is not a valid index");
            return (-1);
        default:
            fprintf(stderr, "EMS ERROR: EMSwriteIndexMap: Unknown mem type\n");
            return (-1);
    }

    int nTries = 0;
    if (EMSisMapped) {
        int matched = false;
        while (nTries < MAX_OPEN_HASH_STEPS && !matched) {
            idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
            // Wait until the map key is FULL, mark it busy while map lookup is performed
            mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
            mapTags.tags.fe = EMS_TAG_FULL;  // When written back, mark FULL
            if (mapTags.tags.type == idxType || mapTags.tags.type == EMS_TYPE_UNDEFINED) {
                switch (mapTags.tags.type) {
                    case EMS_TYPE_BOOLEAN:
                        if (boolArgVal == (bufInt64[EMSmapData(idx)] != 0)) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_INTEGER:
                        if (intArgVal == bufInt64[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_FLOAT:
                        if (floatArgVal == bufDouble[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_STRING: {
                        int64_t keyStrOffset = bufInt64[EMSmapData(idx)];
                        if (strcmp(argString, EMSheapPtr(keyStrOffset)) == 0) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        // This map key index is still unused, so there was no match.
                        // Instead, allocate this element
                        bufTags[EMSmapTag(idx)].tags.type = idxType;
                        switch (idxType) {
                            case EMS_TYPE_BOOLEAN:
                                bufInt64[EMSmapData(idx)] = boolArgVal;
                                break;
                            case EMS_TYPE_INTEGER:
                                bufInt64[EMSmapData(idx)] = intArgVal;
                                break;
                            case EMS_TYPE_FLOAT:
                                bufDouble[EMSmapData(idx)] = floatArgVal;
                                break;
                            case EMS_TYPE_STRING: {
                                size_t len = strlen(argString);
                                int64_t textOffset;
                                EMS_ALLOC(textOffset, len + 1, "EMSwriteIndexMap(string): out of memory to store string", -1);
                                bufInt64[EMSmapData(idx)] = textOffset;
                                strcpy(EMSheapPtr(textOffset), argString);
                            }
                                break;
                            case EMS_TYPE_UNDEFINED:
                                bufInt64[EMSmapData(idx)] = 0xdeadbeef;
                                break;
                            default:
                                fprintf(stderr, "EMS ERROR: EMSwriteIndexMap: unknown arg type\n");
                        }
                        matched = true;
                        break;
                    default:
                        fprintf(stderr, "EMS ERROR: EMSwriteIndexMap: Unknown mem type\n");
                        matched = true;
                }
            }

            if (!matched) {
                // No match so set this key map back to full and try the next entry
                bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                nTries++;
                idx++;
            }
        }
    } else {  // Wasn't mapped, do bounds check
        if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
            idx = -1;
        }
    }

    if (nTries >= MAX_OPEN_HASH_STEPS) {
        idx = -1;
        fprintf(stderr, "EMSwriteIndexMap ran out of key mappings (returning %" PRIu64 ")\n", idx);
    }

    return idx;
}




//==================================================================
//  Read EMS memory, enforcing Full/Empty tag transitions
//
void EMSreadUsingTags(const Nan::FunctionCallbackInfo<v8::Value>& info, // Index to read from
                      unsigned char initialFE,            // Block until F/E tags are this value
                      unsigned char finalFE)              // Set the tag to this value when done
{
    RESET_NAP_TIME;
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    const char *bufChar = (const char *) emsBuf;
    EMStag_t newTag, oldTag, memTag;

    if (info.Length() < 1 || info.Length() > 2) {
        return Nan::ThrowError("EMSreadUsingTags: Wrong number of args");
    }

    int64_t idx = EMSreadIndexMap(info);
    //  Allocate on Write, writes include modification of the tag:
    //  If the EMS object being read is undefined and we're changing the f/e state
    //  then allocate the undefined object and set the state.  If the state is
    //  not changing, do not allocate the undefined element.
    if(EMSisMapped  &&  idx < 0  && finalFE != EMS_TAG_ANY) {
        idx = EMSwriteIndexMap(info);
        if(idx < 0){
            Nan::ThrowError("EMSreadUsingTags: Unable to allocate on read for new map index");
            return;
        }
    }

    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        if (EMSisMapped) {
            info.GetReturnValue().Set(Nan::Undefined());
            return;
        } else {
            Nan::ThrowError("EMSreadUsingTags: index out of bounds");
            return;
        }
    }

    while (true) {
        memTag.byte = bufTags[EMSdataTag(idx)].byte;
        //  Wait until FE tag is not FULL
        if (initialFE == EMS_TAG_ANY ||
            (initialFE != EMS_TAG_RW_LOCK && memTag.tags.fe == initialFE) ||
            (initialFE == EMS_TAG_RW_LOCK &&
             ((memTag.tags.fe == EMS_TAG_RW_LOCK && newTag.tags.rw < EMS_RW_NREADERS_MAX) || memTag.tags.fe ==
                                                                                             EMS_TAG_FULL) &&
             (memTag.tags.rw < ((1 << EMS_TYPE_NBITS_RW) - 1))// Counter is already saturated
            )
                ) {
            newTag.byte = memTag.byte;
            oldTag.byte = memTag.byte;
            newTag.tags.fe = EMS_TAG_BUSY;
            if (initialFE == EMS_TAG_RW_LOCK) {
                newTag.tags.rw++;
            } else {
                oldTag.tags.fe = initialFE;
            }
            //  Transition FE from FULL to BUSY
            if (initialFE == EMS_TAG_ANY ||
                __sync_bool_compare_and_swap(&(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte)) {
                // Under BUSY lock:
                //   Read the data, then reset the FE tag, then return the original value in memory
                newTag.tags.fe = finalFE;
                switch (newTag.tags.type) {
                    case EMS_TYPE_BOOLEAN: {
                        bool retBool = bufInt64[EMSdataData(idx)];
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        info.GetReturnValue().Set(Nan::New(retBool));
                        return;
                    }
                    case EMS_TYPE_INTEGER: {
                        int32_t retInt = bufInt64[EMSdataData(idx)];  //  TODO: Bug -- only 32 bits of 64?
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        info.GetReturnValue().Set(Nan::New(retInt));
                        return;
                    }
                    case EMS_TYPE_FLOAT: {
                        double retFloat = bufDouble[EMSdataData(idx)];
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        info.GetReturnValue().Set(Nan::New(retFloat));
                        return;
                    }
                    case EMS_TYPE_JSON:
                    case EMS_TYPE_STRING: {
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        if (newTag.tags.type == EMS_TYPE_JSON) {
                            v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
                            retObj->Set(Nan::New("data").ToLocalChecked(),
                                        Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked() );
                            info.GetReturnValue().Set(retObj);
                            return;
                        } else {
                            info.GetReturnValue().Set(Nan::New(EMSheapPtr(bufInt64[EMSdataData(idx)])).ToLocalChecked());
                            return;
                        }
                    }
                    case EMS_TYPE_UNDEFINED: {
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        info.GetReturnValue().Set(Nan::Undefined());
                        return;
                    }
                    default:
                        Nan::ThrowError("EMSreadFE unknown type");
                        return;
                }
            } else {
                // Tag was marked BUSY between test read and CAS, must retry
            }
        } else {
            // Tag was already marked BUSY, must retry
        }
        // CAS failed or memory wasn't in initial state, wait and retry
        NANOSLEEP;
    }
}


//==================================================================
//  Read under multiple readers-single writer lock
void EMSreadRW(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadUsingTags(info, EMS_TAG_RW_LOCK, EMS_TAG_RW_LOCK);
    return;
}


//==================================================================
//  Read when full and leave empty
void EMSreadFE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadUsingTags(info, EMS_TAG_FULL, EMS_TAG_EMPTY);
    return;
}


//==================================================================
//  Read when full and leave Full
void EMSreadFF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadUsingTags(info, EMS_TAG_FULL, EMS_TAG_FULL);
    return;
}


//==================================================================
//   Wrapper around read from an EMS array -- first determine the type
void EMSread(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSreadUsingTags(info, EMS_TAG_ANY, EMS_TAG_ANY);
    return;
}


//==================================================================
//  Decrement the reference counte of the multiple readers-single writer lock
//
void EMSreleaseRW(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    RESET_NAP_TIME;
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t newTag, oldTag;
    if (info.Length() == 1) {
        int64_t idx = EMSreadIndexMap(info);
        if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
            Nan::ThrowError("EMSreleaseRW: invalid index");
            return;
        }
        while (true) {
            oldTag.byte = bufTags[EMSdataTag(idx)].byte;
            newTag.byte = oldTag.byte;
            if (oldTag.tags.fe == EMS_TAG_RW_LOCK) {
                //  Already under a RW lock
                if (oldTag.tags.rw == 0) {
                    //  Assert the RW count is consistent with the lock state
                    Nan::ThrowError("EMSreleaseRW: locked but Count already 0");
                    return;
                } else {
                    //  Decrement the RW reference count
                    newTag.tags.rw--;
                    //  If this is the last reader, set the FE tag back to full
                    if (newTag.tags.rw == 0) { newTag.tags.fe = EMS_TAG_FULL; }
                    //  Attempt to commit the RW reference count & FE tag
                    if (__sync_bool_compare_and_swap(&(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte)) {
                        info.GetReturnValue().Set(Nan::New(newTag.tags.rw));
                        return;
                    } else {
                        // Another thread decremented the RW count while we also tried
                    }
                }
            } else {
                if (oldTag.tags.fe != EMS_TAG_BUSY) {
                    // Assert the RW lock being release is not in some other state then RW_LOCK or BUSY
                    Nan::ThrowError("EMSreleaseRW: Lost RW lock?  Not locked or busy");
                    return;
                }
            }
            // Failed to update the RW count, sleep and retry
            NANOSLEEP;
        }
    } else {
        Nan::ThrowError("EMSreleaseRW: Wrong number of arguments");
        return;
    }
}


//==================================================================
//  Write EMS honoring the F/E tags
//
void EMSwriteUsingTags(const Nan::FunctionCallbackInfo<v8::Value>& info,  // Index to read from
                       unsigned char initialFE,             // Block until F/E tags are this value
                       unsigned char finalFE)               // Set the tag to this value when done
{
    RESET_NAP_TIME;
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    int64_t idx = EMSwriteIndexMap(info);
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    char *bufChar = emsBuf;
    EMStag_t newTag, oldTag, memTag;
    int stringIsJSON = false;

    if (info.Length() == 3) {
        stringIsJSON = info[2]->ToBoolean()->Value();
    } else {
        if (info.Length() != 2) {
            Nan::ThrowError("EMSwriteUsingTags: Wrong number of args");
            return;
        }
    }
    if (idx < 0) {
        Nan::ThrowError("EMSwriteUsingTags: index out of bounds");
        return;
    }

    // Wait for the memory to be in the initial F/E state and transition to Busy
    if (initialFE != EMS_TAG_ANY) {
        EMStransitionFEtag(&bufTags[EMSdataTag(idx)], initialFE, EMS_TAG_BUSY, EMS_TAG_ANY);
    }

    while (true) {
        idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
        memTag.byte = bufTags[EMSdataTag(idx)].byte;
        //  Wait until FE tag is not BUSY
        if (initialFE != EMS_TAG_ANY || finalFE == EMS_TAG_ANY || memTag.tags.fe != EMS_TAG_BUSY) {
            oldTag.byte = memTag.byte;
            newTag.byte = memTag.byte;
            if (finalFE != EMS_TAG_ANY) newTag.tags.fe = EMS_TAG_BUSY;
            //  Transition FE from !BUSY to BUSY
            if (initialFE != EMS_TAG_ANY || finalFE == EMS_TAG_ANY ||
                __sync_bool_compare_and_swap(&(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte)) {
                //  If the old data was a string, free it because it will be overwritten
                if (oldTag.tags.type == EMS_TYPE_STRING || oldTag.tags.type == EMS_TYPE_JSON) {
                    EMS_FREE(bufInt64[EMSdataData(idx)]);
                }

                // Store argument value into EMS memory
                switch (EMSv8toEMStype(info[1], stringIsJSON)) {
                    case EMS_TYPE_BOOLEAN:
                        bufInt64[EMSdataData(idx)] = info[1]->ToBoolean()->Value();
                        break;
                    case EMS_TYPE_INTEGER:
                        bufInt64[EMSdataData(idx)] = (int64_t) info[1]->ToInteger()->Value();
                        break;
                    case EMS_TYPE_FLOAT:
                        bufDouble[EMSdataData(idx)] = info[1]->ToNumber()->Value();
                        break;
                    case EMS_TYPE_JSON:
                    case EMS_TYPE_STRING: {
                        const char *json = JS_ARG_TO_CSTR(info[1]);
                        size_t len = strlen(json);
                        int64_t textOffset;
                        EMS_ALLOC(textOffset, len + 1, "EMSwriteUsingTags: out of memory to store string", );
                        bufInt64[EMSdataData(idx)] = textOffset;
                        strcpy(EMSheapPtr(textOffset), json);
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        bufInt64[EMSdataData(idx)] = 0xdeadbeef;
                        break;
                    default:
                        Nan::ThrowError("EMSwriteUsingTags: Unknown arg type");
                        return;
                }

                oldTag.byte = newTag.byte;
                if (finalFE != EMS_TAG_ANY) {
                    newTag.tags.fe = finalFE;
                    newTag.tags.rw = 0;
                }
                newTag.tags.type = EMSv8toEMStype(info[1], stringIsJSON);
                if (finalFE != EMS_TAG_ANY && bufTags[EMSdataTag(idx)].byte != oldTag.byte) {
                    Nan::ThrowError("EMSwriteUsingTags: Lost tag lock while BUSY");
                    return;
                }

                //  Set the tags for the data (and map, if used) back to full to finish the operation
                bufTags[EMSdataTag(idx)].byte = newTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                info.GetReturnValue().Set(Nan::True());
                return;
            } else {
                // Tag was marked BUSY between test read and CAS, must retry
            }
        } else {
            // Tag was already marked BUSY, must retry
        }
        //  Failed to set the tags, sleep and retry
        NANOSLEEP;
    }
}


//==================================================================
//  WriteXF
void EMSwriteXF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteUsingTags(info, EMS_TAG_ANY, EMS_TAG_FULL);
    return;
}

//==================================================================
//  WriteXE
void EMSwriteXE(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteUsingTags(info, EMS_TAG_ANY, EMS_TAG_EMPTY);
    return;
}

//==================================================================
//  WriteEF
void EMSwriteEF(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteUsingTags(info, EMS_TAG_EMPTY, EMS_TAG_FULL);
    return;
}

//==================================================================
//  Write
void EMSwrite(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    EMSwriteUsingTags(info, EMS_TAG_ANY, EMS_TAG_ANY);
    return;
}


//==================================================================
//  Set only the Full/Empty tag  from JavaScript 
//  without inspecting or modifying the data.
//
void EMSsetTag(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t tag;
    int64_t idx = info[0]->ToInteger()->Value();

    tag.byte = bufTags[EMSdataTag(idx)].byte;
    if (info[1]->ToBoolean()->Value()) {
        tag.tags.fe = EMS_TAG_FULL;
    } else {
        tag.tags.fe = EMS_TAG_EMPTY;
    }
    bufTags[EMSdataTag(idx)].byte = tag.byte;
}


//==================================================================
//  Return the key of a mapped object given the EMS index
void EMSindex2key(const Nan::FunctionCallbackInfo<v8::Value> &info) {
    THIS_INFO_TO_EMSBUF(info, "mmapID");
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    char *bufChar = emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;

    if(!EMSisMapped) {
        Nan::ThrowError("EMSindex2key: Unmapping an index but Array is not mapped");
        return;
    }

    int64_t idx = info[0]->ToInteger()->Value();
    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        Nan::ThrowError("EMSindex2key: index out of bounds");
        return;
    }

    switch (bufTags[EMSmapTag(idx)].tags.type) {
        case EMS_TYPE_BOOLEAN: {
            bool retBool = bufInt64[EMSmapData(idx)];
            info.GetReturnValue().Set(Nan::New(retBool));
            return;
        }
        case EMS_TYPE_INTEGER: {
            int32_t retInt = bufInt64[EMSmapData(idx)];  //  TODO: Bug -- only 32 bits of 64?
            info.GetReturnValue().Set(Nan::New(retInt));
            return;
        }
        case EMS_TYPE_FLOAT: {
            double retFloat = bufDouble[EMSmapData(idx)];
            info.GetReturnValue().Set(Nan::New(retFloat));
            return;
        }
        case EMS_TYPE_JSON: {
            v8::Local<v8::Object> retObj = Nan::New<v8::Object>();
            retObj->Set(Nan::New("data").ToLocalChecked(), 
                        Nan::New(EMSheapPtr(bufInt64[EMSmapData(idx)])).ToLocalChecked());
            info.GetReturnValue().Set(retObj);
            return;
        }
        case EMS_TYPE_STRING: {
            info.GetReturnValue().Set(Nan::New(EMSheapPtr(bufInt64[EMSmapData(idx)])).ToLocalChecked());
            return;
        }
        case EMS_TYPE_UNDEFINED: {
            info.GetReturnValue().Set(Nan::Undefined());
            return;
        }
        default:
            Nan::ThrowTypeError("EMSindex2key unknown type");
            return;
    }
}




//==================================================================
//  Synchronize the EMS memory to persistent storage
//
void EMSsync(const Nan::FunctionCallbackInfo<v8::Value>& info) {
#if 0
    v8::HandleScope scope;
    EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag_t   *bufTags   = (EMStag_t *) emsBuf;
  int64_t    idx;
  if(args[0]->IsUndefined()) {
    idx    = 0;
  } else {
    idx    = args[0]->ToInteger()->Value();
  }
  
  EMStag_t     tag;
  tag.byte = bufTags[EMSdataTag(idx)].byte;
  int resultIdx = 0;
  int resultTag = 0;
  int resultStr = 0;
#if 1
  fprintf(stderr, "msync not complete\n");
  resultStr = 1;
#else
  int flags = MS_SYNC;
  char     *bufChar   = (char *) emsBuf;
  int64_t   pgsize    = getpagesize(); //sysconf(_SC_PAGE_SIZE);
#define PGALIGN(addr) ((void*) (( ((uint64_t)addr) / pgsize) * pgsize ))

  //fprintf(stderr, "msync%lx  %" PRIu64 "  %d\n", emsBuf, length, flags);
  resultIdx = msync((void*) emsBuf, pgsize, flags);
  /*
  if(tag.tags.type != EMS_TYPE_UNDEFINED)
    //resultIdx = msync(&(bufInt64[idx]), sizeof(int64_t), flags);
    resultIdx = msync( PGALIGN(&bufInt64[idx]), pgsize, flags);
  if(tag.tags.type  ==  EMS_TYPE_STRING)
    //resultStr = msync(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]]),
    //	      strlen(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]])),
    // flags);
    resultStr = msync( PGALIGN( &(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]]) ),
               strlen(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]])),
               flags);
  //resultTag = msync(&(bufTags[EMSdataTag(idx)].byte), 1, flags);
  //fprintf(stderr, "msync(%llx  %" PRIu64 "  %d\n", PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), pgsize, flags);
  //  resultTag = msync(PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), pgsize, flags);
  //  resultTag = msync(PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), 1, flags);
  //fprintf(stderr, "result  %d  %d  %d\n", resultIdx, resultStr, resultTag);
*/
#endif
  if(resultIdx == 0  &&  resultStr == 0  &&  resultTag == 0)
    return v8::True();
  else
    return v8::False();
#else
    printf("EMSsync() was called but stubbed out\n");
#endif
}


//==================================================================
//  EMS Entry Point:   Allocate and initialize the EMS domain memory
//
void initialize(const Nan::FunctionCallbackInfo<v8::Value>& info) {
    if (info.Length() < 14) {
        Nan::ThrowError("initialize: Missing arguments");
        return;
    }

    //  Parse all the arguments
    int64_t nElements  = info[0]->ToInteger()->Value();
    int64_t heapSize   = info[1]->ToInteger()->Value();
    int64_t useMap         = info[2]->ToBoolean()->Value();
    char filename[MAX_FNAME_LEN + 1];
    strncpy(filename, JS_ARG_TO_CSTR(info[3]), MAX_FNAME_LEN);

    int64_t persist        = info[4]->ToBoolean()->Value();
    int64_t useExisting    = info[5]->ToBoolean()->Value();
    int64_t doDataFill     = info[6]->ToBoolean()->Value();
    // Data Fill type TBD during fill
    int64_t fillIsJSON     = info[8]->ToBoolean()->Value();
    int64_t doSetFEtags    = info[9]->ToBoolean()->Value();
    int64_t setFEtags  = info[10]->ToInteger()->Value();
    EMSmyID            = info[11]->ToInteger()->Value();
    int64_t pinThreads = info[12]->ToBoolean()->Value();
    int64_t nThreads   = info[13]->ToInteger()->Value();
    int64_t pctMLock   = info[14]->ToInteger()->Value();
    int fd;

    //  Node 0 is first and always has mutual excusion during intialization
    //  perform once-only initialization here
    if (EMSmyID == 0) {
        if (!useExisting) {
            unlink(filename);
            shm_unlink(filename);
        }
    }

    if (useExisting) {
        struct timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 200000000;
        struct stat statbuf;
        while (stat(filename, &statbuf) != 0) nanosleep(&sleep_time, NULL);
    }

    if (persist)
        fd = open(filename, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    else
        // fd = shm_open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        fd = open(filename, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd < 0) {
        perror("What happened?");
        Nan::ErrnoException(errno, "EMS", "Unable to open file");
        return;
    }

    size_t nMemBlocks = (heapSize / EMS_MEM_BLOCKSZ) + 1;
    size_t nMemBlocksPow2 = emsNextPow2(nMemBlocks);
    size_t nMemLevels = __builtin_ctzl(nMemBlocksPow2);
    size_t bottomOfMap = -1;
    size_t bottomOfMalloc = -1;
    size_t bottomOfHeap = -1;
    int64_t filesize;

    bottomOfMap = EMSdataTagWord(nElements) + EMSwordSize;  // Map begins 1 word AFTER the last tag word of data
    if (useMap) {
        bottomOfMalloc = bottomOfMap + bottomOfMap;
    } else {
        bottomOfMalloc = bottomOfMap;
    }
    bottomOfHeap = bottomOfMalloc + sizeof(struct emsMem) + (nMemBlocksPow2 * 2 - 2);

    if (nElements <= 0) {
        filesize = EMS_CB_LOCKS + nThreads;   // EMS Control Block
        filesize *= sizeof(int);
    } else {
        filesize = bottomOfHeap + (nMemBlocksPow2 * EMS_MEM_BLOCKSZ);
    }
    if (ftruncate(fd, filesize) != 0) {
        if (errno != EINVAL) {
            fprintf(stderr, "EMS: Error during initialization, unable to set memory size to %" PRIu64 " bytes\n",
                    filesize);
            Nan::ErrnoException(errno, "EMS", "Unable to resize domain memory");
            return;
        }
    }

    char *emsBuf = (char *) mmap(0, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t) 0);
    if (emsBuf == MAP_FAILED) {
        Nan::ErrnoException(errno, "EMS", "Unable to map domain memory");
    }
    close(fd);

    if (nElements <= 0) pctMLock = 100;   // lock RAM if master control block
    if (mlock((void *) emsBuf, (size_t) (filesize * (pctMLock / 100))) != 0) {
        fprintf(stderr, "NOTICE: EMS thread %d was not able to lock EMS memory to RAM for %s\n", EMSmyID, filename);
    } else {
        // success
    }

    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    volatile int *bufInt32 = (int32_t *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;

    if (EMSmyID == 0) {
        if (nElements <= 0) {   // This is the EMS CB
            bufInt32[EMS_CB_NTHREADS] = nThreads;
            bufInt32[EMS_CB_NBAR0] = nThreads;
            bufInt32[EMS_CB_NBAR1] = nThreads;
            bufInt32[EMS_CB_BARPHASE] = 0;
            bufInt32[EMS_CB_CRITICAL] = 0;
            bufInt32[EMS_CB_SINGLE] = 0;
            for (int i = EMS_CB_LOCKS; i < EMS_CB_LOCKS + nThreads; i++) {
                bufInt32[i] = EMS_TAG_BUSY;  //  Reset all locks
            }
        } else {   //  This is a user data domain
            if (!useExisting) {
                EMStag_t tag;
                tag.tags.rw = 0;
                tag.tags.type = EMS_TYPE_INTEGER;
                tag.tags.fe = EMS_TAG_FULL;
                bufInt64[EMScbData(EMS_ARR_NELEM)] = nElements;
                bufInt64[EMScbData(EMS_ARR_HEAPSZ)] = heapSize;     // Unused?
                bufInt64[EMScbData(EMS_ARR_MAPBOT)] = bottomOfMap / EMSwordSize;
                bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] = bottomOfMalloc;
                bufInt64[EMScbData(EMS_ARR_HEAPBOT)] = bottomOfHeap;
                bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] = 0;
                bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].byte = tag.byte;
                bufInt64[EMScbData(EMS_ARR_STACKTOP)] = 0;
                bufTags[EMScbTag(EMS_ARR_STACKTOP)].byte = tag.byte;
                bufInt64[EMScbData(EMS_ARR_MEM_MUTEX)] = EMS_TAG_EMPTY;
                bufInt64[EMScbData(EMS_ARR_FILESZ)] = filesize;
                struct emsMem *emsMemBuffer = (struct emsMem *) &bufChar[bufInt64[EMScbData(EMS_ARR_MALLOCBOT)]];
                emsMemBuffer->level = nMemLevels;
            }
        }
    }

    if (pinThreads) {
#if defined(__linux)
        cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((EMSmyID % nThreads), &cpuset);  // Round-robin over-subscribed systems
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
#endif
    }
    EMStag_t tag;
    tag.tags.rw = 0;
    int64_t iterPerThread = (nElements / nThreads) + 1;
    int64_t startIter = iterPerThread * EMSmyID;
    int64_t endIter = iterPerThread * (EMSmyID + 1);
    if (endIter > nElements) endIter = nElements;
    for (int64_t idx = startIter; idx < endIter; idx++) {
        tag.tags.rw = 0;
        if (doDataFill) {
            tag.tags.type = EMSv8toEMStype(info[7], fillIsJSON);
            switch (tag.tags.type) {
                case EMS_TYPE_INTEGER:
                    bufInt64[EMSdataData(idx)] = info[7]->ToInteger()->Value();
                    break;
                case EMS_TYPE_FLOAT:
                    bufDouble[EMSdataData(idx)] = info[7]->ToNumber()->Value();
                    break;
                case EMS_TYPE_UNDEFINED:
                    bufInt64[EMSdataData(idx)] = 0xdeadbeef;
                    break;
                case EMS_TYPE_BOOLEAN:
                    bufInt64[EMSdataData(idx)] = info[7]->ToBoolean()->Value();
                    break;
                case EMS_TYPE_JSON:
                case EMS_TYPE_STRING: {
                    const char *fill_str = JS_ARG_TO_CSTR(info[7]);
                    int len = strlen(fill_str);
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, len + 1, "EMSinit: out of memory to store string", );
                    bufInt64[EMSdataData(idx)] = textOffset;
                    strcpy(EMSheapPtr(textOffset), fill_str);
                }
                    break;
                default:
                    Nan::ErrnoException(errno, "EMS", "EMSinit: type is unknown");
                    return;
            }
        } else {
            tag.tags.type = EMS_TYPE_UNDEFINED;
        }

        if (doSetFEtags) {
            if (setFEtags) tag.tags.fe = EMS_TAG_FULL;
            else tag.tags.fe = EMS_TAG_EMPTY;
        }

        if (doSetFEtags || doDataFill) {
            bufTags[EMSdataTag(idx)].byte = tag.byte;
        }

        if (useMap) {
            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            if (!useExisting) { bufTags[EMSmapTag(idx)].tags.type = EMS_TYPE_UNDEFINED; }
        }
    }

    // ========================================================================================
    v8::Local<v8::Object> obj = Nan::New<v8::Object>();
    int emsBufN = 0;
    while(emsBufN < EMS_MAX_N_BUFS  &&  emsBufs[emsBufN] != NULL)  emsBufN++;
    if(emsBufN < EMS_MAX_N_BUFS) {
        emsBufs[emsBufN] = emsBuf;
    } else {
        fprintf(stderr, "ERROR: Unable to allocate a buffer ID/index\n");
    }
    obj->Set(Nan::New("mmapID").ToLocalChecked(), Nan::New(emsBufN));

#define ADD_FUNC_TO_V8_OBJ(obj, func_name, func) \
    { \
        v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(func); \
        v8::Local<v8::Function> fn = tpl->GetFunction(); \
        fn->SetName(Nan::New(func_name).ToLocalChecked()); \
        obj->Set(Nan::New(func_name).ToLocalChecked(), tpl->GetFunction()); \
    }

    ADD_FUNC_TO_V8_OBJ(obj, "initialize", initialize);
    ADD_FUNC_TO_V8_OBJ(obj, "faa", EMSfaa);
    ADD_FUNC_TO_V8_OBJ(obj, "cas", EMScas);
    ADD_FUNC_TO_V8_OBJ(obj, "read", EMSread);
    ADD_FUNC_TO_V8_OBJ(obj, "write", EMSwrite);
    ADD_FUNC_TO_V8_OBJ(obj, "readRW", EMSreadRW);
    ADD_FUNC_TO_V8_OBJ(obj, "releaseRW", EMSreleaseRW);
    ADD_FUNC_TO_V8_OBJ(obj, "readFE", EMSreadFE);
    ADD_FUNC_TO_V8_OBJ(obj, "readFF", EMSreadFF);
    ADD_FUNC_TO_V8_OBJ(obj, "setTag", EMSsetTag);
    ADD_FUNC_TO_V8_OBJ(obj, "writeEF", EMSwriteEF);
    ADD_FUNC_TO_V8_OBJ(obj, "writeXF", EMSwriteXF);
    ADD_FUNC_TO_V8_OBJ(obj, "writeXE", EMSwriteXE);
    ADD_FUNC_TO_V8_OBJ(obj, "push", EMSpush);
    ADD_FUNC_TO_V8_OBJ(obj, "pop", EMSpop);
    ADD_FUNC_TO_V8_OBJ(obj, "enqueue", EMSenqueue);
    ADD_FUNC_TO_V8_OBJ(obj, "dequeue", EMSdequeue);
    ADD_FUNC_TO_V8_OBJ(obj, "sync", EMSsync);
    ADD_FUNC_TO_V8_OBJ(obj, "index2key", EMSindex2key);
    info.GetReturnValue().Set(obj);

    return;
}



//---------------------------------------------------------------
static void RegisterModule(v8::Handle <v8::Object> target) {
    ADD_FUNC_TO_V8_OBJ(target, "initialize", initialize);
    ADD_FUNC_TO_V8_OBJ(target, "barrier", EMSbarrier);
    ADD_FUNC_TO_V8_OBJ(target, "singleTask", EMSsingleTask);
    ADD_FUNC_TO_V8_OBJ(target, "criticalEnter", EMScriticalEnter);
    ADD_FUNC_TO_V8_OBJ(target, "criticalExit", EMScriticalExit);
    ADD_FUNC_TO_V8_OBJ(target, "loopInit", EMSloopInit);
    ADD_FUNC_TO_V8_OBJ(target, "loopChunk", EMSloopChunk);
    // ADD_FUNC_TO_V8_OBJ(target, "sync", EMSsync);
    // ADD_FUNC_TO_V8_OBJ(target, "length");
    // ADD_FUNC_TO_V8_OBJ(target, "Buffer");
}

NODE_MODULE(ems, RegisterModule);
