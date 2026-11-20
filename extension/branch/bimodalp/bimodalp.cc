#include "bimodalp.h"
#include <cstdlib>
#include "instruction.h"

void bimodalp::init()
{
  if (const char* s = std::getenv("BP_LOGSIZE")) logsize = static_cast<unsigned>(std::atoi(s));
  table.assign(std::size_t{1} << logsize, 1); // weakly not-taken
  inited = true;
}
std::size_t bimodalp::index(champsim::address ip) const
{
  auto a = ip.to<uint64_t>();
  return static_cast<std::size_t>((a ^ (a >> logsize)) & ((uint64_t{1} << logsize) - 1));
}
bool bimodalp::predict_branch(champsim::address ip)
{
  if (!inited) init();
  return table[index(ip)] >= 2;
}
void bimodalp::last_branch_result(champsim::address ip, champsim::address, bool taken, uint8_t branch_type)
{
  if (!inited) init();
  if (branch_type != static_cast<uint8_t>(BRANCH_CONDITIONAL)) return; // train on conditional branches only
  auto& c = table[index(ip)];
  if (taken && c < 3) ++c;
  if (!taken && c > 0) --c;
}
