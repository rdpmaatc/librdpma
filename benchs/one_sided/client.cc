#include <gflags/gflags.h>

#include "rlib/core/lib.hh"

#include "r2/src/rdma/sop.hh"

#include "../gen_addr.hh"
#include "../latency.hh"
#include "../statucs.hh"
#include "../thread.hh"

#include "src/huge_region.hh"

DEFINE_string(addr, "localhost:8888", "Server address to connect to.");
DEFINE_int64(use_nic_idx, 0, "Which NIC to create QP");
DEFINE_int64(reg_nic_name, 73, "The name to register an opened NIC at rctrl.");
DEFINE_int64(reg_mem_name, 73, "The name to register an MR at rctrl.");
DEFINE_uint64(address_space, 1,
              "The random read/write space of the registered memory (in GB)");

DEFINE_uint32(seq_payload, 2,
              "The sequential read/write payload (in MB)");

DEFINE_int64(threads, 8, "Number of threads to use.");

DEFINE_uint64(window_sz, 10, "The window sz of each coroutine.");

DEFINE_int64(payload, 256, "Number of payload to read/write");

DEFINE_int64(id, 0, "");

DEFINE_bool(use_read, true, "");

DEFINE_bool(add_sync, false, "");
DEFINE_bool(random, false, "");

DEFINE_uint64(coros, 8, "Number of coroutine used per thread.");

using namespace rdmaio;
using namespace rdmaio::rmem;
using namespace rdmaio::qp;

using namespace test;

using namespace nvm;

using namespace r2;
using namespace r2::rdma;

volatile bool running = true;

static const int per_socket_cores = 12; // TODO!! hard coded
// const int per_socket_cores = 8;//reserve 2 cores

static int socket_0[] = {0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22,
                         24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46};

static int socket_1[] = {1,  3,  5,  7,  9,  11, 13, 15, 17, 19, 21, 23,
                         25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47};

int BindToCore(int t_id) {

  if (t_id >= (per_socket_cores * 2))
    return 0;

  int x = t_id;
  int y = 0;

#ifdef SCALE
  assert(false);
  // specific  binding for scale tests
  int mac_per_node = 16 / nthreads; // there are total 16 threads avialable
  int mac_num = current_partition % mac_per_node;

  if (mac_num < mac_per_node / 2) {
    y = socket_0[x + mac_num * nthreads];
  } else {
    y = socket_1[x + (mac_num - mac_per_node / 2) * nthreads];
  }
#else
  // bind ,andway
  if (x >= per_socket_cores) {
    // there is no other cores in the first socket
    y = socket_1[x - per_socket_cores];
  } else {
    y = socket_0[x];
  }

#endif

  // fprintf(stdout,"worker: %d binding %d\n",x,y);
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(y, &mask);
  sched_setaffinity(0, sizeof(mask), &mask);

  return 0;
}

// issue RDMA requests in a window
template <usize window_sz>
void rdma_window_read(RC *qp, u64 *start_buf, FastRandom &rand,
                      u64 &pending_reqs) {

  const auto address_space =
      static_cast<u64>(FLAGS_address_space) * (1024 * 1024 * 1024L) -
      FLAGS_payload;
  // const auto address_space = 14080 * 1024 - FLAGS_payload;

  u64 *buf = start_buf;
  for (uint i = 0; i < window_sz; ++i) {
    buf[i] = rand.next();
    u64 remote_addr = rand.next() % address_space;
    //    RDMA_LOG(2) << "read addr: " << remote_addr; sleep(1);

#if 0 // write case
    auto res_s = qp->send_normal(
                                 {.op = IBV_WR_RDMA_WRITE,
                                  .flags = IBV_SEND_INLINE | ((pending_reqs == 0) ? (IBV_SEND_SIGNALED) : 0),
                                  .len = sizeof(u64),
                                  .wr_id = 0},
                                 {.local_addr = reinterpret_cast<RMem::raw_ptr_t>(&(buf[i])),
                                  .remote_addr = remote_addr,
                                  .imm_data = 0});
#else
    auto res_s = qp->send_normal(
        {.op = IBV_WR_RDMA_READ,
         .flags = 0 | ((pending_reqs == 0) ? (IBV_SEND_SIGNALED) : 0),
         .len = FLAGS_payload,
         .wr_id = 0},
        {.local_addr = reinterpret_cast<RMem::raw_ptr_t>(&(buf[i])),
         .remote_addr = remote_addr,
         .imm_data = 0});
#endif
    RDMA_ASSERT(res_s == IOCode::Ok) << " error: " << res_s.desc;

    if (pending_reqs >= 32) {
      auto res_p = qp->wait_one_comp();
      RDMA_ASSERT(res_p == IOCode::Ok);

      pending_reqs = 0;
    } else
      pending_reqs += 1;
  }
}

template <usize window_sz>
void rdma_window_write(RC *qp, u64 *start_buf, FastRandom &rand,
                       u64 &pending_reqs) {

  const auto address_space =
      static_cast<u64>(FLAGS_address_space) * (1024 * 1024 * 1024L);

  // const auto address_space = 14080 * 1024 - FLAGS_payload;

  u64 *buf = start_buf;
  for (uint i = 0; i < window_sz; ++i) {
    buf[i] = rand.next();
    u64 remote_addr = rand.next() % address_space;
    // u64 remote_addr = i * sizeof(u64);
    //    RDMA_LOG(2) << "read addr: " << remote_addr; sleep(1);

#if 1 // write case
    auto res_s = qp->send_normal(
        {.op = IBV_WR_RDMA_WRITE,
         .flags =
             IBV_SEND_INLINE | ((pending_reqs == 0) ? (IBV_SEND_SIGNALED) : 0),
         .len = sizeof(u64),
         .wr_id = 0},
        {.local_addr = reinterpret_cast<RMem::raw_ptr_t>(&(buf[i])),
         .remote_addr = remote_addr,
         .imm_data = 0});
#else
    auto res_s = qp->send_normal(
        {.op = IBV_WR_RDMA_READ,
         .flags = 0 | ((pending_reqs == 0) ? (IBV_SEND_SIGNALED) : 0),
         .len = 8,
         .wr_id = 0},
        {.local_addr = reinterpret_cast<RMem::raw_ptr_t>(&(buf[i])),
         .remote_addr = remote_addr,
         .imm_data = 0});
#endif
    RDMA_ASSERT(res_s == IOCode::Ok) << " error: " << res_s.desc;

    if (pending_reqs >= 32) {
      auto res_p = qp->wait_one_comp();
      RDMA_ASSERT(res_p == IOCode::Ok);

      pending_reqs = 0;
    } else
      pending_reqs += 1;
  }
}

void seq_bench();

int main(int argc, char **argv) {

  gflags::ParseCommandLineFlags(&argc, &argv, true);

  std::vector<gflags::CommandLineFlagInfo> all_flags;
  gflags::GetAllFlags(&all_flags);
  for (auto f : all_flags) {
    if (f.current_value.size())
      RDMA_LOG(2) << f.name << ": " << f.current_value;
  }

  RDMA_LOG(4) << "client bootstrap with " << FLAGS_threads << " threads";

  //seq_bench();

  using TThread = Thread<int>;
  std::vector<std::unique_ptr<TThread>> threads;
  std::vector<Statics> statics(FLAGS_threads);

  auto address_space =
      static_cast<u64>(FLAGS_address_space) * (1024 * 1024 * 1024L) -
      FLAGS_payload;

  if (FLAGS_use_read) {
    RDMA_LOG(4) << "eval use one-sided READ";
  } else {
    RDMA_LOG(4) << "eval use one-sided WRITE";
  }

  // use huge page to for local RDMA buffer
  auto huge_region = HugeRegion::create(2 * 1024 * 1024).value();
  auto local_mem = std::make_shared<RMem>( // nvm_region->size(),
      huge_region->size(),
      [&huge_region](u64 s) -> RMem::raw_ptr_t {
        return huge_region->start_ptr();
      },
      [](RMem::raw_ptr_t ptr) {
        // currently we donot free the resource, since the region will use
        // to the end
      });

  for (uint thread_id = 0; thread_id < FLAGS_threads; ++thread_id) {

    threads.push_back(std::make_unique<TThread>([thread_id, address_space,
                                                 local_mem, huge_region,
                                                 &statics]() -> int {
      BindToCore(thread_id);
      // 1. create a local QP to use
      // below are platform specific opts
      auto idx = FLAGS_use_nic_idx;
      if (thread_id >= 12)
        idx -= 1;
      auto nic = RNic::create(RNicInfo::query_dev_names().at(idx)).value();

      auto qp = RC::create(nic, QPConfig()).value();

      // 2. create the pair QP at server using CM
      ConnectManager cm(FLAGS_addr);
      // RDMA_LOG(2) << "start to connect to server: " << FLAGS_addr;
      auto wait_res = cm.wait_ready(1000000, 4);
      if (wait_res ==
          IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
        RDMA_ASSERT(false) << "cm connect to server timeout " << wait_res.desc;

      char qp_name[64];
      snprintf(qp_name, 64, "%rc:%d:@%d", thread_id, FLAGS_id);
      u64 key = 0;

      int rnic_idx = idx;
      //int rnic_idx = 0;
      auto qp_res = cm.cc_rc(qp_name, qp, rnic_idx, QPConfig());
      RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);
      // RDMA_LOG(4) << "client fetch QP authentical key: " << key;

      // 3. create the local MR for usage, and create the remote MR for usage

      auto local_mr = RegHandler::create(local_mem, nic).value();

      auto fetch_res = cm.fetch_remote_mr(rnic_idx);
      RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
      auto remote_attr = std::get<1>(fetch_res.desc);

      fetch_res = cm.fetch_remote_mr(rnic_idx * 73 + 73);
      RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
      auto dram_mr = std::get<1>(fetch_res.desc);

      // RDMA_LOG(4) << "remote attr's key: " << remote_attr.key << " "
      //<< local_mr->get_reg_attr().value().key;

      qp->bind_remote_mr(remote_attr);
      auto local_attr = local_mr->get_reg_attr().value();
      qp->bind_local_mr(local_attr);

      // the benchmark code

      u64 bench_ops = 1000;
      FastRandom rand(0xdeadbeaf + FLAGS_id * 0xdddd + thread_id);

      // const u64 four_h_mb = 1 * 1024 * 1024;
      const u64 four_h_mb = 400 * 1024 * 1024;
      // const u64 four_h_mb = address_space / FLAGS_threads;
      ASSERT(four_h_mb > 0);

      RandomAddr rgen(address_space, 0); // random generator
      // RandomAddr rgen(four_h_mb, four_h_mb * thread_id);
      SeqAddr sgen(four_h_mb, four_h_mb * thread_id); // sequential generator

      r2::Timer timer;

      // RDMA_LOG(4) << "all done, start bench code!";
      // coroutine version
      SScheduler ssched;
      u64 *test_buf = (u64 *)(local_mem->raw_ptr);

      for (uint i = 0; i < FLAGS_coros; ++i) {
        ssched.spawn([local_attr, remote_attr, thread_id, test_buf, qp, &rand,
                      four_h_mb, address_space, &statics, &rgen,&dram_mr,
                      &sgen](R2_ASYNC) {
          //auto my_buf_off = FLAGS_payload * thread_id;
          //u64 *my_buf = (u64 *)(my_buf_off + (char *)test_buf);
          u64 *my_buf = (u64 *)((char *)test_buf + thread_id * 40960 +
                                R2_COR_ID() * 4096);

          const auto address_space =
              static_cast<u64>(FLAGS_address_space) * (1024 * 1024 * 1024L) -
              FLAGS_payload;

          if (FLAGS_add_sync) // add sync must be used with write
            assert(!FLAGS_use_read);

          // const u64 four_h_mb = 400 * 1024 * 1024;
          SROp op;

          FlatLatRecorder lats; // used to record lat

          r2::Timer t;
          t.reset();

          while (running) {

            /*
              We only record latency at the first thread.
              This is because latency timing using timer has overhead.
              Restricting the number of threads can minimize this overhead.
            */
            //if (thread_id == 0)
              t.reset();

            u64 write_addr = 0;
            if (FLAGS_random)
              write_addr = rgen.gen(rand);
            else {
              //write_addr = sgen.gen(FLAGS_payload);
              // write_addr = thread_id * 4096;
            }

            ASSERT(write_addr < address_space) << " write addr: " << write_addr;

            if (!FLAGS_add_sync) {
              // normal case read/write
              op.set_payload(&my_buf[0], FLAGS_payload)
                  .set_remote_addr(write_addr);

              int write_flag = 0;
              if (FLAGS_use_read) {
                op.set_read();
              } else {
                write_flag |= (FLAGS_payload < 64 ? IBV_SEND_INLINE : 0);
                op.set_write();
              }
              // write path
              if (1) {
                qp->bind_remote_mr(remote_attr);
                auto ret = op.execute(qp, IBV_SEND_SIGNALED | write_flag,
                                      R2_ASYNC_WAIT);
                ASSERT(ret == IOCode::Ok)
                    << RC::wc_status(ret.desc) << " " << ret.code.name();
              }

              //op.execute_sync(qp, write_flag | IBV_SEND_SIGNALED);

              if(0) {
                // additional read path to ensure flush
                op.set_remote_addr(write_addr + FLAGS_payload - sizeof(u8));
                op.set_read();
                op.set_payload(&my_buf[0], sizeof(u8));
                qp->bind_remote_mr(dram_mr);
                //op.set_payload(&my_buf[0],0);

                auto retr = op.execute(qp, IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
                ASSERT(retr == IOCode::Ok)
                    << RC::wc_status(retr.desc) << " " << retr.code.name();
              }

              //retr = op.execute(qp, IBV_SEND_SIGNALED, R2_ASYNC_WAIT);
              //ASSERT(retr == IOCode::Ok)
              //  << RC::wc_status(ret.desc) << " " << ret.code.name();
            } else {

#if 1 // the doorbell version of the request
      // DoorbellHelper<2> *dp = new DoorbellHelper<2>(IBV_WR_RDMA_WRITE);
      // DoorbellHelper<2> &doorbell
              DoorbellHelper<2> doorbell(IBV_WR_RDMA_WRITE);

              doorbell.next();

              // 1. setup the write WR
              doorbell.cur_wr().opcode = IBV_WR_RDMA_WRITE;
              doorbell.cur_wr().send_flags =
                  ((FLAGS_payload <= kMaxInlinSz) ? IBV_SEND_INLINE : 0);
              doorbell.cur_wr().wr.rdma.remote_addr =
                  remote_attr.buf + (write_addr);
              doorbell.cur_wr().wr.rdma.rkey = remote_attr.key;

              doorbell.cur_sge() = {.addr = (u64)(&my_buf[0]),
                                    .length = FLAGS_payload,
                                    .lkey = local_attr.key};

              // 2. setup the read WR
              // this request ensure that the previous write request is flushed
              // out of the NIC pipeline
#if 1
              if (1) {

                doorbell.next();

                auto &read_sr = doorbell.cur_wr();
                auto &read_sge = doorbell.cur_sge();

                read_sr.opcode = IBV_WR_RDMA_READ;
                read_sr.send_flags = IBV_SEND_SIGNALED;
                assert(FLAGS_payload >= sizeof(u8));
                // read the last byte
                read_sr.wr.rdma.remote_addr =
                  //remote_attr.buf + write_addr + FLAGS_payload - sizeof(u8);
                  dram_mr.buf + 4096 * thread_id * 64 * R2_COR_ID();

                /* XD: using a DRAM MR is ok, as the RDMA_send will also flush the buffer
                   This improve the performance of about 1us
                 */
                read_sr.wr.rdma.rkey = dram_mr.key;

                //read_sr.wr.rdma.rkey = remote_attr.key;


                read_sge.addr = (u64)(&my_buf[0]);
                //read_sge.length =
                //sizeof(u8); // again, read a byte is sufficient
                //read_sge.length = 1;
                //read_sge.length = 0;
                read_sge.length = sizeof(u8);
                read_sge.lkey = local_attr.key;
              }

              auto id = R2_COR_ID();

              // 3. send the doorbell
              auto res_d = op.execute_doorbell(qp, doorbell, R2_ASYNC_WAIT);
              ASSERT(res_d == IOCode::Ok)
                  << "error: " << RC::wc_status(res_d.desc);
#endif
            }

            // record the latency
            //if (thread_id == 0) {
              lats.add_one(t.passed_msec());
              statics[thread_id].float_data = lats.get_lat();
              //}
            statics[thread_id].inc(1);
            R2_YIELD;
          }
          R2_RET;
        });
      }
      ssched.run();
#endif
      return 0;
    }));

    /***********************************************************/

    // finally, some clean up, to delete my created QP at server
    // auto del_res = cm.delete_remote_rc(73, key);
    // RDMA_ASSERT(del_res == IOCode::Ok)
    //<< "delete remote QP error: " << del_res.desc;
  }
  for (auto &t : threads)
    t->start();

  sleep(1);
  Reporter::report_thpt(statics, 40);

  running = false;
  // RDMA_LOG(4) << "client returns";

  for (auto &t : threads) {
    t->join();
  }

  return 0;
}

void seq_bench() {

  u64 M = 1024 * 1024;
  u64 seq_payload =  FLAGS_seq_payload * M;
  //seq_payload = 1024;

  LOG(4) << "use seq_payload: "<< seq_payload;

  auto huge_region = HugeRegion::create(seq_payload).value();
  // convert to a format that can be register with RLib
  auto local_mem = huge_region->convert_to_rmem().value();

  usize thread_id = 0;
  BindToCore(thread_id);

  // 1. create a local QP to use
  // below are platform specific opts
  auto idx = FLAGS_use_nic_idx;
  if (thread_id >= 12)
    idx -= 1;
  auto nic = RNic::create(RNicInfo::query_dev_names().at(idx)).value();

  auto qp = RC::create(nic, QPConfig()).value();

  // 2. create the pair QP at server using CM
  ConnectManager cm(FLAGS_addr);
  // RDMA_LOG(2) << "start to connect to server: " << FLAGS_addr;
  auto wait_res = cm.wait_ready(1000000, 4);
  if (wait_res ==
      IOCode::Timeout) // wait 1 second for server to ready, retry 2 times
    RDMA_ASSERT(false) << "cm connect to server timeout " << wait_res.desc;

  char qp_name[64];
  snprintf(qp_name, 64, "%rc:%d:@%d", thread_id, FLAGS_id);
  u64 key = 0;

  // int rnic_idx = idx;
  int rnic_idx = 0;
  auto qp_res = cm.cc_rc(qp_name, qp, rnic_idx, QPConfig());
  RDMA_ASSERT(qp_res == IOCode::Ok) << std::get<0>(qp_res.desc);
  // RDMA_LOG(4) << "client fetch QP authentical key: " << key;

  // 3. create the local MR for usage, and create the remote MR for usage

  auto local_mr = RegHandler::create(local_mem, nic).value();

  auto fetch_res = cm.fetch_remote_mr(rnic_idx);
  RDMA_ASSERT(fetch_res == IOCode::Ok) << std::get<0>(fetch_res.desc);
  auto remote_attr = std::get<1>(fetch_res.desc);

  // RDMA_LOG(4) << "remote attr's key: " << remote_attr.key << " "
  //<< local_mr->get_reg_attr().value().key;

  qp->bind_remote_mr(remote_attr);
  auto local_attr = local_mr->get_reg_attr().value();
  qp->bind_local_mr(local_attr);

  r2::Timer t;
  t.reset();

  u64 transfered = 0;

  while(1) {
    // send the request
    char *local_ptr = (char *)(huge_region->addr);
    SROp op;
    op.set_payload(local_ptr,static_cast<u32>(seq_payload)).set_remote_addr(0);
    if (FLAGS_use_read) {
      op.set_read();
    } else {
      op.set_write();
    }
    auto res_s = op.execute_sync(qp,IBV_SEND_SIGNALED);
    ASSERT(res_s == IOCode::Ok);

    transfered += seq_payload;
    if (t.passed_sec() > 1) {
      // print the bandwidth

      double bandwidth =
        (transfered / static_cast<double>(t.passed_msec())) * 1000000.0;
      bandwidth /= (1024 * 1024 * 1024);

      LOG(4) << "mointor bandwidth: " << bandwidth << " GB/s";

      transfered = 0;
      t.reset();
    }
    // end evaluation loop
  }
}
