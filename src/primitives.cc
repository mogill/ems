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
#include "ems.h"


//==================================================================
//  Push onto stack
int EMSpush(int mmapID, EMSvalueType *value) {  // TODO: Eventually promote return value to 64bit
    void *emsBuf = emsBufs[mmapID];
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t newTag;

    // Wait until the stack top is full, then mark it busy while updating the stack
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int32_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)];  // TODO BUG: Truncating the full 64b range
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
    if (idx == bufInt64[EMScbData(EMS_ARR_NELEM)] - 1) {
        fprintf(stderr, "EMSpush: Ran out of stack entries\n");
        return -1;
    }

    //  Wait until the target memory at the top of the stack is empty
    newTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], NULL, EMS_TAG_EMPTY, EMS_TAG_BUSY, EMS_TAG_ANY);
    newTag.tags.rw = 0;
    newTag.tags.type = value->type;
    newTag.tags.fe = EMS_TAG_FULL;

    //  Write the value onto the stack
    switch (newTag.tags.type) {
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER:
        case EMS_TYPE_FLOAT:
            bufInt64[EMSdataData(idx)] = (int64_t) value->value;
            break;
        case EMS_TYPE_BUFFER: {
            int64_t byteOffset;
            EMS_ALLOC(byteOffset, strlen((const char *) value->value), bufChar, "EMSpush: out of memory to store buffer\n", -1); // + 1 NULL padding
            bufInt64[EMSdataData(idx)] = byteOffset;
            memcpy(EMSheapPtr(byteOffset), (const char *) value->value, value->length);
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            int64_t textOffset;
            EMS_ALLOC(textOffset, strlen((const char *) value->value) + 1, bufChar, "EMSpush: out of memory to store string\n", -1);
            bufInt64[EMSdataData(idx)] = textOffset;
            strcpy(EMSheapPtr(textOffset), (const char *) value->value);
        }
            break;
        case EMS_TYPE_UNDEFINED:
            bufInt64[EMSdataData(idx)] = 0xdeadbeef;
            break;
        default:
            fprintf(stderr, "EMSpush: Unknown value type\n");
            return -1;
    }

    //  Mark the data on the stack as FULL
    bufTags[EMSdataTag(idx)].byte = newTag.byte;

    //  Push is complete, Mark the stack pointer as full
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;

    return idx;
}


//==================================================================
//  Pop data from stack
//
bool EMSpop(int mmapID, EMSvalueType *returnValue) {
    void *emsBuf = emsBufs[mmapID];
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t dataTag;

    //  Wait until the stack pointer is full and mark it empty while pop is performed
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]--;
    int64_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)];
    if (idx < 0) {
        //  Stack is empty, return undefined
        bufInt64[EMScbData(EMS_ARR_STACKTOP)] = 0;
        bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
        returnValue->type = EMS_TYPE_UNDEFINED;
        returnValue->value = (void *) 0xf00dd00f;
        return true;
    }
    //  Wait until the data pointed to by the stack pointer is full, then mark it
    //  busy while it is copied, and set it to EMPTY when finished
    dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    returnValue->type = dataTag.tags.type;
    switch (dataTag.tags.type) {
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER:
        case EMS_TYPE_FLOAT: {
            returnValue->value = (void *) bufInt64[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            return true;
        }
        case EMS_TYPE_BUFFER: {
            size_t memStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));  // TODO: Use size of allocation, not strlen
            returnValue->value = malloc(memStrLen);  // + 1 NULL padding // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMSpop: Unable to allocate space to return stack top string\n");
                return false;
            }
            memcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]), memStrLen);
            EMS_FREE(bufInt64[EMSdataData(idx)]);
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            return true;
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            size_t memStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));  // TODO: Use size of allocation, not strlen
            returnValue->value = malloc(memStrLen + 1);  // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMSpop: Unable to allocate space to return stack top string\n");
                return false;
            }
            strcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]));
            EMS_FREE(bufInt64[EMSdataData(idx)]);
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            return true;
        }
        case EMS_TYPE_UNDEFINED: {
            bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_EMPTY;
            bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
            returnValue->value = (void *) 0xdeadbeef;
            return true;
        }
        default:
            fprintf(stderr, "EMSpop: ERROR - unknown top of stack data type\n");
            return false;
    }
}


//==================================================================
//  Enqueue data
//  Heap top and bottom are monotonically increasing, but the index
//  returned is a circular buffer.
int EMSenqueue(int mmapID, EMSvalueType *value) {  // TODO: Eventually promote return value to 64bit
    void *emsBuf = emsBufs[mmapID];
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    char *bufChar = (char *) emsBuf;

    //  Wait until the heap top is full, and mark it busy while data is enqueued
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int32_t idx = bufInt64[EMScbData(EMS_ARR_STACKTOP)] % bufInt64[EMScbData(EMS_ARR_NELEM)];  // TODO: BUG  This could be truncated
    bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
    if (bufInt64[EMScbData(EMS_ARR_STACKTOP)] - bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] >
        bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMSenqueue: Ran out of stack entries\n");
        return -1;
    }

    //  Wait for data pointed to by heap top to be empty, then set to Full while it is filled
    bufTags[EMSdataTag(idx)].tags.rw = 0;
    bufTags[EMSdataTag(idx)].tags.type = value->type;
    switch (bufTags[EMSdataTag(idx)].tags.type) {
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER:
        case EMS_TYPE_FLOAT:
            bufInt64[EMSdataData(idx)] = (int64_t) value->value;
            break;
        case EMS_TYPE_BUFFER: {
            int64_t byteOffset;
            // TODO: size_t byteLength = value->length or something...
            size_t byteLength = strlen((const char *) value->value);
            EMS_ALLOC(byteOffset, byteLength, bufChar, "EMSenqueue: out of memory to store buffer\n", -1); // +1 NULL padding
            bufInt64[EMSdataData(idx)] = byteOffset;
            memcpy(EMSheapPtr(byteOffset), (const char *) value->value, byteLength);
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            int64_t textOffset;
            EMS_ALLOC(textOffset, strlen((const char *) value->value) + 1, bufChar, "EMSenqueue: out of memory to store string\n", -1);
            bufInt64[EMSdataData(idx)] = textOffset;
            strcpy(EMSheapPtr(textOffset), (const char *) value->value);
        }
            break;
        case EMS_TYPE_UNDEFINED:
            bufInt64[EMSdataData(idx)] = 0xdeadbeef;
            break;
        default:
            fprintf(stderr, "EMSenqueue: Unknown value type\n");
            return -1;
    }

    //  Set the tag on the data to FULL
    bufTags[EMSdataTag(idx)].tags.fe = EMS_TAG_FULL;

    //  Enqueue is complete, set the tag on the heap to to FULL
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_TAG_FULL;
    return idx;
}


//==================================================================
//  Dequeue
bool EMSdequeue(int mmapID, EMSvalueType *returnValue) {
    void *emsBuf = emsBufs[mmapID];
    int64_t *bufInt64 = (int64_t *) emsBuf;
    EMStag_t *bufTags = (EMStag_t *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t dataTag;

    //  Wait for bottom of heap pointer to be full, and mark it busy while data is dequeued
    EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    int64_t idx = bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] % bufInt64[EMScbData(EMS_ARR_NELEM)];
    //  If Queue is empty, return undefined
    if (bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] >= bufInt64[EMScbData(EMS_ARR_STACKTOP)]) {
        bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] = bufInt64[EMScbData(EMS_ARR_STACKTOP)];
        bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
        returnValue->type = EMS_TYPE_UNDEFINED;
        returnValue->value = (void *) 0xf00dd00f;
        return true;
    }

    bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)]++;
    //  Wait for the data pointed to by the bottom of the heap to be full,
    //  then mark busy while copying it, and finally set it to empty when done
    dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], NULL, EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
    dataTag.tags.fe = EMS_TAG_EMPTY;
    returnValue->type = dataTag.tags.type;
    switch (dataTag.tags.type) {
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER:
        case EMS_TYPE_FLOAT: {
            returnValue->value = (void *) bufInt64[EMSdataData(idx)];
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            return true;
        }
        case EMS_TYPE_BUFFER: {
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            size_t memStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));  // TODO: Use size of allocation, not strlen
            // returnValue->value = malloc(memStrLen + 1); +1 NULL padding  // freed in NodeJSfaa
            returnValue->value = malloc(memStrLen);  // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMSdequeue: Unable to allocate space to return queue head string\n");
                return false;
            }
            memcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]), memStrLen);
            EMS_FREE(bufInt64[EMSdataData(idx)]);
            return true;
        }
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING: {
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            size_t memStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));  // TODO: Use size of allocation, not strlen
            returnValue->value = malloc(memStrLen + 1);  // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMSdequeue: Unable to allocate space to return queue head string\n");
                return false;
            }
            strcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]));
            EMS_FREE(bufInt64[EMSdataData(idx)]);
            return true;
        }
        case EMS_TYPE_UNDEFINED: {
            bufTags[EMSdataTag(idx)].byte = dataTag.byte;
            bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_TAG_FULL;
            returnValue->value = (void *) 0xdeadbeef;
            return true;
        }
        default:
            fprintf(stderr, "EMSdequeue: ERROR - unknown type at head of queue\n");
            return false;
    }
}
