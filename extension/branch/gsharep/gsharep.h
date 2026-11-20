#ifndef BRANCH_GSHAREP_H
#define BRANCH_GSHAREP_H
// Parameterised gshare: 2^GS_LOGSIZE two-bit counters, GS_HIST bits of global history
// (env vars GS_LOGSIZE default 14, GS_HIST default = GS_LOGSIZE).
#include <cstdint>
#include <vector>
#include "address.h"
#include "modules.h"

struct gsharep : champsim::modules::branch_predictor {
  using branch_predictor::branch_predictor;
  bool inited = false;
  unsigned logsize = 14, histlen = 14;
  uint64_t hist = 0;
  std::vector<uint8_t> table;
  void init();
  std::size_t index(champsim::address ip) const;
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};
#endif
