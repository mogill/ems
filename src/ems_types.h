/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 1.5.0   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2017, Jace A Mogill.  All rights reserved.                   |
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
#ifndef EMSPROJ_EMS_TYPES_H
#define EMSPROJ_EMS_TYPES_H
// Bitfields of a Tag Byte
#define EMS_TYPE_NBITS_FE    2
#define EMS_TYPE_NBITS_TYPE  3
#define EMS_TYPE_NBITS_RW    3
#define EMS_RW_NREADERS_MAX  ((1 << EMS_TYPE_NBITS_RW) - 1)
typedef union {
    struct {
        unsigned char fe   : EMS_TYPE_NBITS_FE;
        unsigned char type : EMS_TYPE_NBITS_TYPE;
        unsigned char rw   : EMS_TYPE_NBITS_RW;
    } tags;
    unsigned char byte;
} EMStag_t;

#define EMS_VALUE_TYPE_INITIALIZER {.length=0, .value=NULL, .type=EMS_TYPE_INVALID}


// Type-punning is now a warning in GCC, but this syntax is still okay
typedef union {
    double d;
    uint64_t u64;
} EMSulong_double;


// Internal EMS representation of a JSON value
typedef struct {
    size_t length;  // Defined only for JSON and strings
    void *value;
    unsigned char type;
} EMSvalueType;


#endif
//EMSPROJ_EMS_TYPES_H
