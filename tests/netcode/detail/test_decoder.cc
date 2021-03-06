#include <algorithm> // equal

#include <catch.hpp>
#include "tests/netcode/launch.hh"
#include "tests/netcode/common.hh"

#include "netcode/detail/decoder.hh"
#include "netcode/detail/encoder.hh"
#include "netcode/detail/source_list.hh"

/*------------------------------------------------------------------------------------------------*/

using namespace ntc;

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: reconstruct a source from a repair")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};

    // Push the source.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});

    // A repair to store encoded sources
    detail::encoder_repair r0{0 /* id */};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    const auto s0 = decoder.create_source_from_repair(mk_decoder_repair(r0));
    REQUIRE(s0.symbol_size() == s0_data.size());
    REQUIRE(std::equal(s0.symbol(), s0.symbol() + s0.symbol_size(), s0_data.begin()));
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: remove a source from a repair")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};
    detail::byte_buffer s1_data{'e','f','g','h','i','j','k','l'};

    // Push 2 sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});

    // A repair to store encoded sources
    detail::encoder_repair r0{0 /* id */};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    SECTION("Remove s0, we should be able to reconstruct s1")
    {
      detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};
      const detail::decoder_source s0{0, s0_data, s0_data.size()};
      auto dr0 = mk_decoder_repair(r0);
      decoder.remove_source_from_repair(s0, dr0);
      REQUIRE(dr0.source_ids().size() == 1);
      REQUIRE(*(dr0.source_ids().begin()) == 1);

      const auto s1 = decoder.create_source_from_repair(std::move(dr0));
      REQUIRE(s1.symbol_size() == s1_data.size());
      REQUIRE(std::equal(s1.symbol(), s1.symbol() + s1.symbol_size(), s1_data.begin()));
    }

    SECTION("Remove s1, we should be able to reconstruct s0")
    {
      detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};
      const detail::decoder_source s1{1, s1_data, s1_data.size()};
      auto dr0 = mk_decoder_repair(r0);
      decoder.remove_source_from_repair(s1, dr0);
      REQUIRE(dr0.source_ids().size() == 1);
      REQUIRE(*(dr0.source_ids().begin()) == 0);

      const auto s0 = decoder.create_source_from_repair(std::move(dr0));
      REQUIRE(s0.symbol_size() == s0_data.size());
      REQUIRE(std::equal(s0.symbol(), s0.symbol() + s0.symbol_size(), s0_data.begin()));
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: useless repair")
{
  launch([](std::uint8_t gf_size)
  {
    // Push 5 sources.
    detail::source_list sl;
    sl.emplace(0, detail::byte_buffer{});
    sl.emplace(1, detail::byte_buffer{});
    sl.emplace(2, detail::byte_buffer{});
    sl.emplace(3, detail::byte_buffer{});
    sl.emplace(4, detail::byte_buffer{});

    // A repair to store encoded sources
    detail::encoder_repair r0{0 /* id */};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};
    decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{1, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{2, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{3, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{4, detail::byte_buffer{}, 0});
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 5);
    REQUIRE(decoder.missing_sources().empty());
    REQUIRE(decoder.repairs().size() == 0);
    REQUIRE(decoder.nb_useless_repairs() == 1);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: missing sources")
{
  launch([](std::uint8_t gf_size)
  {
    // Push 5 sources.
    detail::source_list sl;
    sl.emplace(0, detail::byte_buffer{});
    sl.emplace(1, detail::byte_buffer{});
    sl.emplace(2, detail::byte_buffer{});
    sl.emplace(3, detail::byte_buffer{});
    sl.emplace(4, detail::byte_buffer{});

    // A repair to store encoded sources
    detail::encoder_repair r0{0 /* id */};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};
    decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{2, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{4, detail::byte_buffer{}, 0});
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 3);
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.repairs().size() == 1);
    REQUIRE(decoder.nb_useless_repairs() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: drop outdated sources")
{
  launch([](std::uint8_t gf_size)
  {
    // We need an encoder to fill repairs.
    detail::encoder encoder{gf_size};

    // The decoder to test
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // Send some sources to the decoder.
    decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
    decoder(detail::decoder_source{1, detail::byte_buffer{}, 0});
    REQUIRE(decoder.sources().size() == 2);

    // Now create a repair that acknowledges the 2 first sources.
    detail::source_list sl;
    sl.emplace(2, detail::byte_buffer{});
    sl.emplace(3, detail::byte_buffer{});
    sl.emplace(4, detail::byte_buffer{});
    detail::encoder_repair r0{0};
    encoder(r0, sl);

    SECTION("sources lost")
    {
      // Send repair.
      decoder(mk_decoder_repair(r0));

      // Now test the decoder.
      REQUIRE(decoder.sources().size() == 0);
      REQUIRE(decoder.missing_sources().size() == 3);
      REQUIRE(decoder.missing_sources().count(2));
      REQUIRE(decoder.missing_sources().count(3));
      REQUIRE(decoder.missing_sources().count(4));
      REQUIRE(decoder.repairs().size() == 1);
      REQUIRE(decoder.nb_useless_repairs() == 0);
    }

    SECTION("sources received")
    {
      // Send sources
      decoder(detail::decoder_source{2, detail::byte_buffer{}, 0});
      decoder(detail::decoder_source{3, detail::byte_buffer{}, 0});
      decoder(detail::decoder_source{4, detail::byte_buffer{}, 0});

      // Send repair.
      decoder(mk_decoder_repair(r0));

      // Now test the decoder.
      REQUIRE(decoder.sources().size() == 3);
      REQUIRE(decoder.sources().count(2));
      REQUIRE(decoder.sources().count(3));
      REQUIRE(decoder.sources().count(4));
      REQUIRE(decoder.missing_sources().size() == 0);
      REQUIRE(decoder.repairs().size() == 0);
      REQUIRE(decoder.nb_useless_repairs() == 1);
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: drop outdated lost sources")
{
  launch([](std::uint8_t gf_size)
  {
    // We need an encoder to fill repairs.
    detail::encoder encoder{gf_size};

    // The decoder to test
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // A repair with the first 2 sources.
    detail::source_list sl0;
    sl0.emplace(0, detail::byte_buffer{});
    sl0.emplace(1, detail::byte_buffer{});
    detail::encoder_repair r0{0};
    encoder(r0, sl0);

    // First 2 sources are lost.
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.missing_sources().count(0));
    REQUIRE(decoder.missing_sources().count(1));
    REQUIRE(decoder.repairs().count(0));
    REQUIRE(decoder.repairs().find(0)->second.source_ids().size() > 0);

    // Now create a repair that drops the 2 first sources due to a limited window size.
    detail::source_list sl1;
    sl1.emplace(2, detail::byte_buffer{});
    sl1.emplace(3, detail::byte_buffer{});
    detail::encoder_repair r1{1};
    encoder(r1, sl1);

    SECTION("sources lost")
    {
      // Send repair.
      decoder(mk_decoder_repair(r1));

      // Now test the decoder.
      REQUIRE(decoder.sources().empty());
      // s2 and s3 are missing.
      REQUIRE(decoder.missing_sources().size() == 2);
      REQUIRE(decoder.missing_sources().count(2));
      REQUIRE(decoder.missing_sources().count(3));
      // r0 should have been dropped.
      REQUIRE(decoder.repairs().size() == 1);
      REQUIRE(decoder.nb_useless_repairs() == 0);
    }

    SECTION("sources received")
    {
      // Send sources
      decoder(detail::decoder_source{2, detail::byte_buffer{}, 0});
      decoder(detail::decoder_source{3, detail::byte_buffer{}, 0});

      // Send repair.
      decoder(mk_decoder_repair(r1));

      // Now test the decoder.
      REQUIRE(decoder.sources().size() == 2);
      REQUIRE(decoder.sources().count(2));
      REQUIRE(decoder.sources().count(3));
      REQUIRE(decoder.missing_sources().empty());
      REQUIRE(decoder.nb_useless_repairs() == 1);
      // r0 should have been dropped and r1 is useless
      REQUIRE(decoder.repairs().size() == 0);
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: one source lost encoded in one received repair")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};

    // Push the source.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});

    // A repair to store encoded sources
    detail::encoder_repair r0{0 /* id */};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{ gf_size, [&](const detail::decoder_source& s0)
                                      {
                                        REQUIRE(s0.id() == 0);
                                        REQUIRE(s0.symbol_size() == s0_data.size());
                                        REQUIRE(std::equal( s0.symbol()
                                                          , s0.symbol() + s0.symbol_size()
                                                          , s0_data.begin()));
                                     }
                           , in_order::no};
    decoder(mk_decoder_repair(r0));
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: 2 lost sources from 2 repairs")
{
  launch([](std::uint8_t gf_size)
  {
    detail::encoder encoder{gf_size};
    detail::decoder decoder{gf_size, [&](const detail::decoder_source&){}, in_order::no};

    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};
    detail::byte_buffer s1_data{'e','f','g','h','i','j','k','l'};

    // Push 2 sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});

    // 2 repairs to store encoded sources
    detail::encoder_repair r0{0 /* id */};
    detail::encoder_repair r1{1 /* id */};
    encoder(r0, sl);
    encoder(r1, sl);

    // Send first repair.
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().empty());
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.missing_sources().count(0));
    REQUIRE(decoder.missing_sources().count(1));
    REQUIRE(decoder.repairs().size() == 1);
    REQUIRE(decoder.repairs().count(0));

    // Send second repair, full decoding should take place.
    decoder(mk_decoder_repair(r1));
    REQUIRE(decoder.nb_failed_full_decodings() == 0);
    REQUIRE(decoder.sources().size() == 2);
    REQUIRE(decoder.sources().count(0));
    REQUIRE(decoder.sources().count(1));
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);

    // Now, check contents.
    REQUIRE(decoder.sources().find(0)->second.symbol_size() == s0_data.size());
    REQUIRE(std::equal( s0_data.begin(), s0_data.end()
                      , decoder.sources().find(0)->second.symbol()));
    REQUIRE(decoder.sources().find(1)->second.symbol_size() == s1_data.size());
    REQUIRE(std::equal( s1_data.begin(), s1_data.end()
                      , decoder.sources().find(1)->second.symbol()));
  });
}

/*------------------------------------------------------------------------------------------------*/

// Tests might broke if coefficient generator is changed as the coefficient matrix might not be
// invertible.
TEST_CASE("Decoder: several lost sources from several repairs")
{
  launch([](std::uint8_t gf_size)
  {
    detail::encoder encoder{gf_size};
    detail::decoder decoder{gf_size, [&](const detail::decoder_source&){}, in_order::no};

    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','a','a','a'};
    detail::byte_buffer s1_data{'b','b','b','b','b','b','b','b'};
    detail::byte_buffer s2_data{'c','c','c','c','c','c','c','c','c','c','c','c'};
    detail::byte_buffer s3_data{'d','d','d','d'};
    detail::byte_buffer s4_data{'e','e','e','e','e','e','e','e'};

    // Push 2 sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});

    // 2 repairs to store encoded sources
    detail::encoder_repair r0{0};
    detail::encoder_repair r1{1};
    encoder(r0, sl);
    encoder(r1, sl);

    // Send first repair.
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().empty());
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.missing_sources().count(0));
    REQUIRE(decoder.missing_sources().count(1));
    REQUIRE(decoder.repairs().size() == 1);
    REQUIRE(decoder.repairs().count(0));

    // Send second repair, full decoding should take place.
    decoder(mk_decoder_repair(r1));
    REQUIRE(decoder.sources().size() == 2);
    REQUIRE(decoder.sources().count(0));
    REQUIRE(decoder.sources().count(1));
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);

    // Now, check contents.
    REQUIRE(decoder.sources().find(0)->second.symbol_size() == s0_data.size());
    REQUIRE(std::equal( s0_data.begin(), s0_data.end()
                      , decoder.sources().find(0)->second.symbol()));
    REQUIRE(decoder.sources().find(1)->second.symbol_size() == s1_data.size());
    REQUIRE(std::equal( s1_data.begin(), s1_data.end()
                      , decoder.sources().find(1)->second.symbol()));

    SECTION("Sources are not outdated")
    {
      auto nb_failed_full_decodings = 0ul;

      // More repairs.
      detail::encoder_repair r2{2};
      detail::encoder_repair r3{3};
      detail::encoder_repair r4{4};

      // Push 3 new sources.
      add_source(sl, 2, detail::byte_buffer{s2_data});
      add_source(sl, 3, detail::byte_buffer{s3_data});
      encoder(r2, sl);
      add_source(sl, 4, detail::byte_buffer{s4_data});
      encoder(r3, sl);
      encoder(r4, sl);

      // Send 1 more repair, there should not be any decoding.
      decoder(mk_decoder_repair(r2));
      REQUIRE(decoder.sources().size() == 2);
      REQUIRE(decoder.sources().count(0));
      REQUIRE(decoder.sources().count(1));
      REQUIRE(decoder.missing_sources().size() == 2);
      REQUIRE(decoder.repairs().size() == 1);

      // Send 1 more repair, there should not be any decoding.
      decoder(mk_decoder_repair(r3));
      REQUIRE(decoder.sources().size() == 2);
      REQUIRE(decoder.sources().count(0));
      REQUIRE(decoder.sources().count(1));
      REQUIRE(decoder.missing_sources().size() == 3);
      REQUIRE(decoder.repairs().size() == 2);

      // Send 1 more repair, full decoding could take place.
      decoder(mk_decoder_repair(r4));
      if (decoder.nb_failed_full_decodings() != nb_failed_full_decodings)
      {
        // Previous decoding attempt failed.
        REQUIRE(decoder.repairs().size() == 2);

        ++nb_failed_full_decodings;
        // Try with a new repair.
        detail::encoder_repair r5{5};
        encoder(r5, sl);
        decoder(mk_decoder_repair(r5));
        if (decoder.nb_failed_full_decodings() != nb_failed_full_decodings)
        {
          REQUIRE(false); // Failure to decode, again ?!!
        }

        REQUIRE(decoder.sources().size() == 5);
        REQUIRE(decoder.sources().count(0));
        REQUIRE(decoder.sources().count(1));
        REQUIRE(decoder.sources().count(2));
        REQUIRE(decoder.sources().count(3));
        REQUIRE(decoder.sources().count(4));
        REQUIRE(decoder.missing_sources().size() == 0);
        REQUIRE(decoder.repairs().size() == 0);


        // Check contents.
        REQUIRE(decoder.sources().find(2)->second.symbol_size() == s2_data.size());
        REQUIRE(std::equal( s2_data.begin(), s2_data.end()
                          , decoder.sources().find(2)->second.symbol()));
        REQUIRE(decoder.sources().find(3)->second.symbol_size() == s3_data.size());
        REQUIRE(std::equal( s3_data.begin(), s3_data.end()
                          , decoder.sources().find(3)->second.symbol()));
        REQUIRE(decoder.sources().find(4)->second.symbol_size() == s4_data.size());
        REQUIRE(std::equal( s4_data.begin(), s4_data.end()
                          , decoder.sources().find(4)->second.symbol()));      }
    }

    SECTION("Sources are outdated")
    {
      auto nb_failed_full_decodings = 0ul;

      // More repairs.
      detail::encoder_repair r2{2};
      detail::encoder_repair r3{3};
      detail::encoder_repair r4{4};

      // Ack: remove the 2 previously sent sources, thus they won't be encoded in following repairs.
      sl.pop_front();
      sl.pop_front();

      // Push 3 new sources.
      add_source(sl, 2, detail::byte_buffer{s2_data});
      add_source(sl, 3, detail::byte_buffer{s3_data});
      encoder(r2, sl);
      add_source(sl, 4, detail::byte_buffer{s4_data});
      encoder(r3, sl);
      encoder(r4, sl);

      // Send 1 more repair, there should not be any decoding.
      decoder(mk_decoder_repair(r2));
      REQUIRE(decoder.sources().size() == 0);
      REQUIRE(decoder.missing_sources().size() == 2);
      REQUIRE(decoder.repairs().size() == 1);

      // Send 1 more repair, there should not be any decoding.
      decoder(mk_decoder_repair(r3));
      REQUIRE(decoder.sources().size() == 0);
      REQUIRE(decoder.missing_sources().size() == 3);
      REQUIRE(decoder.repairs().size() == 2);

      // Send 1 more repair, full decoding could take place.
      decoder(mk_decoder_repair(r4));
      if (decoder.nb_failed_full_decodings() != nb_failed_full_decodings)
      {
        // Previous decoding attempt failed.
        REQUIRE(decoder.repairs().size() == 2);

        ++nb_failed_full_decodings;
        // Try with a new repair.
        detail::encoder_repair r5{5};
        encoder(r5, sl);
        decoder(mk_decoder_repair(r5));
        if (decoder.nb_failed_full_decodings() != nb_failed_full_decodings)
        {
          REQUIRE(false); // Failure to decode, again ?!!
        }
      }

      REQUIRE(decoder.sources().size() == 3);
      REQUIRE(decoder.sources().count(2));
      REQUIRE(decoder.sources().count(3));
      REQUIRE(decoder.sources().count(4));
      REQUIRE(decoder.missing_sources().size() == 0);
      REQUIRE(decoder.repairs().size() == 0);

      // Check contents.
      REQUIRE(decoder.sources().find(2)->second.symbol_size() == s2_data.size());
      REQUIRE(std::equal( s2_data.begin(), s2_data.end()
                        , decoder.sources().find(2)->second.symbol()));
      REQUIRE(decoder.sources().find(3)->second.symbol_size() == s3_data.size());
      REQUIRE(std::equal( s3_data.begin(), s3_data.end()
                        , decoder.sources().find(3)->second.symbol()));
      REQUIRE(decoder.sources().find(4)->second.symbol_size() == s4_data.size());
      REQUIRE(std::equal( s4_data.begin(), s4_data.end()
                        , decoder.sources().find(4)->second.symbol()));
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: duplicate source")
{
  launch([](std::uint8_t gf_size)
  {
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // Send source.
    decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
    REQUIRE(decoder.sources().size() == 1);
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);
    REQUIRE(decoder.nb_useless_repairs() == 0);

    // Send duplicate source.
    decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
    REQUIRE(decoder.sources().size() == 1);
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);
    REQUIRE(decoder.nb_useless_repairs() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: out-of-order source after repair")
{
  launch([](std::uint8_t gf_size)
  {
    detail::encoder encoder{gf_size};
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    detail::source_list sl;
    sl.emplace(0, detail::byte_buffer{});
    detail::encoder_repair r0{0};
    encoder(r0, sl);

    // Send repair
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 1);
    REQUIRE(decoder.sources().count(0));

    SECTION("Lost source is not outdated")
    {
      // Eventually, the missing source is received.
      decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
      REQUIRE(decoder.sources().size() == 1);
      REQUIRE(decoder.sources().count(0));
    }

    SECTION("Lost source is outdated")
    {
      // No more s0 on encoder side.
      sl.pop_front();

      // A new source along with a new repair.
      sl.emplace(1, detail::byte_buffer{});
      detail::encoder_repair r1{0};
      encoder(r1, sl);

      // Send repair
      decoder(mk_decoder_repair(r1));
      REQUIRE(decoder.sources().size() == 1);
      REQUIRE(decoder.sources().count(1));

      // Eventually, the missing source is received.
      decoder(detail::decoder_source{0, detail::byte_buffer{}, 0});
      REQUIRE(decoder.sources().size() == 1);
      REQUIRE(decoder.sources().count(1));
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: duplicate repair 1")
{
  launch([](std::uint8_t gf_size)
  {
    // We'll need two encoders as we can't copy a repair.
    detail::encoder encoder0{gf_size};
    detail::encoder encoder1{gf_size};

    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // A dummy lost source. Should be repaired immediatly.
    detail::source_list sl;
    sl.emplace(0, detail::byte_buffer{});

    // Create original repair.
    detail::encoder_repair r0{0};
    encoder0(r0, sl);

    // Create copy.
    detail::encoder_repair r0_dup{0};
    encoder1(r0_dup, sl);

    // Send original repair.
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 1);
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);
    REQUIRE(decoder.nb_useless_repairs() == 0);

    SECTION("Reconstructed source is not outdated")
    {
      // Now send duplicate. Should be seen as useless.
      decoder(mk_decoder_repair(r0_dup));
      REQUIRE(decoder.sources().size() == 1);
      REQUIRE(decoder.missing_sources().size() == 0);
      REQUIRE(decoder.repairs().size() == 0);
      REQUIRE(decoder.nb_useless_repairs() == 1);
    }

    SECTION("Reconstructed source is outdated")
    {
      sl.pop_front();
      sl.emplace(1, detail::byte_buffer{});
      detail::encoder_repair r1{0};
      encoder0(r1, sl);
      // Send repair.
      decoder(mk_decoder_repair(r1));

      // Now send duplicate.
      decoder(mk_decoder_repair(r0_dup));
      REQUIRE(decoder.sources().size() == 1);
      REQUIRE(decoder.sources().count(1));
      REQUIRE(decoder.missing_sources().size() == 0);
      REQUIRE(decoder.repairs().size() == 0);
      REQUIRE(decoder.nb_useless_repairs() == 0);
    }
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: duplicate repair 2")
{
  launch([](std::uint8_t gf_size)
  {
    // We'll need two encoders as we can't copy a repair.
    detail::encoder encoder0{gf_size};
    detail::encoder encoder1{gf_size};

    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // Some dummy lost sources.
    detail::source_list sl;
    sl.emplace(0, detail::byte_buffer{});
    sl.emplace(1, detail::byte_buffer{});

    // Create original repair.
    detail::encoder_repair r0{0};
    encoder0(r0, sl);

    // Create copy.
    detail::encoder_repair r0_dup{0};
    encoder1(r0_dup, sl);

    // Send original repair.
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 0);
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.repairs().size() == 1);
    REQUIRE(decoder.nb_useless_repairs() == 0);

    // Send duplicate.
    decoder(mk_decoder_repair(r0_dup));
    REQUIRE(decoder.sources().size() == 0);
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.repairs().size() == 1);
    REQUIRE(decoder.nb_useless_repairs() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: source after repair")
{
  launch([](std::uint8_t gf_size)
  {
    detail::encoder encoder{gf_size};
    detail::decoder decoder{gf_size, [&](const detail::decoder_source&){}, in_order::no};

    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};
    detail::byte_buffer s1_data{'e','f','g','h','i','j','k','l'};

    // Push 2 sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});

    // 2 repairs to store encoded sources
    detail::encoder_repair r0{0};
    detail::encoder_repair r1{1};
    encoder(r0, sl);
    encoder(r1, sl);

    // r0 is received before s0 and s1
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 0);
    REQUIRE(decoder.missing_sources().size() == 2);
    REQUIRE(decoder.repairs().size() == 1);

    // s0 is received
    decoder({0, detail::byte_buffer{s0_data}, s0_data.size()});
    REQUIRE(decoder.sources().size() == 2);
    REQUIRE(decoder.sources().count(0));
    REQUIRE(decoder.sources().count(1));
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: repair with only one source")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','b','c','d'};

    // Push the source.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});

    // A repair to store encoded sources
    detail::encoder_repair r0{0};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // r0 is received
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 1);
    REQUIRE(decoder.sources().count(0));
    REQUIRE(decoder.sources().find(0)->second.symbol_size() == s0_data.size());
    REQUIRE(std::equal( s0_data.begin(), s0_data.end()
                      , decoder.sources().find(0)->second.symbol()));
    REQUIRE(decoder.missing_sources().size() == 0);
    REQUIRE(decoder.repairs().size() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Decoder: 1 packet loss")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','a','a','a'};
    detail::byte_buffer s1_data{'b','b','b','b','b','b','b','b'};
    detail::byte_buffer s2_data{'c','c','c','c','c','c','c','c','c','c','c','c'};
    detail::byte_buffer s3_data{'d','d','d','d'};

    // Push the sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});
    add_source(sl, 2, detail::byte_buffer{s2_data});
    add_source(sl, 3, detail::byte_buffer{s3_data});

    // A repair to store encoded sources
    detail::encoder_repair r0{0};

    // We need an encoder to fill the repair.
    detail::encoder{gf_size}(r0, sl);

    // Now test the decoder.
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::no};

    // s1 -> s3 are received
    decoder(detail::decoder_source{1, detail::byte_buffer{s1_data}, s1_data.size()});
    decoder(detail::decoder_source{2, detail::byte_buffer{s2_data}, s2_data.size()});
    decoder(detail::decoder_source{3, detail::byte_buffer{s3_data}, s3_data.size()});
    REQUIRE(decoder.sources().size() == 3);
    REQUIRE(decoder.sources().count(1));
    REQUIRE(decoder.sources().count(2));
    REQUIRE(decoder.sources().count(3));


    // r0 is received
    decoder(mk_decoder_repair(r0));
    REQUIRE(decoder.sources().size() == 4);
    REQUIRE(decoder.sources().count(0));
    REQUIRE(decoder.sources().count(1));
    REQUIRE(decoder.sources().count(2));
    REQUIRE(decoder.sources().count(3));
    REQUIRE(decoder.repairs().size() == 0);
    REQUIRE(decoder.nb_useless_repairs() == 0);
    REQUIRE(decoder.nb_failed_full_decodings() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("2 repairs for 3 sources")
{
  launch([](std::uint8_t gf_size)
  {
    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','a','a','a'};
    detail::byte_buffer s1_data{'b','b','b','b','b','b','b','b','b','b','b','b'};
    detail::byte_buffer s2_data{'c','c','c','c'};

    // Push the sources.
    detail::source_list sl;
    add_source(sl, 0, detail::byte_buffer{s0_data});
    add_source(sl, 1, detail::byte_buffer{s1_data});
    add_source(sl, 2, detail::byte_buffer{s2_data});

    // Two repairs to store encoded sources
    detail::encoder_repair r0{0};
    detail::encoder_repair r1{1};

    // Fill the repair.
    detail::encoder{gf_size}(r0, sl);
    detail::encoder{gf_size}(r1, sl);

    // Now test the decoder.
    detail::decoder decoder{ gf_size
                           , [&](const detail::decoder_source& src)
                             {
                               if (src.id() == 0)
                               {
                                 REQUIRE(src.symbol_size() == s0_data.size());
                                 REQUIRE(std::equal(begin(s0_data), end(s0_data), src.symbol()));
                               }
                               else if (src.id() == 1)
                               {
                                 REQUIRE(src.symbol_size() == s1_data.size());
                                 REQUIRE(std::equal(begin(s1_data), end(s1_data), src.symbol()));
                               }
                               else if (src.id() == 2)
                               {
                                 REQUIRE(src.symbol_size() == s2_data.size());
                                 REQUIRE(std::equal(begin(s2_data), end(s2_data), src.symbol()));
                               }
                               else
                               {
                                 REQUIRE(false);
                               }
                             }
                           , in_order::no};

    // r0 is received
    decoder(mk_decoder_repair(r0));
    decoder(mk_decoder_repair(r1));
    REQUIRE(decoder.nb_decoded() == 0);
    REQUIRE(decoder.missing_sources().size() == 3);

    // s2 is received, s0 and s1 should be decoded.
    decoder(detail::decoder_source{2, s2_data, s2_data.size()});
    REQUIRE(decoder.nb_decoded() == 2);
    REQUIRE(decoder.missing_sources().size() == 0);
  });
}

/*------------------------------------------------------------------------------------------------*/

TEST_CASE("Outdating repair, but not reffered sources")
{
  launch([](std::uint8_t gf_size)
  {
    detail::decoder decoder{gf_size, [](const detail::decoder_source&){}, in_order::yes};

    // The payloads that should be reconstructed.
    detail::byte_buffer s0_data{'a','a','a','a'};
    detail::byte_buffer s1_data{'b','b','b','b','b','b','b','b','b','b','b','b'};
    detail::byte_buffer s2_data{'c','c','c','c'};
    detail::byte_buffer s3_data{'d','d','d','d'};
    detail::byte_buffer s4_data{'e','e','e','e','e','e','e','e',};

    // Push first set of sources.
    detail::source_list sl0;
    add_source(sl0, 0, detail::byte_buffer{s0_data});
    add_source(sl0, 1, detail::byte_buffer{s1_data});
    add_source(sl0, 2, detail::byte_buffer{s2_data});

    // Repair for first set of sources
    detail::encoder_repair r0{0};

    // Fill the repair.
    detail::encoder{gf_size}(r0, sl0);

    decoder(detail::decoder_source{0, detail::byte_buffer{s0_data}, s0_data.size()});
    decoder(mk_decoder_repair(r0));

    REQUIRE(decoder.nb_decoded() == 0);
    REQUIRE(decoder.missing_sources().size() == 2);

    // Push second set of sources ().
    detail::source_list sl1;
    add_source(sl1, 1, detail::byte_buffer{s1_data});
    add_source(sl1, 2, detail::byte_buffer{s2_data});

//    detail::encoder_repair r1{1};

  });
}

/*------------------------------------------------------------------------------------------------*/
