/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.8   |
 |  Synthetic Semantics       http://www.synsem.com/       mogill@synsem.com   |
 +-----------------------------------------------------------------------------+
 |  Copyright (c) 2011-2014, Synthetic Semantics LLC.  All rights reserved.    |
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
#include <node.h>
#include <v8.h>
#include <node_buffer.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#if !defined _GNU_SOURCE
#  define _GNU_SOURCE
#endif
#include <sched.h>
#include "ems_alloc.h"

static v8::Persistent<v8::String> readRW_symbol;
static v8::Persistent<v8::String> releaseRW_symbol;
static v8::Persistent<v8::String> readFE_symbol;
static v8::Persistent<v8::String> readFF_symbol;
static v8::Persistent<v8::String> setTag_symbol;
static v8::Persistent<v8::String> writeEF_symbol;
static v8::Persistent<v8::String> writeXF_symbol;
static v8::Persistent<v8::String> writeXE_symbol;
static v8::Persistent<v8::String> criticalEnter_symbol;
static v8::Persistent<v8::String> criticalExit_symbol;
static v8::Persistent<v8::String> barrier_symbol;
static v8::Persistent<v8::String> single_symbol;
static v8::Persistent<v8::String> faa_symbol;
static v8::Persistent<v8::String> CAS_symbol;
static v8::Persistent<v8::String> read_symbol;
static v8::Persistent<v8::String> write_symbol;
static v8::Persistent<v8::String> push_symbol;
static v8::Persistent<v8::String> pop_symbol;
static v8::Persistent<v8::String> enqueue_symbol;
static v8::Persistent<v8::String> dequeue_symbol;
static v8::Persistent<v8::String> loopInit_symbol;
static v8::Persistent<v8::String> loopChunk_symbol;
static v8::Persistent<v8::String> length_symbol;
static v8::Persistent<v8::String> sync_symbol;
static v8::Persistent<v8::String> buffer_symbol;

//==================================================================
// EMS Full/Empty Tag States
//
#define EMS_ANY       ((char)4)  // Never stored, used for matching
#define EMS_RW_LOCK   ((char)3)
#define EMS_BUSY      ((char)2)
#define EMS_EMPTY     ((char)1)
#define EMS_FULL      ((char)0)


//==================================================================
// EMS Data types
//
#define EMS_UNALLOCATED  0
#define EMS_BOOLEAN      1
#define EMS_STRING       2
#define EMS_FLOAT        3
#define EMS_INTEGER      4
#define EMS_UNDEFINED    5
#define EMS_JSON         6  // Catch-all for JSON arrays and Objects



//==================================================================
// Control Block layout stored at the head of each EMS array
//
//        Name          Offset      Description of contents
//---------------------------------------------------------------------------------------
#define NWORDS_PER_CACHELINE 16
#define EMS_ARR_NELEM      (0 * NWORDS_PER_CACHELINE)   // Maximum number of elements in the EMS array
#define EMS_ARR_HEAPSZ     (1 * NWORDS_PER_CACHELINE)   // # bytes of storage for array data: strings, JSON, maps, etc.
#define EMS_ARR_Q_BOTTOM   (2 * NWORDS_PER_CACHELINE)   // Current index of the queue bottom
#define EMS_ARR_STACKTOP   (3 * NWORDS_PER_CACHELINE)   // Current index of the top of the stack/queue
#define EMS_ARR_MAPBOT     (4 * NWORDS_PER_CACHELINE)   // Index of the base of the index map
#define EMS_ARR_MALLOCBOT  (5 * NWORDS_PER_CACHELINE)   // Index of the base of the heap -- malloc structs start here
#define EMS_ARR_HEAPBOT    (6 * NWORDS_PER_CACHELINE)   // Index of the base of data on the heap -- strings start here
#define EMS_ARR_MEM_MUTEX  (7 * NWORDS_PER_CACHELINE)   // Mutex lock for thememory allocator of this EMS region's 
#define EMS_ARR_FILESZ     (8 * NWORDS_PER_CACHELINE)   // Total size in bytes of the EMS region
// Tag data may follow data by as much as 8 words, so 
// A gap of at least 8 words is required to leave space for 
// the tags associated with header data
#define EMS_ARR_CB_SIZE   (16 * NWORDS_PER_CACHELINE)   // Index of the first EMS array element


//==================================================================
//  Maximum number of slots to check due to conflicts
#define  MAX_OPEN_HASH_STEPS 200


//==================================================================
// EMS Control Block -- Global State for EMS
//
#define EMS_CB_NTHREADS     0     // Number of threads
#define EMS_CB_NBAR0        1     // Number of threads at Barrier 0
#define EMS_CB_NBAR1        2     // Number of threads at Barrier 1
#define EMS_CB_BARPHASE     3     // Current Barrier Phase (0 or 1)
#define EMS_CB_CRITICAL     4     // Mutex for critical regions
#define EMS_CB_SINGLE       5     // Number of threads passed through an execute-once region
#define EMS_LOOP_IDX        6     // Index of next iteration in a Parallel loop to schedule
#define EMS_LOOP_START      7     // Index of first iteration in a parallel loop
#define EMS_LOOP_END        8     // Index of last iteration in a parallel loop
#define EMS_LOOP_CHUNKSZ    9     // Current Number of iterations per thread
#define EMS_LOOP_MINCHUNK  10     // Smallest number of iterations per thread
#define EMS_LOOP_SCHED     11     // Parallel loop scheduling method:
#define    EMS_SCHED_GUIDED  1200
#define    EMS_SCHED_DYNAMIC 1201
#define    EMS_SCHED_STATIC  1202
#define EMS_CB_LOCKS       12     // First index of an array of locks, one lock per thread



//==================================================================
//  Yield the processor and sleep (using exponential decay) without 
//  using resources/
//  Used within spin-loops to reduce hot-spotting
//
#define RESET_NAP_TIME  int EMScurrentNapTime = 100
#define MAX_NAP_TIME  1000000
#define NANOSLEEP    {				\
    struct timespec     sleep_time;		\
    sleep_time.tv_sec  = 0;			\
    sleep_time.tv_nsec = EMScurrentNapTime;	\
    nanosleep(&sleep_time, NULL);		\
    EMScurrentNapTime *= 2;			\
    if(EMScurrentNapTime > MAX_NAP_TIME)	\
      EMScurrentNapTime = MAX_NAP_TIME;		\
 }




//==================================================================
//  Macros to translate from EMS Data Indexes and EMS Control Block 
//  indexes to offsets in the EMS shared memory
//
#define EMSwordSize          (sizeof(size_t))
#define EMSnWordsPerTagWord  (EMSwordSize-1)
#define EMSnWordsPerTag      1
#define EMSnWordsPerLine     EMSwordSize


//==================================================================
//  Layout of EMS memory
//     Tagged Memory
//         CB:     Control Block of array state
//         Data:   Scalar user data
//         Map:    Scalar index map data
//     Untagged Memory
//         Malloc: Storage for the free/used structures
//         Heap:   Open Heap storage
//
#define EMSappIdx2emsIdx(idx)         ((((idx) / EMSnWordsPerTagWord) * EMSnWordsPerLine) + ((idx) % EMSnWordsPerTagWord) )
#define EMSappIdx2LineIdx(idx)        ( ((idx) / EMSnWordsPerTagWord) * EMSnWordsPerLine)
#define EMSappIdx2TagWordIdx(idx)     ( EMSappIdx2LineIdx(idx) + EMSnWordsPerTagWord )
#define EMSappIdx2TagWordOffset(idx)  ( EMSappIdx2TagWordIdx(idx) * EMSwordSize )
#define EMSappTag2emsTag(idx)         ( EMSappIdx2TagWordOffset(idx) + ((idx) % EMSnWordsPerTagWord) )
#define EMScbData(idx)        EMSappIdx2emsIdx(idx)
#define EMScbTag(idx)         EMSappTag2emsTag(idx)
#define EMSdataData(idx)    ( EMSappIdx2emsIdx((idx) + EMS_ARR_CB_SIZE) )
#define EMSdataTag(idx)     ( EMSappTag2emsTag((idx) + EMS_ARR_CB_SIZE) )
#define EMSdataTagWord(idx) ( EMSappIdx2TagWordOffset((idx) + EMS_ARR_CB_SIZE) )
#define EMSmapData(idx)     ( EMSappIdx2emsIdx((idx) + EMS_ARR_CB_SIZE + bufInt64[EMScbData(EMS_ARR_NELEM)]) )
#define EMSmapTag(idx)      ( EMSappTag2emsTag((idx) + EMS_ARR_CB_SIZE + bufInt64[EMScbData(EMS_ARR_NELEM)]) )
#define EMSmapTagWord(idx)  ( EMSappIdx2TagWordOffset((idx) + bufInt64[EMScbData(EMS_ARR_MAPBOT)]) )
#define EMSheapPtr(idx)     ( &bufChar[ bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + (idx) ] )


//==================================================================
//  The EMS Tag structure
//
#define EMS_TYPE_NBITS_FE    2
#define EMS_TYPE_NBITS_TYPE  3
#define EMS_TYPE_NBITS_RW    3
union EMStag {
  struct {
    unsigned char  fe   : EMS_TYPE_NBITS_FE;
    unsigned char  type : EMS_TYPE_NBITS_TYPE;
    unsigned char  rw   : EMS_TYPE_NBITS_RW;
  } tags;
  unsigned char  byte;
} EMStag_dummy;

static int EMSmyID;   // EMS Thread ID

#define MAX_NUMBER2STR_LEN 40   // Maximum number of characters in a %d or %f format


//==================================================================
//  Determine the EMS type of a V8 argument
#define EMSv8toEMStype(arg, stringIsJSON)				\
  (									\
   arg->IsInt32()                     ? EMS_INTEGER :			\
   arg->IsNumber()                    ? EMS_FLOAT   :			\
   (arg->IsString() && !stringIsJSON) ? EMS_STRING  :			\
   (arg->IsString() &&  stringIsJSON) ? EMS_JSON  :			\
   arg->IsBoolean()                   ? EMS_BOOLEAN :			\
   arg->IsUndefined()                 ? EMS_UNDEFINED:			\
   arg->IsUint32()                    ? EMS_INTEGER : -1		\
								)



//==================================================================
//  Wrappers around memory allocator to ensure mutual exclusion
//  The buddy memory allocator is not thread safe, so this is necessary for now.
//  It can be used as the hook to wrap the non-threaded allocator with a
//  multiplexor of several independent regions.
//
//  Returns the byte offset in the EMS data space of the space allocated
//
size_t emsMutexMem_alloc( struct emsMem  *heap,   // Base of EMS malloc structs
			  size_t          len,    // Number of bytes to allocate
			  char           *mutex)  // Pointer to the mem allocator's mutex
{
  RESET_NAP_TIME;
  // Wait until we acquire the allocator's mutex
  while( !__sync_bool_compare_and_swap(mutex, EMS_EMPTY, EMS_FULL) ) {
    NANOSLEEP;
  }
  size_t retval = emsMem_alloc(heap, len );
  *mutex = EMS_EMPTY;    // Unlock the allocator's mutex
  return( retval );
}


void emsMutexMem_free( struct emsMem  *heap,  // Base of EMS malloc structs
		       size_t          addr,  // Offset of alloc'd block in EMS memory
		       char           *mutex) // Pointer to the mem allocator's mutex
{
  RESET_NAP_TIME;
  // Wait until we acquire the allocator's mutex
  while( !__sync_bool_compare_and_swap(mutex, EMS_EMPTY, EMS_FULL) ) {
    NANOSLEEP;
  }
  emsMem_free(heap, addr);
  *mutex = EMS_EMPTY;   // Unlock the allocator's mutex
}


#define EMS_ALLOC(addr, len, errmsg)					\
  addr = emsMutexMem_alloc( (struct emsMem *) &bufChar[ bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] ], \
			    len, (char*) &bufInt64[EMScbData(EMS_ARR_MEM_MUTEX)] ); \
  if(addr < 0)  return v8::ThrowException(node::ErrnoException(errno, "EMS", errmsg ))


#define EMS_FREE(addr) \
  emsMutexMem_free( (struct emsMem *) &bufChar[ bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] ], \
		    addr, (char*) &bufInt64[EMScbData(EMS_ARR_MEM_MUTEX)] )






//==================================================================
//  Callback for destruction of an EMS array
//
static void EMSarrFinalize(char *data, void*hint)
{
  //fprintf(stderr, "%d: EMSarrFinalize  data=%lx  hint=%lx %lld\n", EMSmyID, data, hint, hint);
  munmap(data, (size_t)hint);

#if 0
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
#endif
}






//==================================================================
//  Macro to declare and unwrap the EMS buffer, used to access the
//  EMSarray object metadata
//
#define EMS_DECL(args)							\
  node::Buffer *buffer = node::ObjectWrap::Unwrap<node::Buffer>(args.This()->GetHiddenValue(buffer_symbol)->ToObject()); \
  char*    emsBuf = static_cast<char*>(buffer->handle_->GetIndexedPropertiesExternalArrayData())



//==================================================================
//  Wait until the FE tag is a particular state, then transition it to the new state
//  Return new tag state
//
char EMStransitionFEtag( EMStag volatile *tag, char oldFE,  char newFE, char oldType )
{
  RESET_NAP_TIME;
  EMStag oldTag;           //  Desired tag value to start of the transition
  EMStag newTag;           //  Tag value at the end of the transition
  EMStag volatile memTag;  //  Tag value actually stored in memory
  memTag.byte = tag->byte;
  while(oldType == EMS_ANY  ||  memTag.tags.type == oldType ) {
    oldTag.byte    = memTag.byte;  // Copy current type and RW count information
    oldTag.tags.fe = oldFE;        // Set the desired start tag state
    newTag.byte    = memTag.byte;  // Copy current type and RW count information
    newTag.tags.fe = newFE;        // Set the final tag state

    //  Attempt to transition the state from old to new
    memTag.byte    =  __sync_val_compare_and_swap( &(tag->byte), oldTag.byte, newTag.byte);
    if(memTag.byte == oldTag.byte) {
      return(newTag.byte);
    } else {
      NANOSLEEP;
      memTag.byte  = tag->byte;  // Re-load tag in case was transitioned by another thread
    }
  }
  return(memTag.byte);
}


//==================================================================
//  Hash a string into an integer
//
int64_t EMShashString( v8::String::Utf8Value *key )
{
#define EMS_MAX_KEY_LENGTH 100
  char tmpStr[EMS_MAX_KEY_LENGTH];
  int  charN = 0;
  int64_t  hash = 0;
  strncpy(tmpStr, **key, (size_t) EMS_MAX_KEY_LENGTH);
  while(tmpStr[charN] != 0) {
    hash = tmpStr[charN] + (hash << 6) + (hash << 16) - hash;
    charN++;
  } 
  hash *= 1191613;  // Further scramble to prevent close strings from
                    // having close indexes
  return(labs(hash));
}



#define EMSisMapped (bufInt64[EMScbData(EMS_ARR_MAPBOT)]*(int64_t)EMSwordSize != bufInt64[EMScbData(EMS_ARR_MALLOCBOT)])

//==================================================================
//  Find the matching map key for this argument.
//  Returns the index of the element, or -1 for no match.
//  The stored Map key value is read when full and marked busy.
//  If the data does not match, it is marked full again, but if
//  there is a match the map key is kept busy until the operation
//  on the data is complete.
//
uint64_t EMSreadIndexMap(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int64_t   idx = 0;
  int64_t  *bufInt64    = (int64_t *) emsBuf;
  char     *bufChar     = (char *) emsBuf;
  EMStag   *bufTags     = (EMStag *) emsBuf;
  EMStag    mapTags;
  double   *bufDouble   = (double *) emsBuf;
  int       idxType     = EMSv8toEMStype(args[0], false);
  int64_t   boolArgVal  = false;
  int64_t   intArgVal   = -1;
  double    floatArgVal = 0.0;
  int nTries = 0;
  int matched = false;
  int notPresent = false;
  v8::String::Utf8Value argString(args[0]);

  if (args.Length() == 0) {
    fprintf(stderr, "EMS ERROR: EMSreadIndexMap has no arguments?\n");
    return(-1);
  }

  switch(idxType) {
  case EMS_BOOLEAN: 
    boolArgVal = args[0]->ToBoolean()->Value();
    idx = boolArgVal;
    break;
  case EMS_INTEGER:
    intArgVal = args[0]->ToInteger()->Value();
    idx = intArgVal;
    break;
  case EMS_FLOAT:
    floatArgVal = args[0]->ToNumber()->Value();
    idx = labs(*((int64_t*)&floatArgVal));
    break;
  case EMS_STRING: 
    idx = EMShashString(&argString);
    break;
  default:
    fprintf(stderr, "EMS ERROR: EMSreadIndexMap: Unknown mem type\n");
    return(-1);
  }

  if(EMSisMapped) {
    while(nTries < MAX_OPEN_HASH_STEPS  &&  !matched  && !notPresent) {
      idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
      // Wait until the map key is FULL, mark it busy while map lookup is performed
      mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
      if(mapTags.tags.type  ==  idxType) {
	switch(idxType) {
	case EMS_BOOLEAN:
	  if(boolArgVal == bufInt64[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_INTEGER:
	  if(intArgVal == bufInt64[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_FLOAT:
	  if(floatArgVal == bufDouble[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_STRING: {
	  int64_t  keyStrOffset = bufInt64[EMSmapData(idx)];
	  if(strcmp(*argString, EMSheapPtr(keyStrOffset)) == 0) matched = true;
	}
	  break;
	case EMS_UNDEFINED:
	  // Nothing hashed to this map index yet, so the key does not exist
	  notPresent = true;
	  break;
	default:
	  fprintf(stderr, "EMS ERROR: EMSreadIndexMap: Unknown mem type\n");
	  matched = true;
	}
      }
      if(mapTags.tags.type == EMS_UNDEFINED)  notPresent = true;
      if( !matched ) {
	//  No match, set this Map entry back to full and try again
	bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	nTries++;
	idx++;
      }
    }
    if( !matched ) { idx = -1; }
  } else {  // Wasn't mapped, do bounds check
    if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
      idx = -1;
    }
  }
  
  if(nTries >= MAX_OPEN_HASH_STEPS) {
    fprintf(stderr, "EMSreadIndexMap ran out of key mappings\n");
  }
  if(notPresent)  idx = -1;
  return(idx);
}




//==================================================================
//  Find the matching map key, if not present, find the
//  next available open address.
//  Reads map key when full and marks Busy to perform comparisons,
//  if it is not a match the data is marked full again, but if it does
//  match, the map key is left empty and this function
//  returns the index of an existing or available array element.
//
uint64_t EMSwriteIndexMap(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);

  int64_t   idx = 0;
  int64_t  *bufInt64    = (int64_t *) emsBuf;
  char     *bufChar     = (char *) emsBuf;
  EMStag   *bufTags     = (EMStag *) emsBuf;
  EMStag    mapTags;
  double   *bufDouble   = (double *) emsBuf;
  int       idxType     = EMSv8toEMStype(args[0], false);
  int64_t   boolArgVal  = false;
  int64_t   intArgVal   = -1;
  double    floatArgVal = 0.0;
  v8::String::Utf8Value argString(args[0]);

  if (args.Length() == 0) {
    fprintf(stderr, "EMS ERROR: EMSwriteIndexMap has no arguments?\n");
    return(-1);
  }

  switch(idxType) {
  case EMS_BOOLEAN: 
    boolArgVal = args[0]->ToBoolean()->Value();
    idx = boolArgVal;
    break;
  case EMS_INTEGER:
    intArgVal = args[0]->ToInteger()->Value();
    idx = intArgVal;
    break;
  case EMS_FLOAT:
    floatArgVal = args[0]->ToNumber()->Value();
    idx = *((int64_t*)&floatArgVal);
    break;
  case EMS_STRING:
    idx = EMShashString(&argString);
    break;
  default:
    fprintf(stderr, "EMS ERROR: EMSwriteIndexMap: Unknown mem type\n");
    return(-1);
  }

 int nTries = 0;
  if(EMSisMapped) {
    int matched = false;
    while(nTries < MAX_OPEN_HASH_STEPS  &&  !matched) {
      idx = idx % bufInt64[EMScbData(EMS_ARR_NELEM)];
      // Wait until the map key is FULL, mark it busy while map lookup is performed
      mapTags.byte = EMStransitionFEtag(&bufTags[EMSmapTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
      mapTags.tags.fe = EMS_FULL;  // When written back, mark FULL
      if(mapTags.tags.type  ==  idxType  ||  mapTags.tags.type == EMS_UNDEFINED) {
	switch(mapTags.tags.type) {
	case EMS_BOOLEAN:
	  if(boolArgVal == bufInt64[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_INTEGER:
	  if(intArgVal == bufInt64[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_FLOAT:
	  if(floatArgVal == bufDouble[EMSmapData(idx)]) matched = true;
	  break;
	case EMS_STRING: {
	  int64_t  keyStrOffset = bufInt64[EMSmapData(idx)];
	  if(strcmp(*argString, EMSheapPtr(keyStrOffset)) == 0) {
	    matched = true;
	  } }
	  break;
	case EMS_UNDEFINED:
	  // This map key index is still unused, so there was no match.
	  // Instead, allocate this element
	  bufTags[EMSmapTag(idx)].tags.type = idxType;
	  switch(idxType) {
	  case EMS_BOOLEAN:
	    bufInt64[EMSmapData(idx)] = boolArgVal;
	    break;
	  case EMS_INTEGER:
	    bufInt64[EMSmapData(idx)] = intArgVal;
	    break;
	  case EMS_FLOAT:
	    bufDouble[EMSmapData(idx)] = floatArgVal;
	    break;
	  case EMS_STRING: {
	    size_t  len = argString.length()+1;
	    int64_t  textOffset = 
	      emsMutexMem_alloc( (struct emsMem *) &bufChar[ bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] ],
				 len, (char*) &bufInt64[EMScbData(EMS_ARR_MEM_MUTEX)] );
	    if(textOffset < 0)  {
	      fprintf(stderr, "EMSwriteIndexMap: out of memory to store string");
	      idx = -1;
	    } else {
	      bufInt64[EMSmapData(idx)] = textOffset;
	      strcpy(EMSheapPtr(textOffset), *argString );
	    }
	  }
            break;
	  case EMS_UNDEFINED:
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
      if( !matched ) {
	// No match so set this key map back to full and try the next entry
	bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	nTries++;
	idx++;
      }
    }
  } else {  // Wasn't mapped, do bounds check
    if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
      idx = -1;
    }
  }

  if(nTries >= MAX_OPEN_HASH_STEPS) {
    idx = -1;
    fprintf(stderr, "EMSwriteIndexMap ran out of key mappings (returning %lld)\n", (long long int) idx);
  }

  return(idx);
}






//==================================================================
//  Execute Once -- Single execution 
//  OpenMP style, only first thread executes, remaining skip 
v8::Handle<v8::Value> EMSsingleTask(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;

  // Increment the tally of threads that have reached this statement
  int retval = __sync_fetch_and_add( &bufInt32[EMS_CB_SINGLE], 1 );

  //  If all threads have passed through the counter, reset fo next time
  if(retval == bufInt32[EMS_CB_NTHREADS]-1) {
    bufInt32[EMS_CB_SINGLE] = 0;
  }

  //  Return True if this thread was first to the counter, later
  //  threads return false
  return scope.Close(v8::Boolean::New((retval == 0) ? true : false));
}



//==================================================================
//  Critical Region Entry --  1 thread at a time passes this barrier
//
v8::Handle<v8::Value> EMScriticalEnter(const v8::Arguments& args)
{
  v8::HandleScope scope;
  RESET_NAP_TIME;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;

  // Acquire the mutual exclusion lock
  while( ! __sync_bool_compare_and_swap(&(bufInt32[EMS_CB_CRITICAL]), EMS_FULL, EMS_EMPTY) ) {
    NANOSLEEP;
  }

  return scope.Close(v8::Int32::New(0));
}


//==================================================================
//  Critical Region Exit  
v8::Handle<v8::Value> EMScriticalExit(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;

  // Test the mutual exclusion lock wasn't somehow lost
  if( bufInt32[EMS_CB_CRITICAL] != EMS_EMPTY ) {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", 
						   "EMScriticalExit: critical region mutex lost while locked?!"));
  } else
    bufInt32[EMS_CB_CRITICAL] = EMS_FULL;

  return scope.Close(v8::Int32::New(0));
}



//==================================================================
//  Phase Based Global Thread Barrier 
v8::Handle<v8::Value> EMSbarrier(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;

  int barPhase = bufInt32[EMS_CB_BARPHASE];    // Determine current phase of barrier
  int retval   = __sync_fetch_and_add( &bufInt32[EMS_CB_NBAR0 + barPhase], -1 );
  if(retval < 0)  {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSbarrier: Race condition at barrier"));
  }

  if(retval == 1) {
    //  This thread was the last to reach the barrier,
    //  Reset the barrier count for this phase and graduate to the next phase
    bufInt32[EMS_CB_NBAR0 + barPhase] = bufInt32[EMS_CB_NTHREADS];
    bufInt32[EMS_CB_BARPHASE] = !barPhase;
  } else {
    //  Wait for the barrier phase to change, indicating the last thread arrived
    RESET_NAP_TIME;
    while(barPhase == bufInt32[EMS_CB_BARPHASE]) { NANOSLEEP; }
  }

  return scope.Close(v8::Int32::New(0));
}





//==================================================================
//  Fetch and Add Atomic Memory Operation
//  Returns a+b where a is data in EMS memory and b is an argument
//
v8::Handle<v8::Value> EMSfaa(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  EMStag *bufTags = (EMStag *) emsBuf;

  if (args.Length() == 2) {
    int64_t   idx       = EMSwriteIndexMap(args);
    int64_t  *bufInt64  = (int64_t *) emsBuf;
    double   *bufDouble = (double *) emsBuf;
    char     *bufChar   = (char *) emsBuf;
    EMStag    oldTag;

    if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa: index out of bounds"));
    }

    // Wait until the data is FULL, mark it busy while FAA is performed
    oldTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
    oldTag.tags.fe = EMS_FULL;  // When written back, mark FULL
    int argType = EMSv8toEMStype(args[1], false);  // Never add to an object, treat as string
    switch(oldTag.tags.type) {
    case EMS_BOOLEAN: {    //  Bool + _______
      int64_t retBool = bufInt64[EMSdataData(idx)];  // Read original value in memory
      switch(argType) {
      case EMS_INTEGER:   //  Bool + Int
	bufInt64[EMSdataData(idx)] += args[1]->ToInteger()->Value();
	oldTag.tags.type = EMS_INTEGER;
	break;
      case EMS_FLOAT:     //  Bool + Float
	bufDouble[EMSdataData(idx)] = 
	  (double)bufInt64[EMSdataData(idx)] + args[1]->ToNumber()->Value();
	oldTag.tags.type = EMS_FLOAT;
	break;
      case EMS_UNDEFINED: //  Bool + undefined
	bufDouble[EMSdataData(idx)] = NAN;	
	oldTag.tags.type = EMS_FLOAT;
	break;
      case EMS_BOOLEAN:   //  Bool + Bool
	bufInt64[EMSdataData(idx)] += args[1]->ToBoolean()->Value();
	oldTag.tags.type = EMS_INTEGER;
	break;
      case EMS_STRING: {   //  Bool + string
	v8::String::Utf8Value argString(args[1]);
	int64_t  len = argString.length() + 1 + 5;  //  String length + Terminating null + 'false'
	int64_t  textOffset;
	EMS_ALLOC(textOffset, len, "EMSfaa(bool+string): out of memory to store string" );
	sprintf( EMSheapPtr(textOffset), "%s%s",
		 bufInt64[ EMSdataData(idx) ] ? "true":"false", *argString);
	bufInt64[EMSdataData(idx)] = textOffset;
	oldTag.tags.type = EMS_STRING;
      }
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa: Data is BOOL, but FAA arg type is unknown"));
      }
      //  Write the new type and set the tag to Full, then return the original value
      bufTags[EMSdataTag(idx)].byte = oldTag.byte;
      if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      return scope.Close(v8::Boolean::New( retBool ));
    }  // End of:  Bool + ___

    case EMS_INTEGER: {
      int64_t retInt = bufInt64[EMSdataData(idx)];  // Read original value in memory
      switch(argType) {
      case EMS_INTEGER: {  // Int + int
	int64_t memInt = bufInt64[EMSdataData(idx)];
	if(memInt >= (1<<30)) {  // Possible integer overflow, convert to float
	  bufDouble[EMSdataData(idx)] = 
	    (double)bufInt64[EMSdataData(idx)] + (double)(args[1]->ToInteger()->Value());
	  oldTag.tags.type = EMS_FLOAT;
	} else { //  Did not overflow to flow, still an integer
	  bufInt64[EMSdataData(idx)] += args[1]->ToInteger()->Value();
	}
      }
	break;
      case EMS_FLOAT:     // Int + float
	bufDouble[EMSdataData(idx)] = (double)bufInt64[EMSdataData(idx)] + args[1]->ToNumber()->Value();
	oldTag.tags.type = EMS_FLOAT;
	break;
      case EMS_UNDEFINED: // Int + undefined
	bufDouble[EMSdataData(idx)] = NAN;
	oldTag.tags.type = EMS_FLOAT;
	break;
      case EMS_BOOLEAN:   // Int + bool
	bufInt64[EMSdataData(idx)] += args[1]->ToBoolean()->Value();
	break;
      case EMS_STRING: {   // int + string
	v8::String::Utf8Value argString(args[1]);
	int64_t  len = argString.length() + 1 + MAX_NUMBER2STR_LEN;
	int64_t  textOffset;
	EMS_ALLOC(textOffset, len, "EMSfaa(int+string): out of memory to store string" );
	sprintf( EMSheapPtr(textOffset), "%lld%s", (long long int)bufInt64[ EMSdataData(idx) ], *argString);
	bufInt64[EMSdataData(idx)] = textOffset;
	oldTag.tags.type = EMS_STRING;
      }
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa: Data is INT, but FAA arg type is unknown"));
      }
      //  Write the new type and set the tag to Full, then return the original value
      bufTags[EMSdataTag(idx)].byte = oldTag.byte;
      if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      return scope.Close(v8::Integer::New( retInt ));
    }  // End of: Integer + ____

    case EMS_FLOAT: {
      double retDbl = bufDouble[EMSdataData(idx)];
      switch(argType) {
      case EMS_INTEGER:   // Float + int
	bufDouble[EMSdataData(idx)] += (double)args[1]->ToInteger()->Value();
	break;
      case EMS_FLOAT:     // Float + float
	bufDouble[EMSdataData(idx)] += args[1]->ToNumber()->Value();
	break;
      case EMS_BOOLEAN:   // Float + boolean
	bufDouble[EMSdataData(idx)] += (double)args[1]->ToInteger()->Value();
	break;
      case EMS_STRING: {   // Float + string
	v8::String::Utf8Value argString(args[1]);
	int64_t  len = argString.length() + 1 + MAX_NUMBER2STR_LEN;
	int64_t  textOffset;
	EMS_ALLOC(textOffset, len, "EMSfaa(float+string): out of memory to store string" );
	sprintf( EMSheapPtr(textOffset), "%lf%s", bufDouble[ EMSdataData(idx) ], *argString);
	bufInt64[EMSdataData(idx)] = textOffset;
	oldTag.tags.type = EMS_STRING;
      }
	break;
      case EMS_UNDEFINED: // Float + Undefined
	bufDouble[EMSdataData(idx)] = NAN;
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa: Data is FLOAT, but arg type unknown"));
      }
      //  Write the new type and set the tag to Full, then return the original value
      bufTags[EMSdataTag(idx)].byte = oldTag.byte;
      if(EMSisMapped)  bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      return scope.Close(v8::Number::New( retDbl ));
    } //  End of: float + _______

    case EMS_STRING: {
      v8::Handle<v8::String>  retStr =
	v8::String::New((const char*)EMSheapPtr(bufInt64[EMSdataData(idx)]));
      int64_t  textOffset;
      int64_t  len;
      switch(argType) {
      case EMS_INTEGER: // string + int
	len = strlen( EMSheapPtr(bufInt64[EMSdataData(idx)]) ) + 1 + MAX_NUMBER2STR_LEN;
	EMS_ALLOC(textOffset, len, "EMSfaa(string+int): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "%s%lld",
		 EMSheapPtr(bufInt64[EMSdataData(idx)]),
		 (long long int) args[1]->ToInteger()->Value() );
	break;
      case EMS_FLOAT:   // string + dbl
	len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + MAX_NUMBER2STR_LEN;
	EMS_ALLOC(textOffset, len, "EMSfaa(string+dbl): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "%s%lf",
		 EMSheapPtr(bufInt64[EMSdataData(idx)]),
		 args[1]->ToNumber()->Value() );
	break;
      case EMS_STRING: { // string + string
	v8::String::Utf8Value argString(args[1]);
	len = strlen(EMSheapPtr(bufInt64[EMSdataData(idx)])) + 1 + argString.length();
	EMS_ALLOC(textOffset, len, "EMSfaa(string+string): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "%s%s",
		 EMSheapPtr(bufInt64[EMSdataData(idx)]),
		 *argString );
      }
	break;
      case EMS_BOOLEAN:   // string + bool
	static char strTrue[] = "true";
	static char strFalse[] = "false";
	char *tfString;
	if(args[1]->ToBoolean()->Value()) tfString = strTrue;
	else                              tfString = strFalse;
	len = strlen( EMSheapPtr(bufInt64[EMSdataData(idx)]) ) + 1 + strlen(tfString);
	EMS_ALLOC(textOffset, len, "EMSfaa(string+bool): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "%s%s",
		 EMSheapPtr(bufInt64[EMSdataData(idx)]),
		 tfString );
	break;
      case EMS_UNDEFINED: // string + undefined
        len = strlen( EMSheapPtr(bufInt64[EMSdataData(idx)]) ) + 1 + strlen("undefined");
	EMS_ALLOC(textOffset, len, "EMSfaa(string+bool): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "%s%s",
		 EMSheapPtr(bufInt64[EMSdataData(idx)]),
		 "undefined" );
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa(string+?): Unknown data type"));
      }
      EMS_FREE(bufInt64[EMSdataData(idx)]);
      bufInt64[EMSdataData(idx)] = textOffset;
      oldTag.tags.type = EMS_STRING;
      //  Write the new type and set the tag to Full, then return the original value
      bufTags[EMSdataTag(idx)].byte = oldTag.byte;
      if(EMSisMapped)  bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      return scope.Close(retStr);
    }  // End of: String + __________

    case EMS_UNDEFINED: {
      switch(argType) {  // Undefined + Int, dloat, bool, or undef
      case EMS_INTEGER:
      case EMS_FLOAT:
      case EMS_BOOLEAN:
      case EMS_UNDEFINED: 
        bufDouble[EMSdataData(idx)] = NAN;
	oldTag.tags.type = EMS_FLOAT;
	break;
      case EMS_STRING: { // Undefined + string
        v8::String::Utf8Value string(args[1]);
	int64_t  len = strlen(*string) + 1 + strlen("NaN");
	int64_t  textOffset;
	EMS_ALLOC(textOffset, len, "EMSfaa(undef+String): out of memory to store string");
	sprintf( EMSheapPtr(textOffset), "NaN%s", *string );
	bufInt64[EMSdataData(idx)] = textOffset;
	oldTag.tags.type = EMS_UNDEFINED;
      }
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa(Undefined+___: Unknown stored data type"));
      }
      //  Write the new type and set the tag to Full, then return the original value
      bufTags[EMSdataTag(idx)].byte = oldTag.byte;
      if(EMSisMapped)  bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      return v8::Undefined();
    }
    default:
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa(?+___: Unknown stored data type"));
    }
  }
  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSfaa: Wrong number of arguments"));
}






//==================================================================
//  Atomic Compare and Swap
//
v8::Handle<v8::Value> EMS_CAS(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);

  if (args.Length() == 3) {
    int64_t   idx       = EMSwriteIndexMap(args);
    int64_t  *bufInt64  = (int64_t *) emsBuf;
    double   *bufDouble = (double *) emsBuf;
    char     *bufChar   = (char *) emsBuf;
    EMStag   *bufTags   = (EMStag *) emsBuf;
    EMStag    oldTag, newTag;
    int64_t   boolMemVal  = false;
    int64_t   intMemVal   = -1;
    double    floatMemVal = 0.0;
    int64_t   textOffset;
    v8::Handle<v8::String>  stringMemVal;
    v8::String::Utf8Value oldString(args[1]);
    v8::String::Utf8Value newString(args[2]);
    if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMS_CAS: index out of bounds"));
    }

    int oldType = EMSv8toEMStype(args[1], false);  // Never CAS an object, treat as string
    int newType = EMSv8toEMStype(args[2], false);  // Never CAS an object, treat as string
    int memType = bufTags[EMSdataTag(idx)].tags.type;

    //  Wait for the memory to be Full, then mark it Busy while CAS works
    oldTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
    oldTag.tags.fe = EMS_FULL;
    int swapped = false;

    //  Compare the value in memory the the "old" CAS value
    if(oldType == memType) {
      switch(oldTag.tags.type) {
      case EMS_UNDEFINED:
	swapped = true;
	break;
      case EMS_BOOLEAN:
	boolMemVal = bufInt64[EMSdataData(idx)];
	if(boolMemVal == args[1]->ToBoolean()->Value())   swapped = true;
	break;
      case EMS_INTEGER:
	intMemVal = bufInt64[EMSdataData(idx)];
	if(intMemVal == args[1]->ToInteger()->Value())	  swapped = true;
	break;
      case EMS_FLOAT:
	floatMemVal = bufDouble[EMSdataData(idx)];
	if(floatMemVal == args[1]->ToNumber()->Value())	  swapped = true;
	break;
      case EMS_STRING:
	if(strcmp(EMSheapPtr(bufInt64[EMSdataData(idx)]), *oldString)==0) {
	  stringMemVal =
	    v8::String::New((const char*) EMSheapPtr(bufInt64[EMSdataData(idx)]));
	  swapped = true;
	}
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMS_CAS: oldTag not recognized"));
      }
    }

    //  If memory==old then write the new value
    newTag.tags.fe   = EMS_FULL;
    newTag.tags.rw   = 0;
    newTag.tags.type = memType;
    if(swapped) {
      newTag.tags.type = newType;
      switch(newType) {
      case EMS_UNDEFINED:
	bufInt64[EMSdataData(idx)] = args[2]->ToBoolean()->Value();
	break;
      case EMS_BOOLEAN:
	bufInt64[EMSdataData(idx)] = (int64_t)args[2]->ToBoolean()->Value();
	break;
      case EMS_INTEGER:
	bufInt64[EMSdataData(idx)] = args[2]->ToInteger()->Value();
	break;
      case EMS_FLOAT:
	bufDouble[EMSdataData(idx)] = args[2]->ToNumber()->Value();
	break;
      case EMS_STRING: {
	if(memType == EMS_STRING) EMS_FREE(bufInt64[EMSdataData(idx)]);
	EMS_ALLOC(textOffset, newString.length()+1, "EMS_CAS(string): out of memory to store string");
	strcpy( EMSheapPtr(textOffset), *newString );
	bufInt64[EMSdataData(idx)] = textOffset;
      }
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMS_CAS(): Unrecognized new type"));
      }
    }

    //  Set the tag back to Full and return the original value
    bufTags[EMSdataTag(idx)].byte = newTag.byte;
    //  If there is a map, set the map's tag back to full
    if(EMSisMapped)  bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
    switch(memType) {
    case EMS_UNDEFINED:
      return v8::Undefined();
    case EMS_BOOLEAN:
      return scope.Close(v8::Boolean::New(boolMemVal));
    case EMS_INTEGER:
      return scope.Close(v8::Integer::New(intMemVal));
    case EMS_FLOAT:
      return scope.Close(v8::Number::New(floatMemVal));
    case EMS_STRING: 
      return scope.Close(stringMemVal);
    default:
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMS_CAS(): Unrecognized mem type"));
    }
  } else {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMS_CASnumber wrong number of arguments"));
  }
}
    



//==================================================================
//  Read EMS memory, enforcing Full/Empty tag transitions
//
v8::Handle<v8::Value> EMSreadUsingTags(const v8::Arguments& args, // Index to read from
				       char initialFE,            // Block until F/E tags are this value
				       char finalFE)              // Set the tag to this value when done
{
  RESET_NAP_TIME;
  EMS_DECL(args);
  
  if (args.Length() < 1  ||  args.Length() > 2) {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreadFE: Wrong number of args"));
  }

  int64_t        idx  = EMSreadIndexMap(args);
  EMStag   *bufTags   = (EMStag *) emsBuf;
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  EMStag    newTag, oldTag, memTag;
  if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
    if(EMSisMapped) 
      return v8::Undefined();
    else
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreadUsingTags: index out of bounds"));
  }

  while(true) {
    memTag.byte = bufTags[EMSdataTag(idx)].byte;
    //  Wait until FE tag is not FULL
    if( initialFE ==  EMS_ANY  ||  
	(initialFE != EMS_RW_LOCK  &&  memTag.tags.fe == initialFE)  ||
	(initialFE ==  EMS_RW_LOCK  &&  
	 (memTag.tags.fe == EMS_RW_LOCK  ||  memTag.tags.fe == EMS_FULL)  &&
	 (memTag.tags.rw < ((1 << EMS_TYPE_NBITS_RW) -1))// Counter is already saturated
	 ) 
	) {
      newTag.byte = memTag.byte;
      oldTag.byte = memTag.byte;
      newTag.tags.fe = EMS_BUSY;
      if(initialFE == EMS_RW_LOCK) {
	newTag.tags.rw++;
      } else {
	oldTag.tags.fe = initialFE;
      }
      //  Transition FE from FULL to BUSY
      if( initialFE ==  EMS_ANY  ||
	  __sync_bool_compare_and_swap( &(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte ) ) {
	// Under BUSY lock:
	//   Read the data, then reset the FE tag, then return the original value in memory
	newTag.tags.fe = finalFE;
	//	  fprintf(stderr, "XXXX %3d: idx=%lld  type is=%d\n", EMSmyID, idx, newTag.tags.type);
	switch(newTag.tags.type) {
	case EMS_BOOLEAN: {
	  int64_t retBool = bufInt64[EMSdataData(idx)];
	  if(finalFE != EMS_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
	  if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	  return v8::Boolean::New(retBool);
	}
	case EMS_INTEGER: {
	  int64_t retInt = bufInt64[EMSdataData(idx)];
	  if(finalFE != EMS_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
	  if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	  return v8::Integer::New(retInt);
	}
	case EMS_FLOAT: {
	  double retFloat = bufDouble[EMSdataData(idx)];
	  if(finalFE != EMS_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
	  if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	  return v8::Number::New(retFloat);
	}
	case EMS_JSON:
	case EMS_STRING: {
	  v8::Handle<v8::String>  retStr =
	    v8::String::New((const char*)(EMSheapPtr(bufInt64[EMSdataData(idx)])));
	  if(finalFE != EMS_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
	  if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	  if(newTag.tags.type == EMS_JSON) {
	    v8::Local<v8::Object> retObj = v8::Object::New();
	    retObj->Set(v8::String::New("data"), retStr);
	    //fprintf(stderr, "obj: %s\n", *retStr);
	    return retObj;
	  } else {
	    //fprintf(stderr, "string: %s\n", *retStr);
	    return retStr;
	  }
	}
	case EMS_UNDEFINED: {
	  if(finalFE != EMS_ANY) bufTags[EMSdataTag(idx)].byte = newTag.byte;
	  if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	  return v8::Undefined();
	}
	default:
	  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreadFE unknown type"));
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
v8::Handle<v8::Value> EMSreadRW(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return scope.Close( EMSreadUsingTags(args, EMS_RW_LOCK, EMS_RW_LOCK) );
}



//==================================================================
//  Read when full and leave empty
v8::Handle<v8::Value> EMSreadFE(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return scope.Close( EMSreadUsingTags(args, EMS_FULL, EMS_EMPTY) );
}


//==================================================================
//  Read when full and leave Full
v8::Handle<v8::Value> EMSreadFF(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return scope.Close( EMSreadUsingTags(args, EMS_FULL, EMS_FULL) );
}


//==================================================================
//   Wrapper around read from an EMS array -- first determine the type
v8::Handle<v8::Value> EMSread(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return scope.Close( EMSreadUsingTags(args, EMS_ANY, EMS_ANY) );
}



//==================================================================
//  Decrement the reference counte of the multiple readers-single writer lock
//
v8::Handle<v8::Value> EMSreleaseRW(const v8::Arguments& args)
{
  v8::HandleScope scope;
  RESET_NAP_TIME;
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  EMStag    newTag, oldTag;
  if (args.Length() == 1) {
    int64_t idx  = EMSreadIndexMap(args);
    if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreleaseRW: invalid index"));
    }
    while(true) {
      oldTag.byte = bufTags[EMSdataTag(idx)].byte;
      newTag.byte = oldTag.byte;
      if( oldTag.tags.fe == EMS_RW_LOCK ) {
	//  Already under a RW lock
	if( oldTag.tags.rw == 0 ) {
	  //  Assert the RW count is consistent with the lock state
	  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreleaseRW: locked by Count already 0"));
	} else {
	  //  Decrement the RW reference count
	  newTag.tags.rw--;
	  //  If this is the last reader, set the FE tag back to full
	  if( newTag.tags.rw == 0 ) { newTag.tags.fe = EMS_FULL; }
	  //  Attempt to commit the RW reference count & FE tag
	  if( __sync_bool_compare_and_swap( &(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte) )
	    return scope.Close( v8::Integer::New( newTag.tags.rw ) );
	}
      } else {
	if(oldTag.tags.fe != EMS_BUSY) {
	  // Assert the RW lock being release is not in some other state then RW_LOCK or BUSY
	  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreleaseRW: Lost RW lock?  Not locked or busy"));
	}
      }
      // Failed to update the RW count, sleep and retry
      NANOSLEEP;
    }
  } else {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSreleaseRW: Wrong number of arguments"));
  }
}




//==================================================================
//  Write EMS honoring the F/E tags
//
v8::Handle<v8::Value> EMSwriteUsingTags(const v8::Arguments& args,  // Index to read from
					char initialFE,             // Block until F/E tags are this value
					char finalFE)               // Set the tag to this value when done
{
  v8::HandleScope scope;
  RESET_NAP_TIME;
  EMS_DECL(args);
  int64_t         idx = EMSwriteIndexMap(args);
  EMStag   *bufTags   = (EMStag *) emsBuf;
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  EMStag    newTag, oldTag, memTag;
  int stringIsJSON = false;
  if (args.Length() == 3) {
    stringIsJSON = args[2]->ToBoolean()->Value();
  }  else {
    if(args.Length() != 2)
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSwriteUsingTags: Wrong number of args"));
  }

  if(idx < 0  ||  idx >= bufInt64[EMScbData(EMS_ARR_NELEM)]) {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSwriteUsingTags: index out of bounds"));
  }

  // Wait for the memory to be in the initial F/E state and transition to Busy
  if(initialFE != EMS_ANY) {
    EMStransitionFEtag(&bufTags[EMSdataTag(idx)], initialFE, EMS_BUSY, EMS_ANY);
  }

  while(true) {
    memTag.byte = bufTags[EMSdataTag(idx)].byte;
    //  Wait until FE tag is not BUSY
    if(initialFE != EMS_ANY  ||  finalFE == EMS_ANY  ||  memTag.tags.fe != EMS_BUSY) {
      oldTag.byte = memTag.byte;
      newTag.byte = memTag.byte;
      if(finalFE != EMS_ANY)  newTag.tags.fe = EMS_BUSY;
      //  Transition FE from !BUSY to BUSY
      if( initialFE != EMS_ANY  ||  finalFE == EMS_ANY  ||  
	  __sync_bool_compare_and_swap( &(bufTags[EMSdataTag(idx)].byte), oldTag.byte, newTag.byte ) ) {
	//  If the old data was a string, free it because it will be overwritten
	if(oldTag.tags.type == EMS_STRING) { EMS_FREE(bufInt64[EMSdataData(idx)]); }

	// Store argument value into EMS memory
	switch(EMSv8toEMStype(args[1], stringIsJSON)) {
	case EMS_BOOLEAN:
	  bufInt64[EMSdataData(idx)] = args[1]->ToBoolean()->Value();
	  break;
	case EMS_INTEGER:
	  bufInt64[EMSdataData(idx)] = (int64_t)args[1]->ToInteger()->Value();
	  break;
	case EMS_FLOAT:
	  bufDouble[EMSdataData(idx)] = args[1]->ToNumber()->Value();
	  break;
	case EMS_JSON:
	case EMS_STRING:  {
	  v8::String::Utf8Value string(args[1]);
	  size_t  len = string.length()+1;
	  int64_t  textOffset;
	  EMS_ALLOC(textOffset, len, "EMSwriteUsingTags: out of memory to store string");
	  bufInt64[EMSdataData(idx)] = textOffset;
	  strcpy( EMSheapPtr(textOffset) , *string );
	}
	  break;
	case EMS_UNDEFINED:
	  bufInt64[EMSdataData(idx)] = 0xdeadbeef;
	  break;
	default:
	  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSwriteUsingTags: Unknown arg type"));
	}

	oldTag.byte      = newTag.byte;
	if(finalFE != EMS_ANY) {
	  newTag.tags.fe   = finalFE;
	  newTag.tags.rw   = 0;
	}
	newTag.tags.type = EMSv8toEMStype(args[1], stringIsJSON);
	if( finalFE != EMS_ANY  &&   bufTags[EMSdataTag(idx)].byte != oldTag.byte ) {
	  return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSwriteUsingTags: Lost tag lock while BUSY"));
	}
	//  Set the tags for the data (and map, if used) back to full to finish the operation
	bufTags[EMSdataTag(idx)].byte = newTag.byte;
	if(EMSisMapped) bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
	return v8::Boolean::New(true);
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
v8::Handle<v8::Value> EMSwriteXF(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return( scope.Close(EMSwriteUsingTags(args, EMS_ANY, EMS_FULL) ));
}

//==================================================================
//  WriteXE
v8::Handle<v8::Value> EMSwriteXE(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return( scope.Close(EMSwriteUsingTags(args, EMS_ANY, EMS_EMPTY) ));
}
  
//==================================================================
//  WriteEF
v8::Handle<v8::Value> EMSwriteEF(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return( scope.Close(EMSwriteUsingTags(args, EMS_EMPTY, EMS_FULL) ));
}

//==================================================================
//  Write
v8::Handle<v8::Value> EMSwrite(const v8::Arguments& args)
{
  v8::HandleScope scope;
  return( scope.Close(EMSwriteUsingTags(args, EMS_ANY, EMS_ANY) ));
}


//==================================================================
//  Set only the Full/Empty tag  from JavaScript 
//  without inspecting or modifying the data.
//
v8::Handle<v8::Value> EMSsetTag(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  EMStag   *bufTags = (EMStag *) emsBuf;
  EMStag    tag;
  int64_t   idx = args[0]->ToInteger()->Value();

  tag.byte = bufTags[EMSdataTag(idx)].byte;
  if(args[1]->ToBoolean()->Value()) {
    tag.tags.fe = EMS_FULL;
  } else {
    tag.tags.fe = EMS_EMPTY;
  }
  bufTags[EMSdataTag(idx)].byte = tag.byte;

  return( scope.Close( v8::Undefined() ) );
}





//==================================================================
//  Push onto stack 
//
v8::Handle<v8::Value> Push(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  EMStag    newTag;
  int       stringIsJSON = false;

  if(args.Length() == 2) {
    stringIsJSON = args[1]->ToBoolean()->Value();
  }

  // Wait until the stack top is full, then mark it busy while updating the stack
  EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_FULL, EMS_BUSY, EMS_ANY);
  int64_t idx =  bufInt64[EMScbData(EMS_ARR_STACKTOP)];
  bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
  if(idx == bufInt64[EMScbData(EMS_ARR_NELEM)] - 1) {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSpush: Ran out of stack entries"));
  }

  //  Wait until the target memory at the top of the stack is empty
  newTag.byte = EMStransitionFEtag( &bufTags[EMSdataTag(idx)], EMS_EMPTY, EMS_BUSY, EMS_ANY);
  newTag.tags.rw   = 0;
  newTag.tags.type = EMSv8toEMStype(args[0], stringIsJSON);
  newTag.tags.fe   = EMS_FULL;

  //  Write the value onto the stack
  switch(newTag.tags.type) {
  case EMS_BOOLEAN:
    bufInt64[EMSdataData(idx)] = (int64_t)args[0]->ToBoolean()->Value();
    break;
  case EMS_INTEGER:
    bufInt64[EMSdataData(idx)] = (int64_t)args[0]->ToInteger()->Value();
    break;
  case EMS_FLOAT:
    bufDouble[EMSdataData(idx)] = (double)args[0]->ToNumber()->Value();
    break;
  case EMS_JSON:
  case EMS_STRING: {
    v8::String::Utf8Value string(args[0]);
    size_t  len = string.length()+1;
    int64_t  textOffset;
    EMS_ALLOC(textOffset, len, "EMSpush: out of memory to store string");
    bufInt64[EMSdataData(idx)] = textOffset;
    strcpy( EMSheapPtr(textOffset) , *string );  }
    break;
  case EMS_UNDEFINED:
    bufInt64[EMSdataData(idx)] = 0xdeadbeef;
    break;
  default:
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "xEMSwrite: Unknown arg type"));
  }

  //  Mark the data on the stack as FULL
  bufTags[EMSdataTag(idx)].byte = newTag.byte;

  //  Push is complete, Mark the stack pointer as full
  bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe =  EMS_FULL;

  return scope.Close(v8::Integer::New(idx));
}



//==================================================================
//  Pop data from stack
//
v8::Handle<v8::Value> Pop(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  EMStag    dataTag;

  //  Wait until the stack pointer is full and mark it empty while pop is performed
  EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_FULL, EMS_BUSY, EMS_ANY);
  bufInt64[EMScbData(EMS_ARR_STACKTOP)]--;
  int64_t idx =  bufInt64[EMScbData(EMS_ARR_STACKTOP)];
  if(idx < 0) {
    //  Stack is empty, return undefined
    bufInt64[EMScbData(EMS_ARR_STACKTOP)] = 0;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    return scope.Close(v8::Undefined());
  }

  //  Wait until the data pointed to by the stack pointer is full, then mark it
  //  busy while it is copied, and set it to EMPTY when finished
  dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
  switch(dataTag.tags.type) {
  case EMS_BOOLEAN: {
    int64_t retBool = bufInt64[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].tags.fe = EMS_EMPTY;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    return scope.Close(v8::Boolean::New(retBool));
  }
  case EMS_INTEGER: {
    int64_t retInt = bufInt64[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].tags.fe = EMS_EMPTY;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    return scope.Close(v8::Integer::New(retInt));
  }
  case EMS_FLOAT: {
    double retFloat = bufDouble[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].tags.fe = EMS_EMPTY;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    return scope.Close(v8::Number::New(retFloat));
  }
  case EMS_JSON:
  case EMS_STRING: {
    v8::Handle<v8::String>  retStr =
      v8::String::New((const char*)EMSheapPtr(bufInt64[EMSdataData(idx)]));
    EMS_FREE(bufInt64[EMSdataData(idx)]);
    bufTags[EMSdataTag(idx)].tags.fe = EMS_EMPTY;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    if(dataTag.tags.type == EMS_JSON) {
      v8::Local<v8::Object> retObj = v8::Object::New();
      retObj->Set(v8::String::New("data"), retStr);
      return scope.Close(retObj);
    } else {
      return scope.Close(retStr);
    }
  }
  case EMS_UNDEFINED: {
    bufTags[EMSdataTag(idx)].tags.fe = EMS_EMPTY;
    bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
    return scope.Close(v8::Undefined());
  }
  default:
    return scope.Close(v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSpop unknown type")));
  }
}


//==================================================================
//  Enqueue data
//  Heap top and bottom are monotonically increasing, but the index
//  returned is a circular buffer.
//
v8::Handle<v8::Value> Enqueue(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  int stringIsJSON  = false;
  if(args.Length() == 2) {
    stringIsJSON = args[1]->ToBoolean()->Value();
  }

  //  Wait until the heap top is full, and mark it busy while data is enqueued
  EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_STACKTOP)], EMS_FULL, EMS_BUSY, EMS_ANY);
  int64_t idx =  bufInt64[EMScbData(EMS_ARR_STACKTOP)] % bufInt64[EMScbData(EMS_ARR_NELEM)];
  bufInt64[EMScbData(EMS_ARR_STACKTOP)]++;
  if(bufInt64[EMScbData(EMS_ARR_STACKTOP)] - bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] > 
     bufInt64[EMScbData(EMS_ARR_NELEM)] ) {
    return scope.Close(v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSenqueue: Ran out of stack entries")));
  }

  //  Wait for data pointed to by heap top to be empty, then set to Full while it is filled
  bufTags[EMSdataTag(idx)].tags.rw   = 0;
  bufTags[EMSdataTag(idx)].tags.type = EMSv8toEMStype(args[0], stringIsJSON);
  switch(bufTags[EMSdataTag(idx)].tags.type) {
  case EMS_BOOLEAN:
    bufInt64[EMSdataData(idx)] = (int64_t)args[0]->ToBoolean()->Value();
    break;
  case EMS_INTEGER:
    bufInt64[EMSdataData(idx)] = (int64_t)args[0]->ToInteger()->Value();
    break;
  case EMS_FLOAT:
    bufDouble[EMSdataData(idx)] = (double)args[0]->ToNumber()->Value();
    break;
  case EMS_STRING: {
      v8::String::Utf8Value string(args[0]);
      size_t  len = string.length()+1;
      int64_t  textOffset;
      EMS_ALLOC(textOffset, len, "EMSenqueue: out of memory to store string");
      bufInt64[EMSdataData(idx)] = textOffset;
      strcpy( EMSheapPtr(textOffset), *string );
    }
    break;
  case EMS_UNDEFINED:
    bufInt64[EMSdataData(idx)] = 0xdeadbeef;
    break;
  default:
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "xEMSwrite: Unknown arg type"));
  }

  //  Set the tag on the data to FULL
  bufTags[EMSdataTag(idx)].tags.fe  = EMS_FULL;

  //  Enqueue is complete, set the tag on the heap to to FULL
  bufTags[EMScbTag(EMS_ARR_STACKTOP)].tags.fe = EMS_FULL;
  return scope.Close(v8::Integer::New(idx % bufInt64[EMScbData(EMS_ARR_NELEM)]));
}



//==================================================================
//  Dequeue
v8::Handle<v8::Value> Dequeue(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  EMStag    dataTag;

  /*
  int64_t  index;
  int count = 0;
  if(EMSmyID == 0) {
    for(index = 0;  index < bufInt64[EMScbData(EMS_ARR_NELEM)];  index++ ) {
      if(bufTags[EMSmapTag(index)].tags.type != EMS_UNDEFINED) {
	fprintf(stderr, "%6d:  key= %s   val= %lld\n", index, EMSheapPtr(bufInt64[EMSmapData(index)]), bufInt64[EMSdataData(index)]);
	count++;
      }
    }
    fprintf(stderr, "Different keys=%d\n", count);
  }
  return scope.Close(v8::Integer::New(0)); 
  */

  //==========================================================================
  //==========================================================================
  //==========================================================================

  //  Wait for bottom of heap pointer to be full, and mark it busy while data is dequeued
  EMStransitionFEtag(&bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)], EMS_FULL, EMS_BUSY, EMS_ANY);
  int64_t   idx    = bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] % bufInt64[EMScbData(EMS_ARR_NELEM)];
  //  If Queue is empty, return undefined
  if(bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)]  >= bufInt64[EMScbData(EMS_ARR_STACKTOP)]) {
    bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)] = bufInt64[EMScbData(EMS_ARR_STACKTOP)] ;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    return scope.Close(v8::Undefined());
  }

  bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)]++;
  //  Wait for the data pointed to by the bottom of the heap to be full,
  //  then mark busy while copying it, and finally set it to empty when done
  dataTag.byte = EMStransitionFEtag(&bufTags[EMSdataTag(idx)], EMS_FULL, EMS_BUSY, EMS_ANY);
  dataTag.tags.fe    = EMS_EMPTY;
  switch(dataTag.tags.type) {
  case EMS_BOOLEAN: {
    int64_t retBool = bufInt64[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].byte = dataTag.byte;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    return scope.Close(v8::Boolean::New(retBool));
  }
  case EMS_INTEGER: {
    int64_t retInt = bufInt64[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].byte = dataTag.byte;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    return scope.Close(v8::Integer::New(retInt));
  }
  case EMS_FLOAT: {
    double retFloat = bufDouble[EMSdataData(idx)];
    bufTags[EMSdataTag(idx)].byte = dataTag.byte;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    return scope.Close(v8::Number::New(retFloat));
  }
  case EMS_JSON:
  case EMS_STRING: {
    v8::Handle<v8::String>  retStr =
      v8::String::New((const char*)(EMSheapPtr(bufInt64[EMSdataData(idx)])));
    EMS_FREE(bufInt64[EMSdataData(idx)]);
    bufTags[EMSdataTag(idx)].byte = dataTag.byte;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    if(dataTag.tags.type == EMS_JSON) {
      v8::Local<v8::Object> retObj = v8::Object::New();
      retObj->Set(v8::String::New("data"), retStr);
      return scope.Close(retObj);
    } else {
      return scope.Close(retStr);
    }
  }
  case EMS_UNDEFINED: {
    bufTags[EMSdataTag(idx)].byte = dataTag.byte;
    bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].tags.fe = EMS_FULL;
    return scope.Close(v8::Undefined());
  }
  default:
    return scope.Close(v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSdequeue unknown type")));
  }
}




//==================================================================
//  Parallel Loop -- context initialization
//
v8::Handle<v8::Value> EMSloopInit(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;
  int start    = 0;
  int end      = 0;
  int minChunk = 0;

  if(args.Length() != 4) {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSloopInit: Wrong number of args"));
  }

  start     = args[0]->ToInteger()->Value();
  end       = args[1]->ToInteger()->Value();
  v8::String::Utf8Value schedule(args[2]);
  minChunk  = args[3]->ToInteger()->Value();

  bufInt32[EMS_LOOP_IDX]      = start;
  bufInt32[EMS_LOOP_START]    = start;
  bufInt32[EMS_LOOP_END]      = end;
  if(strcmp(*schedule, "guided") == 0) {
      bufInt32[EMS_LOOP_CHUNKSZ]  = ((end-start)/2) / bufInt32[EMS_CB_NTHREADS];
      if(bufInt32[EMS_LOOP_CHUNKSZ] < minChunk)  bufInt32[EMS_LOOP_CHUNKSZ] = minChunk;
      bufInt32[EMS_LOOP_MINCHUNK] = minChunk;
      bufInt32[EMS_LOOP_SCHED]    = EMS_SCHED_GUIDED;
    } else {
    if(strcmp(*schedule, "dynamic")  ==  0) {
      bufInt32[EMS_LOOP_CHUNKSZ]  = 1;
      bufInt32[EMS_LOOP_MINCHUNK] = 1;
      bufInt32[EMS_LOOP_SCHED]    = EMS_SCHED_DYNAMIC;
    } else {
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSloopInit: Unknown schedule type"));
    }
  }
  return scope.Close(v8::Int32::New(EMS_LOOP_IDX));
}



//==================================================================
//  Determine the current block of iterations to assign to an
//  an idle thread
//  JQM TODO BUG  -- convert to 64 bit using  fe tags
//
v8::Handle<v8::Value> EMSloopChunk(const v8::Arguments& args)
{
  v8::HandleScope scope;
  EMS_DECL(args);
  int32_t  *bufInt32  = (int32_t *) emsBuf;

  int chunkSize = bufInt32[EMS_LOOP_CHUNKSZ];
  int start = __sync_fetch_and_add( &(bufInt32[EMS_LOOP_IDX]), chunkSize);
  int end   = start + chunkSize;

  if(start > bufInt32[EMS_LOOP_END]) end = 0;
  if(end > bufInt32[EMS_LOOP_END])   end = bufInt32[EMS_LOOP_END];

  v8::Handle<v8::Object> retObj = v8::Object::New();
  retObj->Set(v8::String::NewSymbol("start"), v8::Int32::New((int)start));
  retObj->Set(v8::String::NewSymbol("end"), v8::Int32::New((int)end));

  if(bufInt32[EMS_LOOP_SCHED] == EMS_SCHED_GUIDED) {
    //  Compute the size of the chunk the next thread should use
    int newSz =  ((bufInt32[EMS_LOOP_END]-start)/2) / bufInt32[EMS_CB_NTHREADS];
    if(newSz < bufInt32[EMS_LOOP_MINCHUNK]) newSz = bufInt32[EMS_LOOP_MINCHUNK];
    bufInt32[EMS_LOOP_CHUNKSZ] = newSz;
  }
  return scope.Close(retObj);
}




//==================================================================
//  Synchronize the EMS memory to persistent storage
//
v8::Handle<v8::Value> EMSsync(const v8::Arguments& args)
{
  v8::HandleScope scope;
#if 0
  EMS_DECL(args);
  int64_t  *bufInt64  = (int64_t *) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;
  int64_t    idx;
  if(args[0]->IsUndefined()) {
    idx    = 0;
  } else {
    idx    = args[0]->ToInteger()->Value();
  }
  
  EMStag     tag;
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

  //fprintf(stderr, "msync%lx  %lld  %d\n", emsBuf, length, flags); 
  resultIdx = msync((void*) emsBuf, pgsize, flags);
  /*
  if(tag.tags.type != EMS_UNDEFINED)
    //resultIdx = msync(&(bufInt64[idx]), sizeof(int64_t), flags);
    resultIdx = msync( PGALIGN(&bufInt64[idx]), pgsize, flags);
  if(tag.tags.type  ==  EMS_STRING)
    //resultStr = msync(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]]),
    //	      strlen(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]])),
    // flags);
    resultStr = msync( PGALIGN( &(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]]) ),
		       strlen(&(bufChar[bufInt64[EMScbData(EMS_ARR_HEAPBOT)] + bufInt64[idx]])),
		       flags);
  //resultTag = msync(&(bufTags[EMSdataTag(idx)].byte), 1, flags);
  //fprintf(stderr, "msync(%llx  %lld  %d\n", PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), pgsize, flags);
  //  resultTag = msync(PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), pgsize, flags);
  //  resultTag = msync(PGALIGN(&(bufTags[EMSdataTag(idx)].byte)), 1, flags);
  //fprintf(stderr, "result  %d  %d  %d\n", resultIdx, resultStr, resultTag);
*/
#endif
  if(resultIdx == 0  &&  resultStr == 0  &&  resultTag == 0)
    return v8::True();
  else
#endif
    return v8::False();
}






//==================================================================
//  EMS Entry Point:   Allocate and initialize the EMS domain memory
//
v8::Handle<v8::Value> initialize(const v8::Arguments& args)
{
  v8::HandleScope scope;
  if (args.Length() <= 1) {
    return v8::ThrowException(
	v8::Exception::Error(v8::String::New("initialize - Missing arguments")));
  }

  //  Parse all the arguments
  int64_t   nElements   =  args[0]->ToInteger()->Value();
  int64_t   heapSize    =  args[1]->ToInteger()->Value();
  int       useMap      =  args[2]->ToBoolean()->Value();
  v8::String::Utf8Value filename(args[3]);
  int       persist     =  args[4]->ToBoolean()->Value();
  int       useExisting =  args[5]->ToBoolean()->Value();
  int       doDataFill  =  args[6]->ToBoolean()->Value();
  // Data Fill type TBD during fill
  int       fillIsJSON  =  args[8]->ToBoolean()->Value();
  int       doSetFEtags =  args[9]->ToBoolean()->Value();
  int       setFEtags   =  args[10]->ToInteger()->Value();
                EMSmyID = args[11]->ToInteger()->Value();
  int       pinThreads  = args[12]->ToBoolean()->Value();
  int       nThreads    = args[13]->ToInteger()->Value();
  int       doMLock     = args[14]->ToBoolean()->Value();

  int fd;

  //fprintf(stderr, "EMS initialize: nElements=%lld   heapSize=%lld     useMap=%d     fname=%s|    persist=%d   useExisting=%d  doDataFill=%d      doSetFEtags=%d     setFEtags=%d     EMSID=%d    pinThreads=%d   nThreads=%d  mlock=%d\n",  nElements, heapSize, useMap, *filename, persist, useExisting, doDataFill, doSetFEtags, setFEtags, EMSmyID, pinThreads, nThreads, doMLock);
  //  Node 0 is first and always has mutual excusion during intialization
  //  perform once-only initialization here
  if(EMSmyID == 0) {
    if(!useExisting) {
      unlink(*filename);
      shm_unlink(*filename);
    }
  }

  if(persist) 
    fd = open(*filename, O_APPEND | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  else 
    fd = shm_open(*filename,  O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  size_t  nMemBlocks = (heapSize / EMS_MEM_BLOCKSZ)+1;
  size_t  nMemBlocksPow2 = emsNextPow2(nMemBlocks);
  size_t  nMemLevels     = __builtin_ctzl(nMemBlocksPow2);
  size_t  bottomOfMap    = -1;
  size_t  bottomOfMalloc = -1;
  size_t  bottomOfHeap   = -1;
  size_t  filesize;

  bottomOfMap  = EMSdataTagWord(nElements)+EMSwordSize;  // Map begins 1 word AFTER the last tag word of data
  if(useMap) {
    bottomOfMalloc = bottomOfMap + bottomOfMap;
  } else {
    bottomOfMalloc = bottomOfMap;
  }
  bottomOfHeap = bottomOfMalloc + sizeof(struct emsMem) + (nMemBlocksPow2 * 2 -2);

  if(nElements <= 0) {
    filesize = EMS_CB_LOCKS + nThreads;   // EMS Control Block
    filesize *= sizeof(int);
  } else {
    filesize = bottomOfHeap + (nMemBlocksPow2 * EMS_MEM_BLOCKSZ);
  }
  if( ftruncate(fd, filesize) != 0 ) {
    if(errno != EINVAL) {
      fprintf(stderr, "EMS: Error during initialization, unable to set memory size to %lld bytes\n", (long long int)filesize);
      return v8::ThrowException(node::ErrnoException(errno, "EMS", "Unable to resize domain memory"));
    }
  }

  char* emsBuf = (char *) mmap(0, filesize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)0);
  if(emsBuf == MAP_FAILED)  {
    return v8::ThrowException(node::ErrnoException(errno, "EMS", "Unable to map domain memory"));  }
  close(fd);

  if(doMLock  ||  nElements <= 0) {  // lock RAM if requested or is master control block
    if(mlock((void*) emsBuf, filesize) != 0) {
      fprintf(stderr, "EMS thread %d did not lock EMS memory to RAM for %s\n", EMSmyID, *filename);
    } else {
      // success
    }
  }

  int64_t  *bufInt64  = (int64_t *) emsBuf;
  double   *bufDouble = (double *) emsBuf;
  char     *bufChar   = (char *) emsBuf;
  int     * bufInt32  = (int32_t*) emsBuf;
  EMStag   *bufTags   = (EMStag *) emsBuf;

  if(EMSmyID == 0) {
    if(nElements <= 0) {   // This is the EMS CB
      bufInt32[EMS_CB_NTHREADS] = nThreads;
      bufInt32[EMS_CB_NBAR0]    = nThreads;
      bufInt32[EMS_CB_NBAR1]    = nThreads;
      bufInt32[EMS_CB_BARPHASE] = 0;
      bufInt32[EMS_CB_CRITICAL] = 0;
      bufInt32[EMS_CB_SINGLE]   = 0;
      for(int i = EMS_CB_LOCKS;  i < EMS_CB_LOCKS + nThreads;  i++) {
	bufInt32[i] = EMS_BUSY;  //  Reset all locks
      }
    } else {   //  This is a user data domain
      if(!useExisting) {
	EMStag  tag;
	tag.tags.rw   = 0;
	tag.tags.type = EMS_INTEGER;
	tag.tags.fe   = EMS_FULL;
	bufInt64[EMScbData(EMS_ARR_NELEM)]     = nElements;
	bufInt64[EMScbData(EMS_ARR_HEAPSZ)]    = heapSize;     // Unused?
	bufInt64[EMScbData(EMS_ARR_MAPBOT)]    = bottomOfMap / EMSwordSize;
	bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] = bottomOfMalloc;
	bufInt64[EMScbData(EMS_ARR_HEAPBOT)]   = bottomOfHeap;
	bufInt64[EMScbData(EMS_ARR_Q_BOTTOM)]  = 0;
	bufTags[EMScbTag(EMS_ARR_Q_BOTTOM)].byte   = tag.byte;
	bufInt64[EMScbData(EMS_ARR_STACKTOP)]  = 0;
	bufTags[EMScbTag(EMS_ARR_STACKTOP)].byte   = tag.byte;
	bufInt64[EMScbData(EMS_ARR_MEM_MUTEX)] = EMS_EMPTY;
	bufInt64[EMScbData(EMS_ARR_FILESZ)]    = filesize;
	struct emsMem *emsMemBuffer = (struct emsMem *) &bufChar[ bufInt64[EMScbData(EMS_ARR_MALLOCBOT)] ];
	emsMemBuffer->level = nMemLevels;
      }
    }
  }

  if(pinThreads) {
#if defined(__linux)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((EMSmyID % nThreads), &cpuset);  // Round-robin over-subscribed systems
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
#endif
  }
  EMStag    tag;
  tag.tags.rw   = 0;
  int64_t  iterPerThread = (nElements / nThreads) + 1;
  int64_t  startIter     = iterPerThread * EMSmyID;
  int64_t  endIter       = iterPerThread * (EMSmyID+1);
  if(endIter > nElements)  endIter = nElements;
  for(int64_t  idx = startIter;  idx < endIter;  idx++) {
    tag.byte = bufTags[EMSdataTag(idx)].byte;
    if(doDataFill) {
      tag.tags.type = EMSv8toEMStype(args[7], fillIsJSON);
      switch(tag.tags.type) {
      case EMS_INTEGER:
	bufInt64[EMSdataData(idx)] = args[7]->ToInteger()->Value();
	break;
      case EMS_FLOAT:
	bufDouble[EMSdataData(idx)] = args[7]->ToNumber()->Value();
	break;
      case EMS_UNDEFINED:
	bufInt64[EMSdataData(idx)] = 0xdeadbeef;
	break;
      case EMS_BOOLEAN:
	bufInt64[EMSdataData(idx)] = args[7]->ToBoolean()->Value();
	break;
      case EMS_JSON:
      case EMS_STRING: {
	v8::String::Utf8Value argString(args[7]);
	int64_t  len = argString.length() + 1;  //  String length + Terminating null
	int64_t  textOffset;
	EMS_ALLOC(textOffset, len, "EMSinit: out of memory to store string" );
	bufInt64[EMSdataData(idx)] = textOffset;
	strcpy( EMSheapPtr(textOffset), *argString );
      }
	break;
      default:
	return v8::ThrowException(node::ErrnoException(errno, "EMS", "EMSinit: type is unknown"));
      }
    }

    if(doSetFEtags) {
      if(setFEtags)  tag.tags.fe = EMS_FULL;
      else           tag.tags.fe = EMS_EMPTY;
    }

    if(doSetFEtags  ||  doDataFill) {
      bufTags[EMSdataTag(idx)].byte = tag.byte;
    }

    if(useMap) {
      bufTags[EMSmapTag(idx)].tags.fe = EMS_FULL;
      if(!useExisting) { bufTags[EMSmapTag(idx)].tags.type = EMS_UNDEFINED; }
    }
  }
  
  //  node::Buffer *slowBuffer = node::Buffer::New(emsBuf, filesize, EMSarrFinalize, (void *) filesize);
  //  JQM TODO  This kludge allows over-indexing 
  node::Buffer *slowBuffer = node::Buffer::New(emsBuf, 1, EMSarrFinalize, (void *) filesize);


  v8::Local<v8::Object> globalObj = v8::Context::GetCurrent()->Global();
  v8::Local<v8::Function> bufferConstructor = v8::Local<v8::Function>::Cast(globalObj->Get(buffer_symbol));
  //  v8::Handle<v8::Value> constructorArgs[3] = { slowBuffer->handle_, v8::Integer::New(filesize), v8::Integer::New(0) };
  //  JQM TODO  This kludge is required because Node checks the size and
  //            fails if the buffer is too large.  This limit does not appear
  //            to be documented or even necessary.  Indeed, the point of a
  //            buffer is to act as linearly addressable memory, so the buffer
  //            is declared to be length 1 and always over-indexed.  This is a
  //            kludge made necessary by the unnecessary size check.
  v8::Handle<v8::Value> constructorArgs[3] = { slowBuffer->handle_, v8::Integer::New(1), v8::Integer::New(0) };
  v8::Local<v8::Object> actualBuffer = bufferConstructor->NewInstance(3, constructorArgs);

  actualBuffer->Set(sync_symbol, v8::FunctionTemplate::New(EMSsync)->GetFunction());
  actualBuffer->Set(faa_symbol, v8::FunctionTemplate::New(EMSfaa)->GetFunction());
  actualBuffer->Set(CAS_symbol, v8::FunctionTemplate::New(EMS_CAS)->GetFunction());
  actualBuffer->Set(read_symbol, v8::FunctionTemplate::New(EMSread)->GetFunction());
  actualBuffer->Set(write_symbol, v8::FunctionTemplate::New(EMSwrite)->GetFunction());
  actualBuffer->Set(barrier_symbol, v8::FunctionTemplate::New(EMSbarrier)->GetFunction());
  actualBuffer->Set(single_symbol, v8::FunctionTemplate::New(EMSsingleTask)->GetFunction());
  actualBuffer->Set(criticalEnter_symbol, v8::FunctionTemplate::New(EMScriticalEnter)->GetFunction());
  actualBuffer->Set(criticalExit_symbol, v8::FunctionTemplate::New(EMScriticalExit)->GetFunction());
  actualBuffer->Set(readRW_symbol, v8::FunctionTemplate::New(EMSreadRW)->GetFunction());
  actualBuffer->Set(releaseRW_symbol, v8::FunctionTemplate::New(EMSreleaseRW)->GetFunction());
  actualBuffer->Set(readFE_symbol, v8::FunctionTemplate::New(EMSreadFE)->GetFunction());
  actualBuffer->Set(readFF_symbol, v8::FunctionTemplate::New(EMSreadFF)->GetFunction());
  actualBuffer->Set(writeXE_symbol, v8::FunctionTemplate::New(EMSwriteXE)->GetFunction());
  actualBuffer->Set(writeXF_symbol, v8::FunctionTemplate::New(EMSwriteXF)->GetFunction());
  actualBuffer->Set(writeEF_symbol, v8::FunctionTemplate::New(EMSwriteEF)->GetFunction());
  actualBuffer->Set(setTag_symbol, v8::FunctionTemplate::New(EMSsetTag)->GetFunction());
  actualBuffer->Set(push_symbol, v8::FunctionTemplate::New(Push)->GetFunction());
  actualBuffer->Set(pop_symbol, v8::FunctionTemplate::New(Pop)->GetFunction());
  actualBuffer->Set(enqueue_symbol, v8::FunctionTemplate::New(Enqueue)->GetFunction());
  actualBuffer->Set(dequeue_symbol, v8::FunctionTemplate::New(Dequeue)->GetFunction());
  actualBuffer->Set(loopInit_symbol, v8::FunctionTemplate::New(EMSloopInit)->GetFunction());
  actualBuffer->Set(loopChunk_symbol, v8::FunctionTemplate::New(EMSloopChunk)->GetFunction());
  actualBuffer->SetHiddenValue(buffer_symbol, slowBuffer->handle_);

  return scope.Close(actualBuffer);
}


static void RegisterModule(v8::Handle<v8::Object> target)
{
  v8::HandleScope scope;

  faa_symbol = NODE_PSYMBOL("faa");
  CAS_symbol = NODE_PSYMBOL("cas");
  read_symbol = NODE_PSYMBOL("read");
  write_symbol = NODE_PSYMBOL("write");
  barrier_symbol = NODE_PSYMBOL("barrier");
  single_symbol = NODE_PSYMBOL("singleTask");
  criticalEnter_symbol = NODE_PSYMBOL("criticalEnter");
  criticalExit_symbol = NODE_PSYMBOL("criticalExit");
  readRW_symbol  = NODE_PSYMBOL("readRW");
  releaseRW_symbol  = NODE_PSYMBOL("releaseRW");
  readFE_symbol  = NODE_PSYMBOL("readFE");
  readFF_symbol  = NODE_PSYMBOL("readFF");
  setTag_symbol = NODE_PSYMBOL("setTag");
  writeEF_symbol = NODE_PSYMBOL("writeEF");
  writeXF_symbol = NODE_PSYMBOL("writeXF");
  writeXE_symbol = NODE_PSYMBOL("writeXE");
  push_symbol = NODE_PSYMBOL("push");
  pop_symbol = NODE_PSYMBOL("pop");
  enqueue_symbol = NODE_PSYMBOL("enqueue");
  dequeue_symbol = NODE_PSYMBOL("dequeue");
  loopInit_symbol = NODE_PSYMBOL("loopInit");
  loopChunk_symbol = NODE_PSYMBOL("loopChunk");
  length_symbol = NODE_PSYMBOL("length");
  sync_symbol   = NODE_PSYMBOL("sync");
  buffer_symbol = NODE_PSYMBOL("Buffer");
  const v8::PropertyAttribute attribs = (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete);
  target->Set(v8::String::NewSymbol("initialize"),  v8::FunctionTemplate::New(initialize)->GetFunction(), attribs);
}

NODE_MODULE(ems, RegisterModule);
