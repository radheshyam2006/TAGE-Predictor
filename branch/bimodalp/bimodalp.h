#ifndef BRANCH_BIMODALP_H
#define BRANCH_BIMODALP_H
// Parameterised bimodal: 2^BP_LOGSIZE two-bit counters (env var BP_LOGSIZE, default 14).
#include <cstdint>
#include <vector>
#include "address.h"
#include "modules.h"

struct bimodalp : champsim::modules::branch_predictor {
  using branch_predictor::branch_predictor;
  bool inited = false;
  unsigned logsize = 14;
  std::vector<uint8_t> table;
  void init();
  std::size_t index(champsim::address ip) const;
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};
#endif
