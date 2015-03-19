#include <algorithm>

#include "tests/catch.hpp"

#include "netcode/detail/source_list.hh"

/*------------------------------------------------------------------------------------------------*/

using namespace ntc;

/*------------------------------------------------------------------------------------------------*/

namespace /* unnamed */{

bool
contains_id(const detail::source_list& sl, std::size_t id)
{
  return std::find_if( sl.cbegin(), sl.cend()
                     , [&](const detail::source& src){return src.id() == id;})
        != sl.cend();
}

} // namespace unnamed

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("add and remove sources in source_list", "[source_list][source]")
{
  auto sl = detail::source_list{};

  // Push some sources.
  sl.emplace(0, detail::raw_buffer{}, 0);
  sl.emplace(1, detail::raw_buffer{}, 0);
  sl.emplace(2, detail::raw_buffer{}, 0);
  sl.emplace(3, detail::raw_buffer{}, 0);

  REQUIRE(sl.size() == 4);

  SECTION("Remove all sources.")
  {
    const auto ids = detail::source_id_list{0,1,2,3};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 0);
  }

  SECTION("Remove some sources.")
  {
    const auto ids = detail::source_id_list{0,3};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 2));
  }

  SECTION("Remove some sources in two passes.")
  {
    auto ids= detail::source_id_list{0,3};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 2));

    ids = detail::source_id_list{1};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 1);
    REQUIRE(contains_id(sl, 2));
  }

  SECTION("Remove wrong sources.")
  {
    const auto ids= detail::source_id_list{0,2,9};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 3));
  }

  SECTION("Remove sources twice.")
  {
    auto ids= detail::source_id_list{0,2};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 3));

    ids= detail::source_id_list{0};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 3));
  }

  SECTION("Remove sources twice + wrong.")
  {
    auto ids= detail::source_id_list{0,2,9};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 3));

    ids= detail::source_id_list{0};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 2);
    REQUIRE(contains_id(sl, 1));
    REQUIRE(contains_id(sl, 3));

    ids= detail::source_id_list{1};
    sl.erase(begin(ids), end(ids));
    REQUIRE(sl.size() == 1);
    REQUIRE(contains_id(sl, 3));
  }
}

/*------------------------------------------------------------------------------------------------*/
