#ifndef BRANCH_TAGE_H
#define BRANCH_TAGE_H
// Simplified TAGE: bimodal base + NT partially-tagged tables with geometric history lengths.
// Defaults come from the -D macros below (as in the original version).  Every parameter can ALSO be
// overridden at run time with an environment variable, so one binary runs every experiment:
//   TAGE_NT TAGE_LENS(e.g. 5,15,44,130) TAGE_LOGT TAGE_TAG TAGE_LOGBASE
//   TAGE_UPERIOD (log2 of u-aging period, 0=off)  TAGE_USEALT  TAGE_NALLOC  TAGE_CONDONLY  TAGE_STATS_FILE
// With no env vars set, behaviour is bit-identical to the original.
#include <cstdint>
#include <string>
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
  // incremental XOR-fold of the newest `len` history bits into `width` bits.
  // Same function as the original fold(): bit of age i lands at position (i % width).
  struct Fold {
    uint32_t comp = 0; int len = 0, width = 1, outpoint = 0;
    void init(int l, int w) { comp = 0; len = l; width = w; outpoint = l % w; }
    void update(int newest, int oldest)
    {
      comp = (comp << 1) | static_cast<uint32_t>(newest);
      comp ^= static_cast<uint32_t>(oldest) << outpoint;
      comp ^= comp >> width;
      comp &= (1u << width) - 1u;
    }
  };

  int NT = TAGE_NT, LOGSZ = TAGE_LOGSZ, TAGEBITS = TAGE_TAGBITS, LOGBASE = TAGE_LOGBASE;
  int uperiod_log = 18, use_alt = 0, n_alloc = 1, cond_only = 0;
  std::vector<int> HL;

  std::vector<uint8_t> base;
  std::vector<std::vector<entry>> T;
  std::vector<Fold> fidx, ftag1, ftag2;
  std::vector<uint8_t> ghist; // circular buffer, ghist[head] = most recent
  std::size_t head = 0;
  uint64_t tick = 0;
  int use_alt_ctr = 0;
  // state saved between predict and update
  std::vector<uint32_t> idx;
  std::vector<uint16_t> tg;
  int provider = -1, alt = -1;
  bool pred = false, provider_pred = false, altpred = false, base_pred = false, provider_weak = false;
  // statistics
  uint64_t n_cond = 0, n_miss = 0, n_alloc_ok = 0, n_alloc_fail = 0, n_aging = 0, n_alt_used = 0;
  std::vector<uint64_t> prov_cnt;

  tage(O3_CPU* cpu);
  ~tage();
  uint64_t storage_bits() const;
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};
#endif
