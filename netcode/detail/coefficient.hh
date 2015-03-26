#pragma once

#include "netcode/detail/galois_field.hh"

namespace ntc { namespace detail {

/*------------------------------------------------------------------------------------------------*/

/// @internal
/// @brief Compute the coefficient for the repair being built.
inline
std::uint32_t
coefficient(const galois_field& gf, std::uint32_t repair_id, std::uint32_t src_id)
noexcept
{
  return ((repair_id + 1) + (src_id + 1))*(repair_id+1) % gf.size();
}

/*------------------------------------------------------------------------------------------------*/

}} // namespace ntc::detail
