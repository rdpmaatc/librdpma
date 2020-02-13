#pragma once

#include <emmintrin.h>
#include <immintrin.h>
#include <x86intrin.h>
#include <xmmintrin.h>

#include "r2/src/common.hh"
#include "r2/src/mem_block.hh"

#include "src/nvm_region.hh"

#include "./proto.hh"

namespace nvm {

using namespace r2;

inline void clwb(void *p) {
  asm volatile("clwb (%0)\n\t" : : "r"(p) : "memory");
}

inline void flush_cache(char *ptr, u64 size) {
  char *cur_ptr = ptr;
  for (u64 cur = 0; cur < size; cur += 64) {
    _mm_clflush(cur_ptr);
    r2::compile_fence();
    cur_ptr += 64;
  }
}

inline usize nt_memcpy(char *src, u64 size, char *target) {
  auto cur_ptr = src;
  usize cur = 0;

  for(cur = 0; cur < size;) {
    // in 64 bytes granulaity
    for (uint i = 0; i < 8; ++i) {
      // I believe that compiler will unroll this
      _mm_stream_pi((__m64 *)(target + cur), *((__m64 *)(src + cur)));
      cur += sizeof(u64);
    }
  }
  return cur;
}

inline usize NO_OPT nvm_write(char *src, u64 size, char *target) {
#if 0
  auto write_sz = 0;
  auto cur_ptr = src;
  usize cur = 0;
#if 0
  for (cur = 0; cur < size; cur += 64) {
    memcpy(target + cur, cur_ptr, std::min<u64>(64,size));
    r2::compile_fence();
    //    _mm_clflush(target + cur);
    clwb(target + cur);

    cur_ptr += 64;
  }
#endif
  asm volatile("sfence" : : : "memory");
  return cur;

#endif
  auto res = nt_memcpy(src,size,target);
  asm volatile("sfence" : : : "memory");
  return res;
}

inline usize NO_OPT nvm_read(char *src, u64 size, char *target) {
  char *cur_ptr = target;
  usize cur = 0;

  for (cur = 0; cur < size; cur += 64) {
    memcpy(cur_ptr,src + cur, std::min<u64>(64, size));
    r2::compile_fence();
    cur_ptr += 64;
  }
  return cur;
}


inline usize NO_OPT normal_write(char *src, u64 size, char *target,bool sync = false) {
#if 1
  auto write_sz = 0;

  char *cur_ptr = src;
  usize cur = 0;
#if 0
  for (cur = 0; cur < size; cur += 64) {
    //    _mm_prefetch((char *)cur_ptr, _MM_HINT_T0);
    __builtin_prefetch(cur_ptr);
    memcpy(target + cur, cur_ptr, std::min<u64>(64, size));
    if (sync)
      _mm_clflush(target + cur);
    r2::compile_fence();
    cur_ptr += 64;
  }
#endif
  auto memcpy_ret = (char *)(memcpy(target + cur,cur_ptr,size));
  //asm volatile("sfence" : : : "memory");
  //return memcpy_ret + size - target;
  return size;
#else
  //return nt_memcpy(src,size,target);

  auto memcpy_ret = (char *)(memcpy(target, src, size));
  //asm volatile("sfence" : : : "memory");
  return memcpy_ret + size - target;
#endif
}

/*!
  write the payload according to the msg in the nvm
 */
inline usize NO_OPT execute_nvm_ops(Arc<MemoryRegion> &nvm, ::r2::MemBlock &msg,
                                    bool sync,char *reply_buf) {
  Request *req = msg.interpret_as<Request>(sizeof(MsgHeader));
  ASSERT(req != nullptr);

  switch (req->read) {
  case 1: {
    char *payload = msg.interpret_as<char>(sizeof(MsgHeader) + sizeof(Request));
    char *server_buf_ptr = reinterpret_cast<char *>(nvm->addr) + req->addr;

    nvm_read(server_buf_ptr,req->payload,reply_buf);
    return req->payload;
  } break;
  case 0: {

    char *payload = msg.interpret_as<char>(sizeof(MsgHeader) + sizeof(Request));
    char *server_buf_ptr =
        reinterpret_cast<char *>(nvm->addr) + req->addr;
    r2::compile_fence();
    ASSERT(req->addr < nvm->sz)
        << "addr: " << (u64)(req->addr) << "nvm sz: " << nvm->sz << " "
        << sizeof(size_t);
    if (!sync) {
      r2::compile_fence();
      //ASSERT(req->payload <= normal_write(payload, req->payload, server_buf_ptr));
      ASSERT(memcpy(server_buf_ptr,payload,req->payload));
    }
    else {
      ASSERT(req->payload <= nvm_write(payload, req->payload, server_buf_ptr));
      //ASSERT(req->payload <= nt_memcpy(payload,req->payload,server_buf_ptr));
    }

  } break;
  default:
    ASSERT(false);
  }

  return 0;
}

} // namespace nvm
