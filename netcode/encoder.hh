#pragma once

#include <iterator>
#include <list>

#include "netcode/coding.hh"
#include "netcode/handler.hh"
#include "netcode/repair.hh"
#include "netcode/source.hh"
#include "netcode/symbol.hh"
#include "netcode/types.hh"

namespace ntc {

/*------------------------------------------------------------------------------------------------*/

class encoder
{
public:

  /// @brief Can't copy-construct an encoder.
  encoder(const encoder&) = delete;

  /// @brief Cant' copy an encoder.
  encoder& operator=(const encoder&) = delete;

  /// @brief Constructor
  template <typename Handler>
  encoder(Handler&& h, const coding& c, unsigned int code_rate, code_type type)
    : handler_ptr_{new handler_derived<Handler>{std::forward<Handler>(h)}}
    , coding_{c}
    , rate_{code_rate == 0 ? 1 : code_rate}
    , type_{type}
    , current_source_id_{0}
    , current_repair_id_{0}
    , sources_{}
    , repair_{current_repair_id_}
  {}

  /// @brief Constructor
  template <typename Handler>
  encoder(Handler&& h, const coding& c, unsigned int code_rate)
    : encoder{std::forward<Handler>(h), c, code_rate, code_type::systematic}
  {}

  /// @brief Notify the encoder that some data has been received.
  /// @return false if the data could not have been decoded, true otherwise.
  bool
  notify(const char* data)
  {
    return {};
  }

  void
  commit_symbol(symbol_base&& sym)
  {
    sources_.emplace_back(current_source_id_, std::move(sym.symbol_buffer()));
    sources_.back().write([this](std::size_t nb, const char* data){handler_ptr_->write(nb, data);});

    // Should we generate a repair?
    if ((current_source_id_ + 1) % rate_ == 0)
    {
      repair_.id() = current_repair_id_;
      repair_.write([this](std::size_t nb, const char* data)
                          {
                            handler_ptr_->write(nb, data);
                          });
    }

    current_source_id_ += 1;
    current_repair_id_ += 1;
  }

  /// @brief The number of packets which have not been acknowledged.
  std::size_t
  window_size()
  const noexcept
  {
    return sources_.size();
  }

private:

  /// @brief Delete sources which have been acknowledged.
  void
  clear_sources()
  noexcept
  {
  }

  std::unique_ptr<handler_base> handler_ptr_;

  coding coding_;
  unsigned int rate_;

  /// @brief Is the encoder systematic?
  code_type type_;

  id_type current_source_id_;
  id_type current_repair_id_;

  std::list<source> sources_;
  repair repair_;
};

/*------------------------------------------------------------------------------------------------*/

} // namespace ntc
