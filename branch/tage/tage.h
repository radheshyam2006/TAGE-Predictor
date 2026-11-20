#ifndef BRANCH_TAGE_H
#define BRANCH_TAGE_H
// Simplified TAGE: bimodal base + NT partially-tagged tables with geometric history lengths.
// Defaults come from the -D macros below. Each parameter can also be set at run time with an environment
// variable, so ONE binary runs every experiment:
//   TAGE_NT (tables)  TAGE_LENS (e.g. 5,15,44,130)  TAGE_LOGT (log2 entries/table)  TAGE_TAG (tag bits)
//   TAGE_LOGBASE (log2 base entries)  TAGE_UPERIOD (log2 of the u-aging period, 0 = off)  TAGE_STATS_FILE
// With no variables set, behaviour is identical to the original version (checked, see README).
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
    uint8_t u = 0;  // 2-bit useful counter
  };
  // Incremental XOR-fold of the newest `len` history bits into `width` bits. Same function as the original
  // fold(): the history bit of age i lands at position (i % width).
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
  int uperiod_log = 18; // all u counters are halved every 2^18 updates (original: 256K)
  std::vector<int> HL;

  std::vector<uint8_t> base;                  // 2-bit counters
  std::vector<std::vector<entry>> T;          // tagged tables
  std::vector<Fold> fidx, ftag1, ftag2;       // folded histories (index, tag, tag')
  std::vector<uint8_t> ghist;                 // circular global history, ghist[head] = most recent
  std::size_t head = 0;
  uint64_t tick = 0;
  // state saved between predict and update (ChampSim calls them back to back)
  std::vector<uint32_t> idx;
  std::vector<uint16_t> tg;
  int provider = -1, alt = -1;
  bool pred = false, altpred = false, base_pred = false;
  // statistics (written at exit if TAGE_STATS_FILE is set)
  uint64_t n_cond = 0, n_miss = 0, n_alloc_ok = 0, n_alloc_fail = 0, n_aging = 0;
  std::vector<uint64_t> prov_cnt;

  tage(O3_CPU* cpu);
  ~tage();
  uint64_t storage_bits() const;
  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
};
#endif
