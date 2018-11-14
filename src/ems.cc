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
#include "ems.h"

//==================================================================
//  Resolve External Declarations
//
int EMSmyID = -1;   // EMS Process ID
char   *emsBufs[EMS_MAX_N_BUFS] = { NULL };
size_t  emsBufLengths[EMS_MAX_N_BUFS] = { 0 };
char    emsBufFilenames[EMS_MAX_N_BUFS][MAX_FNAME_LEN] = { { 0 } };

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





//==================================================================
//  Convert any type of key to an index
//
int64_t EMSkey2index(void *emsBuf, EMSvalueType *key, bool is_mapped) {
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    const char *bufChar = (char *) emsBuf;

    int64_t idx = 0;
    switch (key->type) {
        case EMS_TYPE_BOOLEAN:
            if ((bool) key->value)  idx = 1;
            else                    idx = 0;
            break;
        case EMS_TYPE_INTEGER:
            idx = llabs((int64_t) key->value);
            break;
        case EMS_TYPE_FLOAT:
            idx = llabs((int64_t) key->value);
            break;
        case EMS_TYPE_STRING:
            idx = EMShashString((char *) key->value);
            break;
        case EMS_TYPE_UNDEFINED:
            fprintf(stderr, "EMS ERROR: EMSkey2index keyType is defined as Undefined\n");
            return -1;
        default:
            fprintf(stderr, "EMS ERROR: EMSkey2index keyType(%d) is unknown\n", key->type);
            return -1;
    }

    int nTries = 0;
    bool matched = false;
    bool notPresent = false;
    EMStag_t mapTags;
    if (is_mapped) {
        while (nTries < MAX_OPEN_HASH_STEPS && !matched && !notPresent) {
            idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
            // Wait until the map key is FULL, mark it busy while map lookup is performed
            mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
            if (mapTags.tags.type == key->type) {
                switch (key->type) {
                    case EMS_TYPE_BOOLEAN:
                    case EMS_TYPE_INTEGER:
                        if ((int64_t) key->value == bufInt64[EMSmapData(idx)]) {
                            matched = true;
                        }
                        break;
                    case EMS_TYPE_FLOAT: {
                        EMSulong_double alias;
                        alias.u64 = (uint64_t) key->value;
                        if (alias.d == bufDouble[EMSmapData(idx)]) {
                            matched = true;
                        }
                    }
                        break;
                    case EMS_TYPE_STRING: {
                        int64_t keyStrOffset = bufInt64[EMSmapData(idx)];
                        if (strcmp((const char *) key->value, EMSheapPtr(keyStrOffset)) == 0) {
                            matched = true;
                        }
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        // Nothing hashed to this map index yet, so the key does not exist
                        notPresent = true;
                        break;
                    default:
                        fprintf(stderr, "EMS ERROR: EMSreadIndexMap: Unknown mem type\n");
                        matched = true;
                }
            }
            if (mapTags.tags.type == EMS_TYPE_UNDEFINED) notPresent = true;
            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            if (!matched) {
                //  No match, set this Map entry back to full and try again
                nTries++;
                idx++;
            }
        }
        if (!matched) { idx = -1; }
    }

    int64_t retval = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
    return retval;
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
unsigned char EMStransitionFEtag(EMStag_t volatile *tag, EMStag_t volatile *mapTag,
                                 unsigned char oldFE, unsigned char newFE, unsigned char oldType) {
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
            // Allow preemptive map acquisition while waiting for data
            if (mapTag) { mapTag->tags.fe = EMS_TAG_FULL; }
            NANOSLEEP;
            if (mapTag) { EMStransitionFEtag(mapTag, NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY); }
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
//  Find the matching map key, if not present, find the
//  next available open address.
//  Reads map key when full and marks Busy to perform comparisons,
//  if it is not a match the data is marked full again, but if it does
//  match, the map key is left empty and this function
//  returns the index of an existing or available array element.
//
int64_t EMSwriteIndexMap(const int mmapID, EMSvalueType *key) {
    char *emsBuf = emsBufs[mmapID];
    volatile int64_t  *bufInt64  = (int64_t *) emsBuf;
    volatile char     *bufChar   = emsBuf;
    volatile EMStag_t *bufTags   = (EMStag_t *) emsBuf;
    volatile double   *bufDouble = (double *) emsBuf;
    EMStag_t mapTags;

    //  If the key already exists, use it
    int64_t idx = EMSkey2index(emsBuf, key, EMSisMapped);
    if(idx > 0) {
        // fprintf(stderr, "write index map -- key already existed\n");
        return idx;
    }
    idx = EMSkey2index(emsBuf, key, false);
    int nTries = 0;
    if (EMSisMapped) {
        int matched = false;
        while (nTries < MAX_OPEN_HASH_STEPS && !matched) {
            idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
            // Wait until the map key is FULL, mark it busy while map lookup is performed
            mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
            mapTags.tags.fe = EMS_TAG_FULL;  // When written back, mark FULL
            if (mapTags.tags.type == key->type || mapTags.tags.type == EMS_TYPE_UNDEFINED) {
                switch (mapTags.tags.type) {
                    case EMS_TYPE_BOOLEAN:
                        if ((int64_t) key->value == (bufInt64[EMSmapData(idx)] != 0)) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_INTEGER:
                        if ((int64_t) key->value == bufInt64[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                        break;
                    case EMS_TYPE_FLOAT: {
                        EMSulong_double alias;
                        alias.u64 = (uint64_t) key->value;
                        if (alias.d == bufDouble[EMSmapData(idx)]) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                    }
                        break;
                    case EMS_TYPE_STRING: {
                        int64_t keyStrOffset = bufInt64[EMSmapData(idx)];
                        if (strcmp((const char *) key->value, (const char *) EMSheapPtr(keyStrOffset)) == 0) {
                            matched = true;
                            bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        }
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        // This map key index is still unused, so there was no match and a new
                        // mapped element must be allocated to perform the tag bit transitions upon
                        bufTags[EMSmapTag(idx)].tags.type = key->type;
                        switch (key->type) {
                            case EMS_TYPE_BOOLEAN:
                                bufInt64[EMSmapData(idx)] = ((int64_t) key->value !=  0);
                                break;
                            case EMS_TYPE_INTEGER:
                                bufInt64[EMSmapData(idx)] = (int64_t) key->value;
                                break;
                            case EMS_TYPE_FLOAT: {
                                EMSulong_double alias;
                                alias.u64 = (uint64_t) key->value;
                                bufDouble[EMSmapData(idx)] = alias.d;
                            }
                                break;
                            case EMS_TYPE_STRING: {
                                int64_t textOffset;
                                EMS_ALLOC(textOffset, key->length + 1, bufChar,
                                          "EMSwriteIndexMap(string): out of memory to store string", -1);
                                bufInt64[EMSmapData(idx)] = textOffset;
                                strcpy((char *) EMSheapPtr(textOffset), (const char *) key->value);
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
                        fprintf(stderr, "EMS ERROR: EMSwriteIndexMap: Unknown tag type (%d) on map key\n", mapTags.tags.type);
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
            fprintf(stderr, "Wasn't mapped do bounds check\n");
            idx = -1;
        }
    }

    if (nTries >= MAX_OPEN_HASH_STEPS) {
        idx = -1;
        fprintf(stderr, "EMSwriteIndexMap ran out of key mappings (ntries=%d)\n", nTries);
    }

    return idx;
}



//==================================================================
//  Read EMS memory, enforcing Full/Empty tag transitions
bool EMSreadUsingTags(const int mmapID,
                      EMSvalueType *key, // Index to read from
                      EMSvalueType *returnValue,
                      unsigned char initialFE,            // Block until F/E tags are this value
                      unsigned char finalFE)              // Set the tag to this value when done
{
    RESET_NAP_TIME;
    void *emsBuf = emsBufs[mmapID];
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    const char *bufChar = (const char *) emsBuf;

    returnValue->type  = EMS_TYPE_UNDEFINED;
    returnValue->value = (void *) 0xdeafbeef;  // TODO: Should return default value even when not doing write allocate

    EMStag_t newTag, oldTag, memTag;
    int64_t idx = EMSkey2index(emsBuf, key, EMSisMapped);

    //  Allocate on Write, writes include modification of the tag:
    //  If the EMS object being read is undefined and we're changing the f/e state
    //  then allocate the undefined object and set the state.  If the state is
    //  not changing, do not allocate the undefined element.
    if(EMSisMapped  &&  idx < 0) {
        if (finalFE != EMS_TAG_ANY) {
            idx = EMSwriteIndexMap(mmapID, key);
            if (idx < 0) {
                fprintf(stderr, "EMSreadUsingTags: Unable to allocate on read for new map index\n");
                return false;
            }
        } else {
            return true;
        }
    }

    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMSreadUsingTags: index out of bounds\n");
        return false;
    }

    while (true) {
        memTag.byte = bufTags[EMSdataTag(idx)].byte;
        //  Wait until FE tag is not FULL
        if (initialFE == EMS_TAG_ANY ||
            (initialFE != EMS_TAG_RW_LOCK && memTag.tags.fe == initialFE) ||
            (initialFE == EMS_TAG_RW_LOCK &&
             ((memTag.tags.fe == EMS_TAG_RW_LOCK && newTag.tags.rw < EMS_RW_NREADERS_MAX) ||
              memTag.tags.fe == EMS_TAG_FULL) &&
             (memTag.tags.rw < ((1 << EMS_TYPE_NBITS_RW) - 1))  // Counter is already saturated
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
                returnValue->type  = newTag.tags.type;
                switch (newTag.tags.type) {
                    case EMS_TYPE_BOOLEAN: {
                        returnValue->value = (void *) (bufInt64[EMSdataData(idx)] != 0);
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        return true;
                    }
                    case EMS_TYPE_INTEGER: {
                        returnValue->value = (void *) bufInt64[EMSdataData(idx)];
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        return true;
                    }
                    case EMS_TYPE_FLOAT: {
                        EMSulong_double alias;
                        alias.d = bufDouble[EMSdataData(idx)];
                        returnValue->value = (void *) alias.u64;
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        return true;
                    }
                    case EMS_TYPE_BUFFER:
                    case EMS_TYPE_JSON:
                    case EMS_TYPE_STRING: {
                        returnValue->value = (void *) EMSheapPtr(bufInt64[EMSdataData(idx)]);
                        returnValue->length = strlen((const char *)returnValue->value); // TODO:
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        return true;
                    }
                    case EMS_TYPE_UNDEFINED: {
                        returnValue->value = (void *) 0xcafebeef;
                        if (finalFE != EMS_TAG_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
                        if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                        return true;
                    }
                    default:
                        fprintf(stderr, "EMSreadUsingTags: unknown type (%d) read from memory\n", newTag.tags.type);
                        return false;
                }
            } else {
                // Tag was marked BUSY between test read and CAS, must retry
            }
        } else {
            // Tag was already marked BUSY, must retry
        }
        // CAS failed or memory wasn't in initial state, wait and retry.
        // Permit preemptive map acquisition while waiting for data.
        if (EMSisMapped) { bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL; }
        NANOSLEEP;
        if (EMSisMapped) {
            EMStransitionFEtag(&bufTags[EMSmapTag(idx)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
        }
    }
}


//==================================================================
//  Read under multiple readers-single writer lock
bool EMSreadRW(const int mmapID, EMSvalueType *key, EMSvalueType *returnValue) {
    return EMSreadUsingTags(mmapID, key, returnValue, EMS_TAG_RW_LOCK, EMS_TAG_RW_LOCK);
}


//==================================================================
//  Read when full and leave empty
bool EMSreadFE(const int mmapID, EMSvalueType *key, EMSvalueType *returnValue) {
    return EMSreadUsingTags(mmapID, key, returnValue, EMS_TAG_FULL, EMS_TAG_EMPTY);
}


//==================================================================
//  Read when full and leave Full
bool EMSreadFF(const int mmapID, EMSvalueType *key, EMSvalueType *returnValue) {
    return EMSreadUsingTags(mmapID, key, returnValue, EMS_TAG_FULL, EMS_TAG_FULL);
}


//==================================================================
//   Wrapper around read
bool EMSread(const int mmapID, EMSvalueType *key, EMSvalueType *returnValue) {
    return EMSreadUsingTags(mmapID, key, returnValue, EMS_TAG_ANY, EMS_TAG_ANY);
}


//==================================================================
//  Decrement the reference count of the multiple readers-single writer lock
int EMSreleaseRW(const int mmapID, EMSvalueType *key) {
    RESET_NAP_TIME;
    void *emsBuf = emsBufs[mmapID];
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t newTag, oldTag;
    int64_t idx = EMSkey2index(emsBuf, key, EMSisMapped);
    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMSreleaseRW: invalid index (%" PRIi64 ")\n", idx);
        return -1;
    }

    while (true) {
        oldTag.byte = bufTags[EMSdataTag(idx)].byte;
        newTag.byte = oldTag.byte;
        if (oldTag.tags.fe == EMS_TAG_RW_LOCK) {
            //  Already under a RW lock
            if (oldTag.tags.rw == 0) {
                //  Assert the RW count is consistent with the lock state
                fprintf(stderr, "EMSreleaseRW: locked but Count already 0\n");
                return -1;
            } else {
                //  Decrement the RW reference count
                newTag.tags.rw--;
                //  If this is the last reader, set the FE tag back to full
                if (newTag.tags.rw == 0) { newTag.tags.fe = EMS_TAG_FULL; }
                //  Attempt to commit the RW reference count & FE tag
                if (__sync_bool_compare_and_swap(&(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte)) {
                    return (int) newTag.tags.rw;
                } else {
                    // Another thread decremented the RW count while we also tried
                }
            }
        } else {
            if (oldTag.tags.fe != EMS_TAG_BUSY) {
                // Assert the RW lock being release is not in some other state then RW_LOCK or BUSY
                fprintf(stderr, "EMSreleaseRW: The RW lock being released is in some other state then RW_LOCK or BUSY\n");
                return -1;
            }
        }
        // Failed to update the RW count, sleep and retry
        NANOSLEEP;
    }
}


//==================================================================
//  Write EMS honoring the F/E tags
bool EMSwriteUsingTags(int mmapID,
                       EMSvalueType *key,
                       EMSvalueType *value,
                       unsigned char initialFE,             // Block until F/E tags are this value
                       unsigned char finalFE)               // Set the tag to this value when done
{
    RESET_NAP_TIME;
    char *emsBuf = emsBufs[mmapID];
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    char *bufChar = emsBuf;
    EMStag_t newTag, oldTag, memTag;
    int64_t idx = EMSwriteIndexMap(mmapID, key);
    if (idx < 0) {
        fprintf(stderr, "EMSwriteUsingTags: index out of bounds\n");
        return false;
    }

    // Wait for the memory to be in the initial F/E state and transition to Busy
    if (initialFE != EMS_TAG_ANY) {
        volatile EMStag_t *maptag;
        if (EMSisMapped) { maptag = &bufTags[EMSmapTag(idx)]; }
        else             { maptag = NULL; }
        EMStransitionFEtag(&bufTags[EMSdataTag(idx)], maptag,
                           initialFE, EMS_TAG_BUSY, EMS_TAG_ANY);
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
                if (oldTag.tags.type == EMS_TYPE_STRING || oldTag.tags.type == EMS_TYPE_JSON || oldTag.tags.type == EMS_TYPE_BUFFER) {
                    EMS_FREE(bufInt64[EMSdataData(idx)]);
                }

                // Store argument value into EMS memory
                switch (value->type) {
                    case EMS_TYPE_BOOLEAN:
                        bufInt64[EMSdataData(idx)] = (int64_t) value->value;
                        break;
                    case EMS_TYPE_INTEGER:
                        bufInt64[EMSdataData(idx)] = (int64_t) value->value;
                        break;
                    case EMS_TYPE_FLOAT: {
                        EMSulong_double alias;
                        alias.u64 = (uint64_t) value->value;
                        bufDouble[EMSdataData(idx)] = alias.d;
                    }
                        break;
                    case EMS_TYPE_BUFFER: {
                        int64_t byteOffset;
                        EMS_ALLOC(byteOffset, value->length, bufChar,  // TODO: NULL padding at end
                                  "EMSwriteUsingTags: out of memory to store buffer", false);
                        bufInt64[EMSdataData(idx)] = byteOffset;
                        memcpy(EMSheapPtr(byteOffset), (const char *) value->value, value->length);
                    }
                    case EMS_TYPE_JSON:
                    case EMS_TYPE_STRING: {
                        int64_t textOffset;
                        EMS_ALLOC(textOffset, value->length + 1, bufChar,  // NULL padding at end
                                  "EMSwriteUsingTags: out of memory to store string", false);
                        bufInt64[EMSdataData(idx)] = textOffset;
                        strcpy(EMSheapPtr(textOffset), (const char *) value->value);
                    }
                        break;
                    case EMS_TYPE_UNDEFINED:
                        bufInt64[EMSdataData(idx)] = 0xdeadbeef;
                        break;
                    default:
                        fprintf(stderr, "EMSwriteUsingTags: Unknown arg type\n");
                        return false;
                }

                oldTag.byte = newTag.byte;
                if (finalFE != EMS_TAG_ANY) {
                    newTag.tags.fe = finalFE;
                    newTag.tags.rw = 0;
                }
                newTag.tags.type = value->type;
                if (finalFE != EMS_TAG_ANY && bufTags[EMSdataTag(idx)].byte != oldTag.byte) {
                    fprintf(stderr, "EMSwriteUsingTags: Lost tag lock while BUSY\n");
                    return false;
                }

                //  Set the tags for the data (and map, if used) back to full to finish the operation
                bufTags[EMSdataTag(idx)].byte = newTag.byte;
                if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
                return true;
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
bool EMSwriteXF(int mmapID, EMSvalueType *key, EMSvalueType *value) {
    return EMSwriteUsingTags(mmapID, key, value, EMS_TAG_ANY, EMS_TAG_FULL);
}

//==================================================================
//  WriteXE
bool EMSwriteXE(int mmapID, EMSvalueType *key, EMSvalueType *value) {
    return EMSwriteUsingTags(mmapID, key, value, EMS_TAG_ANY, EMS_TAG_EMPTY);
}

//==================================================================
//  WriteEF
bool EMSwriteEF(int mmapID, EMSvalueType *key, EMSvalueType *value) {
    return EMSwriteUsingTags(mmapID, key, value, EMS_TAG_EMPTY, EMS_TAG_FULL);
}

//==================================================================
//  Write
bool EMSwrite(int mmapID, EMSvalueType *key, EMSvalueType *value) {
    return EMSwriteUsingTags(mmapID, key, value, EMS_TAG_ANY, EMS_TAG_ANY);
}


//==================================================================
//  Set only the Full/Empty tag  from JavaScript 
//  without inspecting or modifying the data.
bool EMSsetTag(int mmapID, EMSvalueType *key, bool is_full) {
    void *emsBuf = emsBufs[mmapID];
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t tag;

    int64_t idx = EMSkey2index(emsBuf, key, EMSisMapped);
    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        return false;
    }

    tag.byte = bufTags[EMSdataTag(idx)].byte;
    if (is_full) {
        tag.tags.fe = EMS_TAG_FULL;
    } else {
        tag.tags.fe = EMS_TAG_EMPTY;
    }
    bufTags[EMSdataTag(idx)].byte = tag.byte;
    return true;
}


//==================================================================
//  Release all the resources associated with an EMS array
bool EMSdestroy(int mmapID, bool do_unlink) {
    void *emsBuf = emsBufs[mmapID];
    if(munmap(emsBuf, emsBufLengths[mmapID]) != 0) {
        fprintf(stderr, "EMSdestroy: Unable to unmap memory\n");
        return false;
    }

    if (do_unlink) {
        if (unlink(emsBufFilenames[mmapID]) != 0) {
            fprintf(stderr, "EMSdestroy: Unable to unlink file\n");
            return false;
        }
    }

    emsBufFilenames[mmapID][0] = 0;
    emsBufLengths[mmapID] = 0;
    emsBufs[mmapID] = NULL;
    return true;
}



//==================================================================
//  Return the key of a mapped object given the EMS index
bool EMSindex2key(int mmapID, int64_t idx, EMSvalueType *key) {
    void *emsBuf = emsBufs[mmapID];
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    char *bufChar = (char *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;

    if(!EMSisMapped) {
        fprintf(stderr, "EMSindex2key: Unmapping an index but Array is not mapped\n");
        return false;
    }

    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMSindex2key: index out of bounds\n");
        return false;
    }

    key->type = bufTags[EMSmapTag(idx)].tags.type;
    switch (key->type) {
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER: {
            key->value = (void *) bufInt64[EMSmapData(idx)];
            return true;
        }
        case EMS_TYPE_FLOAT: {
            EMSulong_double alias;
            alias.d = bufDouble[EMSmapData(idx)];
            key->value = (void *) alias.u64;
            return true;
        }
        case EMS_TYPE_BUFFER:
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            key->value = (void *)(EMSheapPtr(bufInt64[EMSmapData(idx)]));
            return true;
        }
        case EMS_TYPE_UNDEFINED: {
            key->value = NULL;
            return true;
        }
        default:
            fprintf(stderr, "EMSindex2key unknown type\n");
            return false;
    }
}


//==================================================================
//  Synchronize the EMS memory to persistent storage
//
bool EMSsync(int mmapID) {
    // resultIdx = msync((void*) emsBuf, pgsize, flags);
    printf("EMSsync() was called but stubbed out\n");
    return false;
}


//==================================================================
//  EMS Entry Point:   Allocate and initialize the EMS domain memory
//
int EMSinitialize(int64_t nElements,     // 0
                  size_t heapSize,      // 1
                  bool useMap,           // 2
                  const char *filename,  // 3
                  bool persist,          // 4
                  bool useExisting,      // 5
                  bool doDataFill,       // 6
                  bool fillIsJSON,       // 7
                  EMSvalueType *fillValue,// 8
                  bool doSetFEtags,      // 9
                  bool setFEtagsFull,    // 10
                  int EMSmyIDarg,        // 11
                  bool pinThreads,       // 12
                  int32_t nThreads,      // 13
                  int32_t pctMLock ) {   // 14
    int fd;
    EMSmyID = EMSmyIDarg;

    //  Node 0 is first and always has mutual exclusion during initialization
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
        while (stat(filename, &statbuf) != 0) nanosleep(&sleep_time, NULL); // TODO: timeout?
    }

    if (persist)
        fd = open(filename, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    else
        fd = shm_open(filename, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);

    if (fd < 0) {
        perror("Error opening shared memory file -- Possibly need to be root?");
        return -1;
    }

    size_t nMemBlocks = (heapSize / EMS_MEM_BLOCKSZ) + 1;
    size_t nMemBlocksPow2 = emsNextPow2((int64_t) nMemBlocks);
    int32_t nMemLevels = __builtin_ctzl(nMemBlocksPow2);
    size_t bottomOfMalloc;
    size_t filesize;

    size_t bottomOfMap = (size_t)EMSdataTagWord(nElements) + (size_t)EMSwordSize;  // Map begins 1 word AFTER the last tag word of data
    if (useMap) {
        bottomOfMalloc = bottomOfMap + bottomOfMap;
    } else {
        bottomOfMalloc = bottomOfMap;
    }
    size_t bottomOfHeap = bottomOfMalloc + sizeof(struct emsMem) + (nMemBlocksPow2 * 2 - 2);

    if (nElements <= 0) {
        filesize = EMS_CB_LOCKS + nThreads;   // EMS Control Block
        filesize *= sizeof(int);
    } else {
        filesize = bottomOfHeap + (nMemBlocksPow2 * EMS_MEM_BLOCKSZ);
    }
    if (ftruncate(fd, (off_t) filesize) != 0) {
        if (errno != EINVAL) {
            fprintf(stderr, "EMSinitialize: Error during initialization, unable to set memory size to %" PRIu64 " bytes\n",
                    (uint64_t) filesize);
            return -1;
        }
    }

    char *emsBuf = (char *) mmap(0, filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t) 0);
    if (emsBuf == MAP_FAILED) {
        fprintf(stderr, "EMSinitialize: Unable to map domain memory\n");
        return -1;
    }
    close(fd);

    if (nElements <= 0) pctMLock = 100;   // lock RAM if master control block
    if (mlock((void *) emsBuf, (size_t) (filesize * (pctMLock / 100))) != 0) {
        fprintf(stderr, "EMSinitialize NOTICE: EMS thread %d was not able to lock EMS memory to RAM for %s\n", EMSmyID, filename);
    } else {
        // success
    }

    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    char *bufChar = emsBuf;
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
    size_t fillStrLen = 0;
    if (doDataFill  &&  (fillValue->type == EMS_TYPE_JSON || fillValue->type == EMS_TYPE_STRING || fillValue->type == EMS_TYPE_BUFFER)) {
        fillStrLen = fillValue->length;
    }
    if (endIter > nElements) endIter = nElements;
    for (int64_t idx = startIter; idx < endIter; idx++) {
        tag.tags.rw = 0;
        if (doDataFill) {
            tag.tags.type = fillValue->type;
            switch (tag.tags.type) {
                case EMS_TYPE_BOOLEAN:
                case EMS_TYPE_INTEGER:
                    bufInt64[EMSdataData(idx)] = (int64_t) fillValue->value;
                    break;
                case EMS_TYPE_FLOAT: {
                    EMSulong_double alias;
                    alias.u64 = (uint64_t) fillValue->value;
                    bufDouble[EMSdataData(idx)] = alias.d;
                }
                    break;
                case EMS_TYPE_UNDEFINED:
                    bufInt64[EMSdataData(idx)] = 0xdeadbeef;
                    break;
                case EMS_TYPE_BUFFER: {
                    int64_t byteOffset;
                    EMS_ALLOC(byteOffset, fillStrLen, bufChar, // TODO: + 1 NULL padding
                              "EMSinitialize: out of memory to store buffer", false);
                    bufInt64[EMSdataData(idx)] = byteOffset;
                    memcpy(EMSheapPtr(byteOffset), (const char *) fillValue->value, fillStrLen);
                    // strcpy(EMSheapPtr(byteOffset), (const char *) fillValue->value);
                }
                case EMS_TYPE_JSON:
                case EMS_TYPE_STRING: {
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, fillStrLen + 1, bufChar,
                              "EMSinitialize: out of memory to store string", false);
                    bufInt64[EMSdataData(idx)] = textOffset;
                    strcpy(EMSheapPtr(textOffset), (const char *) fillValue->value);
                }
                    break;
                default:
                    fprintf(stderr, "EMSinitialize: fill type is unknown\n");
                    return -1;
            }
        } else {
            tag.tags.type = EMS_TYPE_UNDEFINED;
        }

        if (doSetFEtags) {
            if (setFEtagsFull) tag.tags.fe = EMS_TAG_FULL;
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

    int emsBufN = 0;
    while(emsBufN < EMS_MAX_N_BUFS  &&  emsBufs[emsBufN] != NULL)  emsBufN++;
    if(emsBufN < EMS_MAX_N_BUFS) {
        emsBufs[emsBufN] = emsBuf;
        emsBufLengths[emsBufN] = filesize;
        strncpy(emsBufFilenames[emsBufN], filename, MAX_FNAME_LEN);
    } else {
        fprintf(stderr, "EMSinitialize: ERROR - Unable to allocate a buffer ID/index\n");
        emsBufN = -1;
    }

    return emsBufN;
}

