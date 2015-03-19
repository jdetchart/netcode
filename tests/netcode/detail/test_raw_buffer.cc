#include "tests/catch.hpp"

#include "netcode/detail/raw_buffer.hh"

/*------------------------------------------------------------------------------------------------*/

using namespace ntc;

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("A raw_buffer is aligned on 16 bytes", "[alignment][buffer]" )
{
  const auto b0 = detail::raw_buffer{};
  REQUIRE((reinterpret_cast<std::size_t>(&b0[0]) % 16ul) == 0ul);

  const auto b1 = b0;
  REQUIRE((reinterpret_cast<std::size_t>(&b1[0]) % 16ul) == 0ul);
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("A zero_raw_buffer is aligned on 16 bytes", "[alignment][buffer]" )
{
  const auto b0 = detail::zero_raw_buffer{};
  REQUIRE((reinterpret_cast<std::size_t>(&b0[0]) % 16ul) == 0ul);

  const auto b1 = b0;
  REQUIRE((reinterpret_cast<std::size_t>(&b1[0]) % 16ul) == 0ul);
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("A raw_buffer is not 0-out when resized ", "[buffer]" )
{
  auto b = detail::raw_buffer{0,1,2,3,4,5,6,7,8,9};
  REQUIRE(b.size() == 10);

  for (char i = 0; i < 10; ++i)
  {
    REQUIRE(b[i] == i);
  }

  b.resize(3);
  REQUIRE(b.size() == 3);
  REQUIRE(b[0] == 0);
  REQUIRE(b[1] == 1);
  REQUIRE(b[2] == 2);

  b.resize(10);
  REQUIRE(b.size() == 10);
  // Values didn't change in the mean time.
  for (char i = 0; i < 10; ++i)
  {
    REQUIRE(b[i] == i);
  }
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("A zero_raw_buffer is 0-out when resized ", "[buffer]" )
{
  auto b = detail::zero_raw_buffer{0,1,2,3,4,5,6,7,8,9};
  REQUIRE(b.size() == 10);

  for (char i = 0; i < 10; ++i)
  {
    REQUIRE(b[i] == i);
  }

  b.resize(3);
  REQUIRE(b.size() == 3);
  REQUIRE(b[0] == 0);
  REQUIRE(b[1] == 1);
  REQUIRE(b[2] == 2);

  b.resize(10);
  REQUIRE(b.size() == 10);
  REQUIRE(b[0] == 0);
  REQUIRE(b[1] == 1);
  REQUIRE(b[2] == 2);
  // Values didn't change in the mean time.
  for (char i = 3; i < 10; ++i)
  {
    REQUIRE(b[i] == 0);
  }
}

/*------------------------------------------------------------------------------------------------*/