#include "tage.h"

#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "instruction.h"

namespace {
int env_int(const char* n, int d) { const char* s = std::getenv(n); return s ? std::atoi(s) : d; }
constexpr std::size_t HBUF = 4096; // circular history buffer; max history length must be < HBUF
} // namespace

tage::tage(O3_CPU* cpu) : champsim::modules::branch_predictor(cpu)
{
  NT = env_int("TAGE_NT", NT);
  LOGSZ = env_int("TAGE_LOGT", LOGSZ);
  TAGEBITS = env_int("TAGE_TAG", TAGEBITS);
  LOGBASE = env_int("TAGE_LOGBASE", LOGBASE);
  uperiod_log = env_int("TAGE_UPERIOD", uperiod_log);
  use_alt = env_int("TAGE_USEALT", use_alt);
  n_alloc = env_int("TAGE_NALLOC", n_alloc);
  cond_only = env_int("TAGE_CONDONLY", cond_only);

  HL = {TAGE_HL};
  if (const char* s = std::getenv("TAGE_LENS")) {
    HL.clear();
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) HL.push_back(std::atoi(tok.c_str()));
  }
  if (static_cast<int>(HL.size()) < NT) { std::fprintf(stderr, "TAGE: need %d history lengths, got %zu\n", NT, HL.size()); std::exit(1); }
  HL.resize(static_cast<std::size_t>(NT));

  base.assign(std::size_t{1} << LOGBASE, 2);
  T.assign(static_cast<std::size_t>(NT), std::vector<entry>(std::size_t{1} << LOGSZ));
  fidx.resize(static_cast<std::size_t>(NT)); ftag1.resize(static_cast<std::size_t>(NT)); ftag2.resize(static_cast<std::size_t>(NT));
  for (int i = 0; i < NT; i++) {
    auto k = static_cast<std::size_t>(i);
    fidx[k].init(HL[k], LOGSZ);
    ftag1[k].init(HL[k], TAGEBITS);
    ftag2[k].init(HL[k], TAGEBITS - 1);
  }
  ghist.assign(HBUF, 0);
  idx.assign(static_cast<std::size_t>(NT), 0);
  tg.assign(static_cast<std::size_t>(NT), 0);
  prov_cnt.assign(static_cast<std::size_t>(NT) + 1, 0); // [0] = base, [i+1] = table i
}

uint64_t tage::storage_bits() const
{
  return (uint64_t{1} << LOGBASE) * 2 + static_cast<uint64_t>(NT) * (uint64_t{1} << LOGSZ) * static_cast<uint64_t>(TAGEBITS + 3 + 2);
}

bool tage::predict_branch(champsim::address ip)
{
  uint64_t pc = ip.to<uint64_t>() >> 2;
  uint32_t bi = pc & ((1u << LOGBASE) - 1);
  base_pred = base[bi] >= 2;
  provider = alt = -1;
  for (int i = 0; i < NT; i++) {
    auto k = static_cast<std::size_t>(i);
    idx[k] = (pc ^ (pc >> LOGSZ) ^ fidx[k].comp) & ((1u << LOGSZ) - 1);
    tg[k] = static_cast<uint16_t>((pc ^ ftag1[k].comp ^ (ftag2[k].comp << 1)) & ((1u << TAGEBITS) - 1));
  }
  for (int i = NT - 1; i >= 0; i--)
    if (T[static_cast<std::size_t>(i)][idx[static_cast<std::size_t>(i)]].tag == tg[static_cast<std::size_t>(i)]) {
      if (provider < 0) provider = i;
      else { alt = i; break; }
    }
  altpred = alt >= 0 ? T[static_cast<std::size_t>(alt)][idx[static_cast<std::size_t>(alt)]].ctr >= 0 : base_pred;
  provider_weak = false;
  if (provider >= 0) {
    const entry& e = T[static_cast<std::size_t>(provider)][idx[static_cast<std::size_t>(provider)]];
    provider_pred = e.ctr >= 0;
    provider_weak = (e.ctr == 0 || e.ctr == -1);
    pred = (use_alt && provider_weak && use_alt_ctr >= 0) ? altpred : provider_pred; // USE_ALT_ON_NA (off by default)
  } else {
    provider_pred = base_pred;
    pred = base_pred;
  }
  return pred;
}

void tage::last_branch_result(champsim::address ip, champsim::address, bool taken, uint8_t branch_type)
{
  uint64_t pc = ip.to<uint64_t>() >> 2;
  uint32_t bi = pc & ((1u << LOGBASE) - 1);
  const bool cond = (branch_type == static_cast<uint8_t>(BRANCH_CONDITIONAL));

  if (!cond && cond_only) goto update_history; // optional: train on conditional branches only

  ++n_cond;
  if (pred != taken) ++n_miss;
  ++prov_cnt[static_cast<std::size_t>(provider + 1)];

  // allocation on misprediction: longer-history tables with u==0 (original: one entry; TAGE_NALLOC allows more)
  if (pred != taken && provider < NT - 1) {
    int start = provider + 1, got = 0;
    for (int i = start; i < NT && got < n_alloc; i++) {
      auto k = static_cast<std::size_t>(i);
      if (T[k][idx[k]].u == 0) {
        T[k][idx[k]] = entry{tg[k], static_cast<int8_t>(taken ? 0 : -1), 0};
        ++got;
      }
    }
    if (got == 0) {
      ++n_alloc_fail;
      for (int i = start; i < NT; i++) { auto k = static_cast<std::size_t>(i); T[k][idx[k]].u--; } // decay useful bits
    } else {
      n_alloc_ok += static_cast<uint64_t>(got);
    }
  }

  // update
  if (provider >= 0) {
    auto& e = T[static_cast<std::size_t>(provider)][idx[static_cast<std::size_t>(provider)]];
    if (provider_pred != altpred) {
      if (provider_pred == taken) { if (e.u < 3) e.u++; }
      else if (e.u > 0) e.u--;
    }
    if (use_alt && provider_weak && provider_pred != altpred) { // USE_ALT_ON_NA monitor
      if (altpred == taken && use_alt_ctr < 7) ++use_alt_ctr;
      if (altpred != taken && use_alt_ctr > -8) --use_alt_ctr;
    }
    if (pred != provider_pred) ++n_alt_used;
    if (taken) { if (e.ctr < 3) e.ctr++; }
    else if (e.ctr > -4) e.ctr--;
  } else {
    if (taken) { if (base[bi] < 3) base[bi]++; }
    else if (base[bi] > 0) base[bi]--;
  }

  // periodic aging of useful bits (original: every 2^18 updates)
  if (uperiod_log > 0 && (++tick & ((uint64_t{1} << uperiod_log) - 1)) == 0) {
    ++n_aging;
    for (auto& t : T) for (auto& e : t) e.u >>= 1;
  }

update_history:
  head = (head + 1) % HBUF;
  ghist[head] = taken ? 1 : 0;
  for (int i = 0; i < NT; i++) {
    auto k = static_cast<std::size_t>(i);
    int oldest = ghist[(head + HBUF - static_cast<std::size_t>(HL[k])) % HBUF];
    fidx[k].update(taken, oldest);
    ftag1[k].update(taken, oldest);
    ftag2[k].update(taken, oldest);
  }
}

tage::~tage()
{
  const char* path = std::getenv("TAGE_STATS_FILE");
  if (!path) return;
  FILE* f = std::fopen(path, "w");
  if (!f) return;
  std::fprintf(f, "storage_bits=%llu cond=%llu miss=%llu alloc_ok=%llu alloc_fail=%llu aging=%llu alt_used=%llu\nprovider(base,T0..):",
               (unsigned long long)storage_bits(), (unsigned long long)n_cond, (unsigned long long)n_miss, (unsigned long long)n_alloc_ok,
               (unsigned long long)n_alloc_fail, (unsigned long long)n_aging, (unsigned long long)n_alt_used);
  for (auto c : prov_cnt) std::fprintf(f, " %llu", (unsigned long long)c);
  std::fprintf(f, "\n");
  std::fclose(f);
}
