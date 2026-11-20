#include "gsharep.h"
#include <cstdlib>
#include "instruction.h"

void gsharep::init()
{
  if (const char* s = std::getenv("GS_LOGSIZE")) logsize = static_cast<unsigned>(std::atoi(s));
  histlen = logsize;
  if (const char* s = std::getenv("GS_HIST")) histlen = static_cast<unsigned>(std::atoi(s));
  if (histlen > 62) histlen = 62;
  table.assign(std::size_t{1} << logsize, 1);
  inited = true;
}
std::size_t gsharep::index(champsim::address ip) const
{
  auto a = ip.to<uint64_t>();
  uint64_t h = hist & ((uint64_t{1} << histlen) - 1);
  return static_cast<std::size_t>((a ^ (a >> logsize) ^ h) & ((uint64_t{1} << logsize) - 1));
}
bool gsharep::predict_branch(champsim::address ip)
{
  if (!inited) init();
  return table[index(ip)] >= 2;
}
void gsharep::last_branch_result(champsim::address ip, champsim::address, bool taken, uint8_t branch_type)
{
  if (!inited) init();
  if (branch_type == static_cast<uint8_t>(BRANCH_CONDITIONAL)) {
    auto& c = table[index(ip)];
    if (taken && c < 3) ++c;
    if (!taken && c > 0) --c;
  }
  hist = (hist << 1) | (taken ? 1u : 0u); // all branches enter the history (same as TAGE)
}
