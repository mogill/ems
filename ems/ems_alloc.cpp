/*-----------------------------------------------------------------------------+
 |  Extended Memory Semantics (EMS)                            Version 0.1.7   |
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
#include "ems_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>

#define BUDDY_UNUSED 0
#define BUDDY_USED   1	
#define BUDDY_SPLIT  2
#define BUDDY_FULL   3


//-----------------------------------------------------------------------------+
//  Allocate memory for testing -- 
//  Performed as part of new EMS object initialization
struct emsMem * emsMem_new(int level) {
  size_t size = 1UL << level;
  printf("emsMem_new: malloc sz=%ld\n", size);
  struct emsMem * self = (struct emsMem *) malloc(sizeof(struct emsMem) + sizeof(uint8_t) * (size * 2 - 2));
  self->level = level;
  memset(self->tree , BUDDY_UNUSED , size*2-1);
  return self;
}

//-----------------------------------------------------------------------------+
//  De-allocate memory from testing, not part of EMS object
void emsMem_delete(struct emsMem * self) {
  free(self);
}



//-----------------------------------------------------------------------------+
//  Pow-2 utility functions for 64 bit
uint64_t emsNextPow2( uint64_t x) {
  if ( __builtin_popcountl(x) == 1 )    return x;
  // Yes, that will overflow on 63 bit numbers.  Hopefully someone will rewrite this by then.
  //  fprintf(stderr, ">>>lz=%d   shift=%d\n", __builtin_clzl(x), (64 - __builtin_clzl(x)));
  return(1UL << (64 - __builtin_clzl(x)));
}


static inline int64_t EMS_index_offset(int64_t index, int32_t level, int64_t max_level) {
  return ((index + 1) - (1UL << level)) << (max_level - level);
}



//-----------------------------------------------------------------------------+
//  Mark the parent buddy of this buddy
static void EMS_mark_parent(struct emsMem * self, int64_t index) {
  for (;;) {
    int64_t buddy = index - 1 + (index & 1) * 2;
    if (buddy > 0 && (self->tree[buddy] == BUDDY_USED ||	self->tree[buddy] == BUDDY_FULL)) {
      index = (index + 1) / 2 - 1;
      self->tree[index] = BUDDY_FULL;
    } else {
      return;
    }
  }
}



//-----------------------------------------------------------------------------+
//  Allocate new memory from the EMS heap
int64_t emsMem_alloc(struct emsMem * self , int64_t s) {
  int64_t size;
  size = emsNextPow2( ((s+(EMS_MEM_BLOCKSZ-1)) / EMS_MEM_BLOCKSZ));
  if(size == 0)  size++;
  int64_t length = 1UL << self->level;

  //  fprintf(stderr, "emsMem_alloc: self=%x   size=%ld   s=%ld    len=%ld\n", self, size, s, length);
  if (size > length)    return -1;

  int64_t index = 0;
  int32_t level = 0;

  while (index >= 0) {
    if (size == length) {
      if (self->tree[index] == BUDDY_UNUSED) {
	self->tree[index] = BUDDY_USED;
	EMS_mark_parent(self, index);
	return( EMS_index_offset(index, level, self->level) * EMS_MEM_BLOCKSZ );
      }
    } else {
      // size < length
      switch (self->tree[index]) {
      case BUDDY_USED:
      case BUDDY_FULL:
	break;
      case BUDDY_UNUSED:
	// split first
	self->tree[index] = BUDDY_SPLIT;
	self->tree[index*2+1] = BUDDY_UNUSED;
	self->tree[index*2+2] = BUDDY_UNUSED;
      default:
	index = index * 2 + 1;
	length /= 2;
	level++;
	continue;
      }
    }
    if (index & 1) {
      ++index;
      continue;
    }
    for (;;) {
      level--;
      length *= 2;
      index = (index+1)/2 -1;
      if (index < 0)
	return -1;
      if (index & 1) {
	++index;
	break;
      }
    }
  }

  return -1;
}



//-----------------------------------------------------------------------------+
//  Combine two buddies into one node
static void EMS_combine(struct emsMem * self, int64_t index) {
  for (;;) {
    int64_t buddy = index - 1 + (index & 1) * 2;
    if (buddy < 0 || self->tree[buddy] != BUDDY_UNUSED) {
      self->tree[index] = BUDDY_UNUSED;
      while (((index = (index + 1) / 2 - 1) >= 0) &&  self->tree[index] == BUDDY_FULL){
	self->tree[index] = BUDDY_SPLIT;
      }
      return;
    }
    index = (index + 1) / 2 - 1;
  }
}



//-----------------------------------------------------------------------------+
//  Release EMS memory back to the heap for reuse
void emsMem_free(struct emsMem * self, int64_t offset) {
  offset /= EMS_MEM_BLOCKSZ;
  assert( offset < (1L << self->level));
  int64_t left = 0;
  int64_t length = 1L << self->level;
  int64_t index = 0;

  for (;;) {
    switch (self->tree[index]) {
    case BUDDY_USED:
      assert(offset == left);
      EMS_combine(self, index);
      return;
    case BUDDY_UNUSED:
      assert(0);
      return;
    default:
      length /= 2;
      if (offset < left + length) {
	index = index * 2 + 1;
      } else {
	left += length;
	index = index * 2 + 2;
      }
      break;
    }
  }
}


//-----------------------------------------------------------------------------+
//  Return the size of a block of memory
int64_t emsMem_size(struct emsMem * self, int64_t offset) {
  assert( offset < (1L << self->level));
  int64_t left = 0;
  int64_t length = 1L << self->level;
  int64_t index = 0;

  for (;;) {
    switch (self->tree[index]) {
    case BUDDY_USED:
      assert(offset == left);
      return length;
    case BUDDY_UNUSED:
      assert(0);
      return length;
    default:
      length /= 2;
      if (offset < left + length) {
	index = index * 2 + 1;
      } else {
	left += length;
	index = index * 2 + 2;
      }
      break;
    }
  }
}



//-----------------------------------------------------------------------------+
//  Diagnostic state dump
static void  EMS_dump(struct emsMem * self, int64_t index, int32_t level) {
  switch (self->tree[index]) {
  case BUDDY_UNUSED:
    printf("(%lld:%ld)", EMS_index_offset(index, level, self->level) , 1L << (self->level - level));
    break;
  case BUDDY_USED:
    printf("[%lld:%ld]", EMS_index_offset(index, level, self->level) , 1L << (self->level - level));
    break;
  case BUDDY_FULL:
    printf("{");
    EMS_dump(self, index * 2 + 1 , level+1);
    EMS_dump(self, index * 2 + 2 , level+1);
    printf("}");
    break;
  default:
    printf("(");
    EMS_dump(self, index * 2 + 1 , level+1);
    EMS_dump(self, index * 2 + 2 , level+1);
    printf(")");
    break;
  }
}

void emsMem_dump(struct emsMem * self) {
  EMS_dump(self, 0 , 0);
  printf("\n");
}
