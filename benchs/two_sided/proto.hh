#pragma once

#include "rlib/core/qps/ud.hh"
#include "rlib/core/common.hh"

#include "rlib/core/rmem/handler.hh"

namespace nvm {

using namespace rdmaio;
using namespace rdmaio::qp;

enum MsgType : u8 { Req = 0, Reply, Connect, ConnectR };

struct __attribute__((packed)) MsgHeader {
  MsgType type;
  u32 magic = 73;
  u32 coro_id = 0;
  u32 sz = 0;
};

/*!
   used for UD connect
 */
struct __attribute__((packed)) ConnectReq {
  QPAttr attr;
};

/*!
  used for RC connect
 */
struct __attribute__((packed)) ConnectReq2 {
  RegAttr attr;
};

struct __attribute__((packed)) ConnectReply {
  u32 session_id;
  u32 coro_id;
};

struct __attribute__((packed)) Request {
  u64 payload;
  u64 addr;
  u8  read = 0; // if read == 1, just read the value
};
}// namespace nvm
