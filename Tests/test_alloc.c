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
#include "../Addon/ems_alloc.h"
#include <stdio.h>


static void
test_size(struct emsMem *b, int64_t addr) {
  int64_t s = emsMem_size(b,addr);
  printf("size %lld (sz = %lld)\n",addr,s);
}


static int64_t
test_alloc(struct emsMem *b, int64_t sz) {
  int64_t r = emsMem_alloc(b,sz);
  printf("alloc %lld (sz= %lld)\n",r,sz);
  //	emsMem_dump(b);
  test_size(b, r);
  return r;
}

static void
test_free(struct emsMem *b, int64_t addr) {
  printf("free %lld\n",addr);
  emsMem_free(b,addr);
  // emsMem_dump(b);
}



int main() {
#define P(x)  printf(">>>>>>>%d %d\n", x,  __builtin_ctzl(emsNextPow2(x)))
  /*
  P(1);
  P(10);
  P(127);
  P(128);
  P(129);
  exit(1);
  */

  struct emsMem * b = emsMem_new(16);

  //  int64_t zz = test_alloc(b,65536);
  //  printf("should fail: %lld\n", zz);

  int64_t x1 = test_alloc(b,65535);
  //  int64_t x2 = test_alloc(b,1);
  emsMem_dump(b);

  test_free(b,x1);
  //  test_free(b,x2);
  emsMem_dump(b);

  int64_t m1 = test_alloc(b,4);
  test_size(b,m1);
  int64_t m2 = test_alloc(b,9);
  test_size(b,m2);
  int64_t m3 = test_alloc(b,3);
  //  struct emsMem *arr[100];
  int64_t arr[100];
  for(int i = 0;  i < 50;  i ++) {
    arr[i] = test_alloc(b, i*2);
  }
  test_size(b,m3);
  int64_t m4 = test_alloc(b,7);
  
  int64_t m5a = test_alloc(b,302);
  
  test_free(b,m3);
  test_free(b,m1);
  for(int64_t i = 0;  i < 50;  i ++) {
    test_free(b, arr[(i+13)%50]);
  }
  //	test_alloc(b, 1000);
  test_free(b,m4);
  test_free(b,m2);
  
  int64_t m5 = test_alloc(b,32);
  test_free(b,m5);
  
  int64_t m6 = test_alloc(b,0);
  test_free(b,m6);
  test_free(b,m5a);  
  emsMem_dump(b);

  emsMem_delete(b);
  return 0;
}
