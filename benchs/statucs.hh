#include "rlib/core/common.hh" // for u64

#include "./timer.hpp"

#include <iomanip>
#include <iostream>
#include <vector>

namespace nvm {

using namespace rdmaio;

struct alignas(128) Statics {
  u64 counter = 0;        // record thpt
  double float_data = 0; // record lat

  void increment() { inc(1); }

  void inc(u64 num) __attribute__((optimize(0)))  { counter += num; }

private:
  char pad[128 - sizeof(u64)];
};

class Reporter {

  template <class T>
  static std::string format_value(T value, int precission = 4) {
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << std::setprecision(precission) << value;
    return ss.str();
  }

public:
  static double report_thpt(const std::vector<Statics> &statics, int epoches) {

    std::vector<Statics> old_statics(statics.size());

    r2::Timer timer;
    for (int epoch = 0; epoch < epoches; epoch += 1) {
      sleep(1);

      u64 sum = 0;
      double lat = 0.0;
      double lat_cnt = 0;

      // now report the throughput
      for (uint i = 0; i < statics.size(); ++i) {
        auto temp = statics[i].counter;
        auto thread_thpt = (temp - old_statics[i].counter);
        //LOG(4) << "thread: " << i << " thpt: " << thread_thpt;
        sum += thread_thpt;
        old_statics[i].counter = temp;

        if (statics[i].float_data != 0) {
          lat_cnt += 1;
          lat += statics[i].float_data;
        }
      }

      if (lat_cnt > 0)
        lat = lat / lat_cnt;

      double passed_msec = timer.passed_msec();
      double res = static_cast<double>(sum) / passed_msec * 1000000.0;
      asm volatile("" : : : "memory");
      timer.reset();

      RDMA_LOG(3) << "epoch @ " << epoch << ": thpt: " << // format_value(res, 0)
                  res
                  << " reqs/sec, " << " with lat: " << lat << " us.";
    }
    return 0.0;
  }
};

} // namespace nvm
