#ifndef BRANCH_TAGE_H
#define BRANCH_TAGE_H
// Simplified TAGE: bimodal base + NT partially-tagged tables with geometric history lengths.
// Parameters can be overridden at compile time with -D (see tage.cc defaults).
#include <array>
#include <cstdint>
#include <vector>

#include "modules.h"

#ifndef TAGE_NT
#define TAGE_NT 4
#endif
#ifndef TAGE_HL
#define TAGE_HL 5, 15, 44, 130
#endif
#ifndef TAGE_LOGSZ
#define TAGE_LOGSZ 10
#endif
#ifndef TAGE_TAGBITS
#define TAGE_TAGBITS 9
#endif
#ifndef TAGE_LOGBASE
#define TAGE_LOGBASE 13
#endif

struct tage : champsim::modules::branch_predictor {
  struct entry {
    uint16_t tag = 0;
    int8_t ctr = 0; // 3-bit signed: -4..3, taken if >=0
    uint8_t u = 0;  // 2-bit useful
  };
  static constexpr int NT = TAGE_NT;
  static constexpr int HL[NT] = {TAGE_HL};
  static constexpr int LOGSZ = TAGE_LOGSZ, TAGEBITS = TAGE_TAGBITS, LOGBASE = TAGE_LOGBASE;

  std::vector<uint8_t> base = std::vector<uint8_t>(1u << LOGBASE, 2);
  std::array<std::vector<entry>, NT> T;
  std::vector<bool> ghist; // ghist[0] = most recent
  uint64_t tick = 0;
  // state saved between predict and update
  std::array<uint32_t, NT> idx{};
  std::array<uint16_t, NT> tg{};
  int provider = -1, alt = -1;
  bool pred = false, altpred = false, base_pred = false;

  tage(O3_CPU* cpu);
  uint32_t fold(int len, int bits) const;
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};
#endif
