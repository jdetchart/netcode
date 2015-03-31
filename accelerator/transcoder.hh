#pragma once

#include <algorithm>
#include <iostream>
#include <random>

#define ASIO_STANDALONE
#define BOOST_DATE_TIME_NO_LIB
#define ASIO_HAS_BOOST_DATE_TIME
#include <asio.hpp>
#include <netcode/decoder.hh>
#include <netcode/encoder.hh>

/*------------------------------------------------------------------------------------------------*/

using asio::ip::address_v4;
using asio::ip::udp;

/*------------------------------------------------------------------------------------------------*/

/// @brief
static constexpr auto max_len = 2048;

/*------------------------------------------------------------------------------------------------*/

static std::random_device rd;

/*------------------------------------------------------------------------------------------------*/

class random_loss
{
public:

  random_loss()
    : state_{state::good}
    , gen_{rd()}
    , dist_{1, 100}
  {}

  bool
  operator()()
  noexcept
  {
    switch (state_)
    {
        case state::good:
        {
          if (dist_(gen_) < 80)
          {
            return false; // no loss
          }
          else
          {
            state_ = state::bad;
            return true; // loss
          }
        }

        case state::bad:
        {
          if (dist_(gen_) > 90)
          {
            return true; // loss
          }
          else
          {
            state_ = state::good;
            return false; // no loss
          }
        }
    }
  }

private:

  enum class state {good, bad};
  state state_;
  std::default_random_engine gen_;
  std::uniform_int_distribution<> dist_;
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Called by encoder when a packet is ready to be written to the network.
struct packet_handler
{
  udp::socket& socket;
  udp::endpoint& endpoint;

  char buffer[max_len];
  std::size_t written;

  bool lossy;
  random_loss loss;

  packet_handler(udp::socket& sock, udp::endpoint& end, bool lossy = false)
    : socket(sock), endpoint(end), buffer(), written(0), lossy(lossy)
  {}

  void
  operator()(const char* data, std::size_t sz)
  noexcept
  {
    std::copy_n(data, sz, buffer + written);
    written += sz;
  }

  void
  operator()()
  noexcept
  {
    // End of packet, we can now send it.
    if (not lossy or not loss())
    {
      socket.send_to(asio::buffer(buffer, written), endpoint);
    }
    written = 0;
  }
};

/*------------------------------------------------------------------------------------------------*/

/// @brief Called by decoder when a data has been decoded or received.
struct data_handler
{
  udp::socket& socket;
  udp::endpoint& endpoint;

  data_handler(udp::socket& sock, udp::endpoint& end)
    : socket(sock), endpoint(end)
  {}

  void
  operator()(const char* data, std::size_t sz)
  noexcept
  {
    socket.send_to(asio::buffer(data, sz), endpoint);
  }
};

/*------------------------------------------------------------------------------------------------*/

class transcoder
{
public:

  /// @brief Constructor.
  transcoder( const ntc::configuration& conf
            , asio::io_service& io
            , udp::socket& app_socket
            , udp::endpoint& app_endpoint
            , udp::socket& socket
            , udp::endpoint& endpoint
            )
    : timer_(io, boost::posix_time::milliseconds(100))
    , stats_timer_(io, boost::posix_time::seconds(2))
    , app_socket_(app_socket)
    , app_endpoint_(app_endpoint)
    , socket_(socket)
    , endpoint_(endpoint)
    , decoder_(packet_handler(socket_, endpoint_), data_handler(app_socket_, app_endpoint_), conf)
    , encoder_(packet_handler(socket_, endpoint_, true), conf)
    , packet_(max_len)
    , data_(max_len)
    , other_side_seen_(false)
  {
    decoder_.conf().ack_frequency = std::chrono::milliseconds{0};

    start_handler();
    start_app_handler();
    start_timer_handler();
    start_stats_timer_handler();
  }

private:

  // Will receive sources and repairs from encoder.
  void
  start_handler()
  {
    socket_.async_receive_from( asio::buffer(packet_.buffer(), max_len)
                              , endpoint_
                              , [this](const asio::error_code& err, std::size_t)
                                {
                                  if (err)
                                  {
                                    throw std::runtime_error(err.message());
                                  }

                                  other_side_seen_ = true;

                                  auto res = 0ul;
                                  switch (ntc::detail::get_packet_type(packet_.buffer()))
                                  {
                                      case ntc::detail::packet_type::ack:
                                      {
                                        res = encoder_(std::move(packet_));
                                        break;
                                      }

                                      case ntc::detail::packet_type::repair:
                                      case ntc::detail::packet_type::source:
                                      {
                                        res = decoder_(std::move(packet_));
                                        break;
                                      }

                                      default:;
                                  }

                                  if (res == 0)
                                  {
                                    throw std::runtime_error("Invalid packet format");
                                  }

                                  // Prepare packet for next incoming.
                                  packet_.reset(max_len);

                                  // Listen again for incoming packets.
                                  start_handler();
                                });
  }

  void
  start_app_handler()
  {
    app_socket_.async_receive_from( asio::buffer(data_.buffer(), max_len)
                                  , app_endpoint_
                                  , [this](const asio::error_code& err, std::size_t sz)
                                    {
                                      if (err)
                                      {
                                        throw std::runtime_error(err.message());
                                      }

                                      // Give the ack to decoder.
                                      data_.used_bytes() = sz;
                                      encoder_(std::move(data_));

                                      // Prepare ack for next incoming.
                                      data_.reset(max_len);

                                      // Listen again.
                                      start_app_handler();
                                    });
  }

  void
  start_timer_handler()
  {
    timer_.expires_from_now(boost::posix_time::milliseconds(100));
    timer_.async_wait([this](const asio::error_code& err)
                      {
                        if (err)
                        {
                          throw std::runtime_error(err.message());
                        }
                        if (other_side_seen_)
                        {
                          decoder_.send_ack();
                        }
                        start_timer_handler();
                      });
  }

  void
  start_stats_timer_handler()
  {
    stats_timer_.expires_from_now(boost::posix_time::seconds(2));
    stats_timer_.async_wait([this](const asio::error_code& err)
                            {
                              if (err)
                              {
                                throw std::runtime_error(err.message());
                              }
                              std::cout << "-- Encoder --\n"
                                        << "<- acks   : " << encoder_.nb_acks() << '\n'
                                        << "-> repairs: " << encoder_.nb_repairs() << '\n'
                                        << "-> sources: " << encoder_.nb_sources() << '\n'
                                        << "window : " << encoder_.window() << '\n'
                                        ;
                              std::cout << '\n';
                              std::cout << "-- Decoder --\n"
                                        << "-> acks   : " << decoder_.nb_acks() << '\n'
                                        << "<- repairs: " << decoder_.nb_repairs() << '\n'
                                        << "<- sources: " << decoder_.nb_sources() << '\n'
                                        << "decoded: " << decoder_.nb_decoded() << '\n'
                                        ;
                              std::cout << '\n' << std::endl;

                              start_stats_timer_handler();
                            });
  }


private:

  /// @brief
  asio::deadline_timer timer_;

  /// @brief
  asio::deadline_timer stats_timer_;

  /// @brief
  udp::socket& app_socket_;

  /// @brief
  udp::endpoint& app_endpoint_;

  /// @brief
  udp::socket& socket_;

  /// @brief
  udp::endpoint& endpoint_;

  /// @brief
  ntc::decoder<packet_handler, data_handler> decoder_;

  /// @brief
  ntc::encoder<packet_handler> encoder_;

  /// @brief
  ntc::packet packet_;

  /// @brief
  ntc::data data_;

  /// @brief
  bool other_side_seen_;
};

/*------------------------------------------------------------------------------------------------*/