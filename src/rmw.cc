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
//  Fetch and Add Atomic Memory Operation
//  Returns a+b where a is data in EMS memory and b is an argument
bool EMSfaa(int mmapID, EMSvalueType *key, EMSvalueType *value, EMSvalueType *returnValue) {
    void *emsBuf = emsBufs[mmapID];
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    int64_t idx = EMSwriteIndexMap(mmapID, key);
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    volatile double *bufDouble = (double *) emsBuf;
    char *bufChar = (char *) emsBuf;
    EMStag_t oldTag;

    if (idx < 0 || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMSfaa: index out of bounds\n");
        return false;
    }

    volatile EMStag_t *maptag;
    if (EMSisMapped) { maptag = &bufTags[EMSmapTag(idx)]; }
    else             { maptag = NULL; }
    // Wait until the data is FULL, mark it busy while FAA is performed
    oldTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], maptag,
                                     EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);

    oldTag.tags.fe = EMS_TAG_FULL;  // When written back, mark FULL
    switch (oldTag.tags.type) {
        case EMS_TYPE_BOOLEAN: {    //  Bool + _______
            bool retBool = bufInt64[EMSdataData(idx)];  // Read original value in memory
            returnValue->value = (void *) retBool;
            returnValue->type  = EMS_TYPE_BOOLEAN;
            switch (value->type) {
                case EMS_TYPE_INTEGER:   //  Bool + Int
                    bufInt64[EMSdataData(idx)] += (int64_t) value->value;
                    oldTag.tags.type = EMS_TYPE_INTEGER;
                    break;
                case EMS_TYPE_FLOAT: {    //  Bool + Float
                    ulong_double alias;
                    alias.u64 = (uint64_t) value->value;
                    bufDouble[EMSdataData(idx)] =
                            (double) bufInt64[EMSdataData(idx)] + alias.d;
                    oldTag.tags.type = EMS_TYPE_FLOAT;
                }
                    break;
                case EMS_TYPE_UNDEFINED: //  Bool + undefined
                    bufDouble[EMSdataData(idx)] = NAN;
                    oldTag.tags.type = EMS_TYPE_FLOAT;
                    break;
                case EMS_TYPE_BOOLEAN:   //  Bool + Bool
                    bufInt64[EMSdataData(idx)] += (int64_t) value->value;
                    oldTag.tags.type = EMS_TYPE_INTEGER;
                    break;
                case EMS_TYPE_STRING: {   //  Bool + string
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, value->length + 1 + 5, //  String length + Terminating null + 'false'
                              bufChar, "EMSfaa(bool+string): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%s",
                            bufInt64[EMSdataData(idx)] ? "true" : "false", (const char *) value->value);
                    bufInt64[EMSdataData(idx)] = textOffset;
                    oldTag.tags.type = EMS_TYPE_STRING;
                }
                    break;
                default:
                    fprintf(stderr, "EMSfaa: Data is BOOL, but FAA arg type is unknown\n");
                    return false;

            }
            //  Write the new type and set the tag to Full, then return the original value
            bufTags[EMSdataTag(idx)].byte = oldTag.byte;
            if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            return true;
        }  // End of:  Bool + ___

        case EMS_TYPE_INTEGER: {
            int32_t retInt = bufInt64[EMSdataData(idx)];  // Read original value in memory
            returnValue->type = EMS_TYPE_INTEGER;
            returnValue->value = (void *) (int64_t) retInt;
            switch (value->type) {
                case EMS_TYPE_INTEGER: {  // Int + int
                    int64_t memInt = bufInt64[EMSdataData(idx)] + (int64_t) value->value;
                    // TODO: Magic max int promotion to float
                    if (memInt >= (1 << 30)) {  // Possible integer overflow, convert to float
                        bufDouble[EMSdataData(idx)] = (double) memInt;
                        oldTag.tags.type = EMS_TYPE_FLOAT;
                    } else { //  Did not overflow to flow, still an integer
                        bufInt64[EMSdataData(idx)] = memInt;
                    }
                }
                    break;
                case EMS_TYPE_FLOAT: {    // Int + float
                    ulong_double alias;
                    alias.u64 = (uint64_t) value->value;
                    bufDouble[EMSdataData(idx)] =
                            (double) bufInt64[EMSdataData(idx)] + alias.d;
                    oldTag.tags.type = EMS_TYPE_FLOAT;
                }
                    break;
                case EMS_TYPE_UNDEFINED: // Int + undefined
                    bufDouble[EMSdataData(idx)] = NAN;
                    oldTag.tags.type = EMS_TYPE_FLOAT;
                    break;
                case EMS_TYPE_BOOLEAN:   // Int + bool
                    bufInt64[EMSdataData(idx)] += (int64_t) value->value;
                    break;
                case EMS_TYPE_STRING: {   // int + string
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, value->length + 1 + MAX_NUMBER2STR_LEN,
                              bufChar, "EMSfaa(int+string): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%lld%s",
                            (long long int) bufInt64[EMSdataData(idx)], (const char *) value->value);
                    bufInt64[EMSdataData(idx)] = textOffset;
                    oldTag.tags.type = EMS_TYPE_STRING;
                }
                    break;
                default:
                    fprintf(stderr, "EMSfaa: Data is INT, but FAA arg type is unknown\n");
                    return false;
            }
            //  Write the new type and set the tag to Full, then return the original value
            bufTags[EMSdataTag(idx)].byte = oldTag.byte;
            if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            return true;
        }  // End of: Integer + ____

        case EMS_TYPE_FLOAT: {
            double retDbl = bufDouble[EMSdataData(idx)];
            returnValue->type = EMS_TYPE_FLOAT;
            ulong_double alias;
            alias.d = retDbl;
            returnValue->value = (void *) alias.u64;

            switch (value->type) {
                case EMS_TYPE_INTEGER:   // Float + int
                    bufDouble[EMSdataData(idx)] += (double) ((int64_t) value->value);
                    break;
                case EMS_TYPE_FLOAT: {   // Float + float
                    ulong_double alias;
                    alias.u64 = (uint64_t) value->value;
                    bufDouble[EMSdataData(idx)] += alias.d;
                }
                    break;
                case EMS_TYPE_BOOLEAN:   // Float + boolean
                    bufDouble[EMSdataData(idx)] += (double) ((int64_t) value->value);
                    break;
                case EMS_TYPE_STRING: {   // Float + string
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, value->length + 1 + MAX_NUMBER2STR_LEN,
                              bufChar, "EMSfaa(float+string): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%lf%s", bufDouble[EMSdataData(idx)], (const char *) value->value);
                    bufInt64[EMSdataData(idx)] = textOffset;
                    oldTag.tags.type = EMS_TYPE_STRING;
                }
                    break;
                case EMS_TYPE_UNDEFINED: // Float + Undefined
                    bufDouble[EMSdataData(idx)] = NAN;
                    break;
                default:
                    fprintf(stderr, "EMSfaa: Data is FLOAT, but arg type unknown\n");
                    return false;
            }
            //  Write the new type and set the tag to Full, then return the original value
            bufTags[EMSdataTag(idx)].byte = oldTag.byte;
            if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            return true;
        } //  End of: float + _______

        case EMS_TYPE_STRING: {
            // size_t oldStrLen = (size_t) emsMem_size(EMS_MEM_MALLOCBOT(bufChar), bufInt64[EMSdataData(idx)]);
            size_t oldStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));
            returnValue->type = EMS_TYPE_STRING;
            returnValue->value = malloc(oldStrLen + 1);  // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMSfaa: Unable to malloc temporary old string\n");
                return false;
            }
            strcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]));
            int64_t textOffset;
            size_t len;
            switch (value->type) {
                case EMS_TYPE_INTEGER: // string + int
                    len = oldStrLen + 1 + MAX_NUMBER2STR_LEN;
                    EMS_ALLOC(textOffset, len, bufChar, "EMSfaa(string+int): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%lld",
                            EMSheapPtr(bufInt64[EMSdataData(idx)]),
                            (long long int) value->value);
                    break;
                case EMS_TYPE_FLOAT: {  // string + dbl
                    ulong_double alias;
                    alias.u64 = (uint64_t) value->value;
                    len = oldStrLen + 1 + MAX_NUMBER2STR_LEN;
                    EMS_ALLOC(textOffset, len, bufChar, "EMSfaa(string+dbl): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%lf",
                            EMSheapPtr(bufInt64[EMSdataData(idx)]), alias.d);
                }
                    break;
                case EMS_TYPE_STRING: { // string + string
                    len = oldStrLen + 1 + value->length;
                    EMS_ALLOC(textOffset, len, bufChar, "EMSfaa(string+string): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%s",
                            EMSheapPtr(bufInt64[EMSdataData(idx)]), (const char *) value->value);
                }
                    break;
                case EMS_TYPE_BOOLEAN:   // string + bool
                    len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + 5;  // 5==strlen("false")
                    EMS_ALLOC(textOffset, len, bufChar, "EMSfaa(string+bool): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%s",
                            EMSheapPtr(bufInt64[EMSdataData(idx)]), (bool) value->value ? "true" : "false");
                    break;
                case EMS_TYPE_UNDEFINED: // string + undefined
                    len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + 9; // 9 == strlen("undefined");
                    EMS_ALLOC(textOffset, len, bufChar, "EMSfaa(string+undefined): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "%s%s",
                            EMSheapPtr(bufInt64[EMSdataData(idx)]), "undefined");
                    break;
                default:
                    fprintf(stderr, "EMSfaa(string+?): Unknown data type\n");
                    return false;
            }
            EMS_FREE(bufInt64[EMSdataData(idx)]);
            bufInt64[EMSdataData(idx)] = textOffset;
            oldTag.tags.type = EMS_TYPE_STRING;
            //  Write the new type and set the tag to Full, then return the original value
            bufTags[EMSdataTag(idx)].byte = oldTag.byte;
            if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            // return value was set at the top of this block
            return true;
        }  // End of: String + __________

         case EMS_TYPE_UNDEFINED: {
             returnValue->type = EMS_TYPE_UNDEFINED;
             returnValue->value = (void *) 0xf00dd00f;
             switch (value->type) {  // Undefined + Int, dloat, bool, or undef
                case EMS_TYPE_INTEGER:
                case EMS_TYPE_FLOAT:
                case EMS_TYPE_BOOLEAN:
                case EMS_TYPE_UNDEFINED:
                    bufDouble[EMSdataData(idx)] = NAN;
                    oldTag.tags.type = EMS_TYPE_FLOAT;
                    break;
                case EMS_TYPE_STRING: { // Undefined + string
                    int64_t textOffset;
                    EMS_ALLOC(textOffset, value->length + 1 + 3, //  3 = strlen("NaN");
                              bufChar, "EMSfaa(undef+String): out of memory to store string\n", false);
                    sprintf(EMSheapPtr(textOffset), "NaN%s", (const char *) value->value);
                    bufInt64[EMSdataData(idx)] = textOffset;
                    oldTag.tags.type = EMS_TYPE_UNDEFINED;
                }
                    break;
                default:
                    fprintf(stderr, "EMSfaa(Undefined+___: Unknown stored data type\n");
                    return false;
            }
            //  Write the new type and set the tag to Full, then return the original value
            bufTags[EMSdataTag(idx)].byte = oldTag.byte;
            if (EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;
            return true;
        }
        default:
            fprintf(stderr, "EMSfaa(?+___: Unknown stored data type\n");
            return false;
    }
    // fprintf(stderr, "EMSfaa: Unknown everything -- failing for unknown reasons\n");
    return false;
}


//==================================================================
//  Atomic Compare and Swap
bool EMScas(int mmapID, EMSvalueType *key,
            EMSvalueType *oldValue, EMSvalueType *newValue,
            EMSvalueType *returnValue) {
    void *emsBuf = emsBufs[mmapID];
    volatile int64_t *bufInt64 = (int64_t *) emsBuf;
    int64_t idx = EMSkey2index(emsBuf, key, EMSisMapped);
    char * bufChar = (char *) emsBuf;
    volatile EMStag_t *bufTags = (EMStag_t *) emsBuf;
    EMStag_t newTag;
    int64_t textOffset;
    int swapped = false;

    if ((!EMSisMapped  &&  idx < 0) || idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
        fprintf(stderr, "EMScas: index out of bounds\n");
        return false;
    }

    size_t memStrLen;
    unsigned char memType;
retry_on_undefined:
    if(EMSisMapped  &&  idx < 0) {
        memType = EMS_TYPE_UNDEFINED;
    } else {
        //  Wait for the memory to be Full, then mark it Busy while CAS works
        volatile EMStag_t *maptag;
        if (EMSisMapped) { maptag = &bufTags[EMSmapTag(idx)]; }
        else             { maptag = NULL; }
        // Wait until the data is FULL, mark it busy while FAA is performed
        EMStransitionFEtag(&bufTags[EMSdataTag(idx)], maptag,
                           EMS_TAG_FULL, EMS_TAG_BUSY, EMS_TAG_ANY);
        memType = bufTags[EMSdataTag(idx)].tags.type;
    }

    //  Read the value in memory
    returnValue->type = memType;
    switch (memType) {
        case EMS_TYPE_UNDEFINED:
            returnValue->value = (void *) 0xf00dcafe;
            break;
        case EMS_TYPE_BOOLEAN:
        case EMS_TYPE_INTEGER:
        case EMS_TYPE_FLOAT:
            returnValue->value =  (void *) bufInt64[EMSdataData(idx)];
            break;
        case EMS_TYPE_JSON:
        case EMS_TYPE_STRING:
            memStrLen = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)]));
            returnValue->value = malloc(memStrLen + 1);  // freed in NodeJSfaa
            if(returnValue->value == NULL) {
                fprintf(stderr, "EMScas: Unable to allocate space to return old string\n");
                return false;
            }
            strcpy((char *) returnValue->value, EMSheapPtr(bufInt64[EMSdataData(idx)]));
            break;
        default:
            fprintf(stderr, "EMScas: memType not recognized\n");
            return false;
    }

    //  Compare the value in memory the the "old" CAS value
    if (oldValue->type == memType) {
        //  Allocate on Write: If this memory was undefined (ie: unallocated),
        //  allocate the index map, store the undefined, and start over again.
        if(EMSisMapped  &&  idx < 0) {
            idx = EMSwriteIndexMap(mmapID, key);
            if (idx < 0) {
                fprintf(stderr, "EMScas: Not able to allocate map on CAS of undefined data\n");
                return false;
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
            case EMS_TYPE_INTEGER:
            case EMS_TYPE_FLOAT:
                if (returnValue->value == oldValue->value)
                    swapped = true;
                break;
            case EMS_TYPE_JSON:
            case EMS_TYPE_STRING:
                if (strcmp((const char *) returnValue->value, (const char *) oldValue->value) == 0) {
                    swapped = true;
                }
                break;
            default:
                fprintf(stderr, "EMScas: oldTag not recognized");
                return false;
        }
    }

    //  If memory==old then write the new value
    newTag.tags.fe = EMS_TAG_FULL;
    newTag.tags.rw = 0;
    newTag.tags.type = memType;
    if (swapped) {
        if (memType == EMS_TYPE_STRING  ||  memType == EMS_TYPE_JSON)
            EMS_FREE((size_t) bufInt64[EMSdataData(idx)]);
        newTag.tags.type = newValue->type;
        switch (newValue->type) {
            case EMS_TYPE_UNDEFINED:
                bufInt64[EMSdataData(idx)] = 0xbeeff00d;
                break;
            case EMS_TYPE_BOOLEAN:
            case EMS_TYPE_INTEGER:
            case EMS_TYPE_FLOAT:
                bufInt64[EMSdataData(idx)] = (int64_t) newValue->value;
                break;
            case EMS_TYPE_JSON:
            case EMS_TYPE_STRING:
                EMS_ALLOC(textOffset, newValue->length + 1,
                          bufChar, "EMScas(string): out of memory to store string\n", false);
                strcpy(EMSheapPtr(textOffset), (const char *) newValue->value);
                bufInt64[EMSdataData(idx)] = textOffset;
                break;
            default:
                fprintf(stderr, "EMScas(): Unrecognized new type\n");
                return false;
        }
    }

    //  Set the tag back to Full and return the original value
    bufTags[EMSdataTag(idx)].byte = newTag.byte;
    //  If there is a map, set the map's tag back to full
    if (EMSisMapped)
        bufTags[EMSmapTag(idx)].tags.fe = EMS_TAG_FULL;

    return true;
}
