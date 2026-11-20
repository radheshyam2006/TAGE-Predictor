#include "tage.h"

#include <cstdlib>

constexpr int tage::HL[];

tage::tage(O3_CPU* cpu) : champsim::modules::branch_predictor(cpu), ghist(HL[NT - 1] + 1, false)
{
  for (auto& t : T) t.assign(1u << LOGSZ, entry{});
}

uint32_t tage::fold(int len, int bits) const
{
  uint32_t r = 0;
  for (int i = 0; i < len; i++)
    if (ghist[i]) r ^= 1u << (i % bits);
  return r;
}

bool tage::predict_branch(champsim::address ip)
{
  uint64_t pc = ip.to<uint64_t>() >> 2;
  uint32_t bi = pc & ((1u << LOGBASE) - 1);
  base_pred = base[bi] >= 2;
  provider = alt = -1;
  for (int i = 0; i < NT; i++) {
    idx[i] = (pc ^ (pc >> LOGSZ) ^ fold(HL[i], LOGSZ)) & ((1u << LOGSZ) - 1);
    tg[i] = (pc ^ fold(HL[i], TAGEBITS) ^ (fold(HL[i], TAGEBITS - 1) << 1)) & ((1u << TAGEBITS) - 1);
  }
  for (int i = NT - 1; i >= 0; i--)
    if (T[i][idx[i]].tag == tg[i]) {
      if (provider < 0) provider = i;
      else { alt = i; break; }
    }
  altpred = alt >= 0 ? T[alt][idx[alt]].ctr >= 0 : base_pred;
  pred = provider >= 0 ? T[provider][idx[provider]].ctr >= 0 : base_pred;
  return pred;
}

void tage::last_branch_result(champsim::address ip, champsim::address, bool taken, uint8_t)
{
  uint64_t pc = ip.to<uint64_t>() >> 2;
  uint32_t bi = pc & ((1u << LOGBASE) - 1);

  // allocation on misprediction: pick a longer-history table with u==0
  if (pred != taken && provider < NT - 1) {
    int start = provider + 1;
    bool done = false;
    for (int i = start; i < NT && !done; i++)
      if (T[i][idx[i]].u == 0) {
        T[i][idx[i]] = entry{tg[i], (int8_t)(taken ? 0 : -1), 0};
        done = true;
      }
    if (!done)
      for (int i = start; i < NT; i++) T[i][idx[i]].u--; // decay useful bits
  }

  // update
  if (provider >= 0) {
    auto& e = T[provider][idx[provider]];
    if (pred != altpred) {
      if (pred == taken) { if (e.u < 3) e.u++; }
      else if (e.u > 0) e.u--;
    }
    if (taken) { if (e.ctr < 3) e.ctr++; }
    else if (e.ctr > -4) e.ctr--;
  } else {
    if (taken) { if (base[bi] < 3) base[bi]++; }
    else if (base[bi] > 0) base[bi]--;
  }

  // periodic reset of useful bits
  if (++tick % (256 * 1024) == 0)
    for (auto& t : T) for (auto& e : t) e.u >>= 1;

  ghist.pop_back();
  ghist.insert(ghist.begin(), taken);
}
