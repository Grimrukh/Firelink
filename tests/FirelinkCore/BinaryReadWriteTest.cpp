// Round-trip tests for the binary I/O primitives. These are the foundation
// for every phase that follows — if BufferReader and BufferWriter don't
// agree on a byte-for-byte level, nothing else works.

#include <doctest/doctest.h>

#include <FirelinkCore/BinaryReadWrite.h>

#include <cstdint>

using namespace Firelink::BinaryReadWrite;

TEST_CASE("BufferWriter -> BufferReader scalar round-trip (little-endian)")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(0xDEADBEEF);
    w.Write<std::int16_t>(-1234);
    w.Write<float>(3.14159f);
    w.WritePad(4);
    w.Write<std::uint64_t>(0x0123456789ABCDEFull);

    auto buf = w.Finalize();
    REQUIRE(buf.size() == 4 + 2 + 4 + 4 + 8);

    BufferReader r(buf.data(), buf.size(), Endian::Little);
    CHECK(r.Read<std::uint32_t>() == 0xDEADBEEF);
    CHECK(r.Read<std::int16_t>() == -1234);
    CHECK(r.Read<float>() == doctest::Approx(3.14159f));
    r.AssertPad(4);
    CHECK(r.Read<std::uint64_t>() == 0x0123456789ABCDEFull);
    CHECK(r.Remaining() == 0);
}

TEST_CASE("BufferWriter -> BufferReader scalar round-trip (big-endian)")
{
    BufferWriter w(Endian::Big);
    w.Write<std::uint32_t>(0x11223344);
    w.Write<std::int16_t>(0x5566);

    auto buf = w.Finalize();
    REQUIRE(buf.size() == 6);

    // First four bytes should be literally 11 22 33 44 on disk.
    CHECK(std::to_integer<std::uint8_t>(buf[0]) == 0x11);
    CHECK(std::to_integer<std::uint8_t>(buf[1]) == 0x22);
    CHECK(std::to_integer<std::uint8_t>(buf[2]) == 0x33);
    CHECK(std::to_integer<std::uint8_t>(buf[3]) == 0x44);
    CHECK(std::to_integer<std::uint8_t>(buf[4]) == 0x55);
    CHECK(std::to_integer<std::uint8_t>(buf[5]) == 0x66);

    BufferReader r(buf.data(), buf.size(), Endian::Big);
    CHECK(r.Read<std::uint32_t>() == 0x11223344);
    CHECK(r.Read<std::int16_t>() == 0x5566);
}

TEST_CASE("BufferWriter reservation fill backfills correctly")
{
    BufferWriter w(Endian::Little);
    int dummy_scope = 0;

    // Header with a reserved offset pointing to data later in the buffer.
    w.Reserve<std::uint32_t>("_data_offset", &dummy_scope);
    w.Write<std::uint32_t>(42); // other header field

    // Fill the reservation with the current position, then write the data.
    w.FillWithPosition<std::uint32_t>("_data_offset", &dummy_scope);
    w.Write<std::uint32_t>(0xCAFEBABE);

    auto buf = w.Finalize();
    REQUIRE(buf.size() == 12);

    BufferReader r(buf.data(), buf.size());
    auto data_offset = r.Read<std::uint32_t>();
    CHECK(data_offset == 8);
    CHECK(r.Read<std::uint32_t>() == 42);
    r.Seek(data_offset);
    CHECK(r.Read<std::uint32_t>() == 0xCAFEBABE);
}

TEST_CASE("BufferWriter Finalize fails on unfilled reservation")
{
    BufferWriter w(Endian::Little);
    constexpr int scope = 0;
    w.Reserve<std::uint32_t>("_unset_offset", &scope);
    CHECK_THROWS_AS((void)w.Finalize(), BinaryWriteError);
}

TEST_CASE("BufferReader AssertValue catches mismatches")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(0x12345678);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    CHECK_THROWS_AS(r.AssertValue<std::uint32_t>(0xAABBCCDD, "test"), BinaryAssertionError);
}

TEST_CASE("BufferReader TempOffset restores cursor even on exception")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(1);
    w.Write<std::uint32_t>(2);
    w.Write<std::uint32_t>(3);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    (void)r.Read<std::uint32_t>(); // advance to pos 4

    try
    {
        auto guard = r.TempOffset(8);
        CHECK(r.Position() == 8);
        throw std::runtime_error("boom");
    }
    catch (...)
    {
    }

    CHECK(r.Position() == 4); // restored despite exception
}

TEST_CASE("BufferReader ReadCStringAt does not disturb cursor")
{
    BufferWriter w(Endian::Little);
    // Write "hello\0" at offset 0.
    constexpr char text[] = "hello";
    w.WriteRaw(text, 6);
    w.Write<std::uint32_t>(0xFEEDFACE);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    (void)r.Read<std::uint32_t>(); // advance
    auto saved_pos = r.Position();

    auto bytes = r.ReadCStringAt(0);
    REQUIRE(bytes.size() == 5);
    CHECK(std::to_integer<char>(bytes[0]) == 'h');
    CHECK(std::to_integer<char>(bytes[4]) == 'o');

    CHECK(r.Position() == saved_pos);
}

TEST_CASE("BufferReader ReadArray round-trips correctly")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint16_t>(10);
    w.Write<std::uint16_t>(20);
    w.Write<std::uint16_t>(30);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    auto arr = r.ReadArray<std::uint16_t>(3);
    REQUIRE(arr.size() == 3);
    CHECK(arr[0] == 10);
    CHECK(arr[1] == 20);
    CHECK(arr[2] == 30);
    CHECK(r.Remaining() == 0);
}

TEST_CASE("BufferReader ReadArray big-endian round-trips correctly")
{
    BufferWriter w(Endian::Big);
    w.Write<std::uint32_t>(0xAABBCCDD);
    w.Write<std::uint32_t>(0x11223344);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size(), Endian::Big);
    auto arr = r.ReadArray<std::uint32_t>(2);
    REQUIRE(arr.size() == 2);
    CHECK(arr[0] == 0xAABBCCDD);
    CHECK(arr[1] == 0x11223344);
}

TEST_CASE("BufferReader ReadRaw copies bytes and advances cursor")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0xAA);
    w.Write<std::uint8_t>(0xBB);
    w.Write<std::uint8_t>(0xCC);
    w.Write<std::uint8_t>(0xDD);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    std::uint8_t dest[4]{};
    r.ReadRaw(dest, 4);
    CHECK(dest[0] == 0xAA);
    CHECK(dest[1] == 0xBB);
    CHECK(dest[2] == 0xCC);
    CHECK(dest[3] == 0xDD);
    CHECK(r.Remaining() == 0);
}

TEST_CASE("BufferReader ReadRaw past end throws")
{
    std::byte data[2]{};
    BufferReader r(data, 2);
    std::uint8_t dest[4]{};
    CHECK_THROWS_AS(r.ReadRaw(dest, 4), BinaryReadError);
}

TEST_CASE("BufferReader Read past end throws")
{
    std::byte data[2]{};
    BufferReader r(data, 2);
    CHECK_THROWS_AS(r.Read<std::uint32_t>(), BinaryReadError);
}

TEST_CASE("BufferReader Skip advances position")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(1);
    w.Write<std::uint32_t>(2);
    w.Write<std::uint32_t>(3);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    r.Skip(4);
    CHECK(r.Position() == 4);
    CHECK(r.Read<std::uint32_t>() == 2);
}

TEST_CASE("BufferReader Skip past end throws")
{
    std::byte data[4]{};
    BufferReader r(data, 4);
    CHECK_THROWS_AS(r.Skip(8), BinaryReadError);
}

TEST_CASE("BufferReader Seek past end throws")
{
    std::byte data[4]{};
    BufferReader r(data, 4);
    CHECK_THROWS_AS(r.Seek(8), BinaryReadError);
}

TEST_CASE("BufferReader Seek to exact end is valid")
{
    std::byte data[4]{};
    BufferReader r(data, 4);
    CHECK_NOTHROW(r.Seek(4));
    CHECK(r.Position() == 4);
    CHECK(r.Remaining() == 0);
}

TEST_CASE("BufferReader Size returns buffer size")
{
    std::byte data[16]{};
    const BufferReader r(data, 16);
    CHECK(r.Size() == 16);
}

TEST_CASE("BufferReader AssertPosition succeeds when correct")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(1);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    r.AssertPosition(0);
    (void)r.Read<std::uint32_t>();
    r.AssertPosition(4);
}

TEST_CASE("BufferReader AssertPosition throws on mismatch")
{
    std::byte data[4]{};
    const BufferReader r(data, 4);
    CHECK_THROWS_AS(r.AssertPosition(2), BinaryAssertionError);
}

TEST_CASE("BufferReader SetEndian / GetEndian")
{
    std::byte data[1]{};
    BufferReader r(data, 1, Endian::Little);
    CHECK(r.GetEndian() == Endian::Little);
    r.SetEndian(Endian::Big);
    CHECK(r.GetEndian() == Endian::Big);
}

TEST_CASE("BufferReader RawAt returns correct pointer")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0x11);
    w.Write<std::uint8_t>(0x22);
    w.Write<std::uint8_t>(0x33);
    const auto buf = w.Finalize();

    const BufferReader r(buf.data(), buf.size());
    const std::byte* p = r.RawAt(1);
    CHECK(std::to_integer<std::uint8_t>(*p) == 0x22);
    // Cursor should be unaffected.
    CHECK(r.Position() == 0);
}

TEST_CASE("BufferReader RawAt past end throws")
{
    std::byte data[4]{};
    const BufferReader r(data, 4);
    CHECK_THROWS_AS((void)r.RawAt(8), BinaryReadError);
}

TEST_CASE("BufferReader RawAt at exact end is valid")
{
    std::byte data[4]{};
    const BufferReader r(data, 4);
    // Offset == size is valid (one-past-end pointer, like .end()).
    CHECK_NOTHROW((void)r.RawAt(4));
}

TEST_CASE("BufferReader AssertPad throws on non-zero byte")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0);
    w.Write<std::uint8_t>(0xFF); // not zero
    w.Write<std::uint8_t>(0);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    CHECK_THROWS_AS(r.AssertPad(3), BinaryAssertionError);
}

TEST_CASE("BufferReader AssertPad past end throws")
{
    std::byte data[2]{};
    BufferReader r(data, 2);
    CHECK_THROWS_AS(r.AssertPad(4), BinaryReadError);
}

TEST_CASE("BufferReader AssertBytes succeeds on matching data")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0xDE);
    w.Write<std::uint8_t>(0xAD);
    w.Write<std::uint8_t>(0xBE);
    w.Write<std::uint8_t>(0xEF);
    const auto buf = w.Finalize();

    constexpr std::uint8_t expected[] = {0xDE, 0xAD, 0xBE, 0xEF};
    BufferReader r(buf.data(), buf.size());
    CHECK_NOTHROW(r.AssertBytes(expected, 4, "magic"));
    CHECK(r.Position() == 4);
}

TEST_CASE("BufferReader AssertBytes throws on mismatch")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(0);
    const auto buf = w.Finalize();

    constexpr std::uint8_t expected[] = {0xFF, 0xFF, 0xFF, 0xFF};
    BufferReader r(buf.data(), buf.size());
    CHECK_THROWS_AS(r.AssertBytes(expected, 4, "magic"), BinaryAssertionError);
}

TEST_CASE("BufferReader AssertBytes past end throws")
{
    std::byte data[2]{};
    BufferReader r(data, 2);
    constexpr std::uint8_t expected[] = {0, 0, 0, 0};
    CHECK_THROWS_AS(r.AssertBytes(expected, 4, "field"), BinaryReadError);
}

TEST_CASE("BufferReader AssertValue succeeds when matching")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(42);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    CHECK_NOTHROW(r.AssertValue<std::uint32_t>(42, "answer"));
}

TEST_CASE("BufferReader ReadUTF16LEStringAt reads correctly")
{
    // Write UTF-16 LE: 'A' = 0x0041, 'B' = 0x0042, then null terminator 0x0000.
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0x41);
    w.Write<std::uint8_t>(0x00); // 'A'
    w.Write<std::uint8_t>(0x42);
    w.Write<std::uint8_t>(0x00); // 'B'
    w.Write<std::uint8_t>(0x00);
    w.Write<std::uint8_t>(0x00); // null
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    auto bytes = r.ReadUTF16LEStringAt(0);
    REQUIRE(bytes.size() == 4); // 2 chars * 2 bytes each
    CHECK(std::to_integer<std::uint8_t>(bytes[0]) == 0x41);
    CHECK(std::to_integer<std::uint8_t>(bytes[1]) == 0x00);
    CHECK(std::to_integer<std::uint8_t>(bytes[2]) == 0x42);
    CHECK(std::to_integer<std::uint8_t>(bytes[3]) == 0x00);
    // Cursor unchanged.
    CHECK(r.Position() == 0);
}

TEST_CASE("BufferReader ReadUTF16LEStringAt empty string")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint8_t>(0x00);
    w.Write<std::uint8_t>(0x00);
    const auto buf = w.Finalize();

    const BufferReader r(buf.data(), buf.size());
    const auto bytes = r.ReadUTF16LEStringAt(0);
    CHECK(bytes.empty());
}

TEST_CASE("BufferReader ReadCStringAt at end of buffer returns empty")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(0xDEADBEEF);
    const auto buf = w.Finalize();

    const BufferReader r(buf.data(), buf.size());
    // Reading past the data: offset == size, should return empty.
    const auto bytes = r.ReadCStringAt(buf.size());
    CHECK(bytes.empty());
}

TEST_CASE("BufferReader TempOffset normal scope restores position")
{
    BufferWriter w(Endian::Little);
    w.Write<std::uint32_t>(100);
    w.Write<std::uint32_t>(200);
    w.Write<std::uint32_t>(300);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    (void)r.Read<std::uint32_t>(); // pos = 4
    {
        auto guard = r.TempOffset(8);
        CHECK(r.Position() == 8);
        CHECK(r.Read<std::uint32_t>() == 300);
    }
    // Guard destroyed — position restored to 4.
    CHECK(r.Position() == 4);
    CHECK(r.Read<std::uint32_t>() == 200);
}

TEST_CASE("BufferReader endian switching mid-read")
{
    // Write one value LE, another BE, into the same buffer manually.
    BufferWriter wLE(Endian::Little);
    wLE.Write<std::uint16_t>(0x0102); // LE on disk: 02 01
    auto bufLE = wLE.Finalize();

    BufferWriter wBE(Endian::Big);
    wBE.Write<std::uint16_t>(0x0304); // BE on disk: 03 04
    auto bufBE = wBE.Finalize();

    // Concat the two buffers.
    std::vector<std::byte> combined;
    combined.insert(combined.end(), bufLE.begin(), bufLE.end());
    combined.insert(combined.end(), bufBE.begin(), bufBE.end());

    BufferReader r(combined.data(), combined.size(), Endian::Little);
    CHECK(r.Read<std::uint16_t>() == 0x0102);
    r.SetEndian(Endian::Big);
    CHECK(r.Read<std::uint16_t>() == 0x0304);
}

// =========================================================================
// BufferWriter — additional coverage
// =========================================================================

TEST_CASE("BufferWriter GetEndian returns construction endian")
{
    BufferWriter wLE(Endian::Little);
    CHECK(wLE.GetEndian() == Endian::Little);
    BufferWriter wBE(Endian::Big);
    CHECK(wBE.GetEndian() == Endian::Big);
}

TEST_CASE("BufferWriter default endian is little")
{
    const BufferWriter w;
    CHECK(w.GetEndian() == Endian::Little);
}

TEST_CASE("BufferWriter Position tracks writes")
{
    BufferWriter w;
    CHECK(w.Position() == 0);
    w.Write<std::uint32_t>(1);
    CHECK(w.Position() == 4);
    w.Write<std::uint8_t>(2);
    CHECK(w.Position() == 5);
    w.WritePad(3);
    CHECK(w.Position() == 8);
}

TEST_CASE("BufferWriter WriteArray round-trips through ReadArray")
{
    constexpr std::uint32_t values[] = {10, 20, 30, 40};

    BufferWriter w(Endian::Little);
    w.WriteArray(values, 4);
    auto buf = w.Finalize();
    REQUIRE(buf.size() == 16);

    BufferReader r(buf.data(), buf.size());
    auto arr = r.ReadArray<std::uint32_t>(4);
    CHECK(arr[0] == 10);
    CHECK(arr[1] == 20);
    CHECK(arr[2] == 30);
    CHECK(arr[3] == 40);
}

TEST_CASE("BufferWriter WriteArray big-endian round-trips")
{
    constexpr std::uint16_t values[] = {0x1122, 0x3344};

    BufferWriter w(Endian::Big);
    w.WriteArray(values, 2);
    auto buf = w.Finalize();

    // Verify raw byte order is big-endian.
    CHECK(std::to_integer<std::uint8_t>(buf[0]) == 0x11);
    CHECK(std::to_integer<std::uint8_t>(buf[1]) == 0x22);
    CHECK(std::to_integer<std::uint8_t>(buf[2]) == 0x33);
    CHECK(std::to_integer<std::uint8_t>(buf[3]) == 0x44);

    BufferReader r(buf.data(), buf.size(), Endian::Big);
    auto arr = r.ReadArray<std::uint16_t>(2);
    CHECK(arr[0] == 0x1122);
    CHECK(arr[1] == 0x3344);
}

TEST_CASE("BufferWriter WriteRaw writes exact bytes")
{
    constexpr std::uint8_t raw[] = {0x01, 0x02, 0x03};
    BufferWriter w;
    w.WriteRaw(raw, 3);
    auto buf = w.Finalize();
    REQUIRE(buf.size() == 3);
    CHECK(std::to_integer<std::uint8_t>(buf[0]) == 0x01);
    CHECK(std::to_integer<std::uint8_t>(buf[1]) == 0x02);
    CHECK(std::to_integer<std::uint8_t>(buf[2]) == 0x03);
}

TEST_CASE("BufferWriter ReserveCapacity does not change size or position")
{
    BufferWriter w;
    w.ReserveCapacity(1024);
    CHECK(w.Position() == 0);
    // Should still finalize to an empty buffer.
    const auto buf = w.Finalize();
    CHECK(buf.empty());
}

TEST_CASE("BufferWriter Reserve duplicate label throws")
{
    BufferWriter w;
    constexpr int scope = 0;
    w.Reserve<std::uint32_t>("dup", &scope);
    CHECK_THROWS_AS(w.Reserve<std::uint32_t>("dup", &scope), BinaryWriteError);
}

TEST_CASE("BufferWriter Fill with explicit value")
{
    BufferWriter w(Endian::Little);
    constexpr int scope = 0;
    w.Reserve<std::uint32_t>("_count", &scope);
    w.Write<std::uint32_t>(0xAAAAAAAA);
    w.Fill<std::uint32_t>("_count", 42, &scope);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    CHECK(r.Read<std::uint32_t>() == 42);
    CHECK(r.Read<std::uint32_t>() == 0xAAAAAAAA);
}

TEST_CASE("BufferWriter Fill unknown label throws")
{
    BufferWriter w;
    CHECK_THROWS_AS(w.Fill<std::uint32_t>("nonexistent", 0), BinaryWriteError);
}

TEST_CASE("BufferWriter Fill big-endian value")
{
    BufferWriter w(Endian::Big);
    w.Reserve<std::uint32_t>("_val");
    w.Fill<std::uint32_t>("_val", 0xAABBCCDD);
    auto buf = w.Finalize();

    // Verify raw bytes are big-endian.
    CHECK(std::to_integer<std::uint8_t>(buf[0]) == 0xAA);
    CHECK(std::to_integer<std::uint8_t>(buf[1]) == 0xBB);
    CHECK(std::to_integer<std::uint8_t>(buf[2]) == 0xCC);
    CHECK(std::to_integer<std::uint8_t>(buf[3]) == 0xDD);
}

TEST_CASE("BufferWriter HasPending and PendingCount")
{
    BufferWriter w;
    int scope = 0;
    CHECK(w.PendingCount() == 0);
    CHECK_FALSE(w.HasPending("_a", &scope));

    w.Reserve<std::uint32_t>("_a", &scope);
    CHECK(w.PendingCount() == 1);
    CHECK(w.HasPending("_a", &scope));
    CHECK_FALSE(w.HasPending("_b", &scope));

    w.Reserve<std::uint32_t>("_b", &scope);
    CHECK(w.PendingCount() == 2);

    w.Fill<std::uint32_t>("_a", 0, &scope);
    CHECK(w.PendingCount() == 1);
    CHECK_FALSE(w.HasPending("_a", &scope));
    CHECK(w.HasPending("_b", &scope));

    w.Fill<std::uint32_t>("_b", 0, &scope);
    CHECK(w.PendingCount() == 0);
}

TEST_CASE("BufferWriter PadAlign pads to boundary")
{
    BufferWriter w;
    w.Write<std::uint8_t>(0xFF); // 1 byte
    CHECK(w.Position() == 1);
    w.PadAlign(4);
    CHECK(w.Position() == 4);

    auto buf = w.Finalize();
    REQUIRE(buf.size() == 4);
    // First byte is data, next 3 are zero padding.
    CHECK(std::to_integer<std::uint8_t>(buf[0]) == 0xFF);
    CHECK(std::to_integer<std::uint8_t>(buf[1]) == 0x00);
    CHECK(std::to_integer<std::uint8_t>(buf[2]) == 0x00);
    CHECK(std::to_integer<std::uint8_t>(buf[3]) == 0x00);
}

TEST_CASE("BufferWriter PadAlign already aligned is no-op")
{
    BufferWriter w;
    w.Write<std::uint32_t>(1); // 4 bytes, already aligned to 4
    CHECK(w.Position() == 4);
    w.PadAlign(4);
    CHECK(w.Position() == 4); // no change
}

TEST_CASE("BufferWriter PadAlign to 16-byte boundary")
{
    BufferWriter w;
    w.Write<std::uint32_t>(1);
    w.Write<std::uint32_t>(2);
    w.Write<std::uint8_t>(3); // pos = 9
    w.PadAlign(16);
    CHECK(w.Position() == 16);
}

TEST_CASE("BufferWriter scoped reservations with different scopes")
{
    BufferWriter w;
    int scopeA = 0;
    int scopeB = 0;

    // Same label, different scopes — should not collide.
    w.Reserve<std::uint32_t>("_offset", &scopeA);
    w.Reserve<std::uint32_t>("_offset", &scopeB);
    CHECK(w.PendingCount() == 2);

    w.Fill<std::uint32_t>("_offset", 100, &scopeA);
    w.Fill<std::uint32_t>("_offset", 200, &scopeB);
    CHECK(w.PendingCount() == 0);

    auto buf = w.Finalize();
    BufferReader r(buf.data(), buf.size());
    CHECK(r.Read<std::uint32_t>() == 100);
    CHECK(r.Read<std::uint32_t>() == 200);
}

TEST_CASE("BufferWriter unscoped Reserve / Fill / FillWithPosition")
{
    BufferWriter w(Endian::Little);
    w.Reserve<std::uint32_t>("header_size");
    w.Write<std::uint32_t>(0xBEEF);
    w.FillWithPosition<std::uint32_t>("header_size");
    w.Write<std::uint32_t>(0xCAFE);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    auto hdr_size = r.Read<std::uint32_t>();
    CHECK(hdr_size == 8); // FillWithPosition wrote position at time of call
    CHECK(r.Read<std::uint32_t>() == 0xBEEF);
    CHECK(r.Read<std::uint32_t>() == 0xCAFE);
}

TEST_CASE("BufferWriter WritePad writes zeros")
{
    BufferWriter w;
    w.Write<std::uint8_t>(0xFF);
    w.WritePad(4);
    w.Write<std::uint8_t>(0xFF);
    const auto buf = w.Finalize();
    REQUIRE(buf.size() == 6);
    for (std::size_t i = 1; i <= 4; ++i)
        CHECK(std::to_integer<std::uint8_t>(buf[i]) == 0);
}

TEST_CASE("BufferWriter Finalize on empty writer returns empty buffer")
{
    BufferWriter w;
    const auto buf = w.Finalize();
    CHECK(buf.empty());
}

// =========================================================================
// detail::ByteSwap
// =========================================================================

TEST_CASE("ByteSwap single byte is identity")
{
    CHECK(detail::ByteSwap<std::uint8_t>(0xAB) == 0xAB);
}

TEST_CASE("ByteSwap 16-bit")
{
    CHECK(detail::ByteSwap<std::uint16_t>(0x1234) == 0x3412);
}

TEST_CASE("ByteSwap 32-bit")
{
    CHECK(detail::ByteSwap<std::uint32_t>(0x12345678) == 0x78563412);
}

TEST_CASE("ByteSwap 64-bit")
{
    CHECK(detail::ByteSwap<std::uint64_t>(0x0102030405060708ull) == 0x0807060504030201ull);
}

TEST_CASE("ByteSwap float round-trip")
{
    constexpr float original = 1.5f;
    const float swapped = detail::ByteSwap(original);
    const float restored = detail::ByteSwap(swapped);
    CHECK(restored == doctest::Approx(original));
}

TEST_CASE("ByteSwap double round-trip")
{
    constexpr double original = 3.14159265358979;
    const double swapped = detail::ByteSwap(original);
    const double restored = detail::ByteSwap(swapped);
    CHECK(restored == doctest::Approx(original));
}

// =========================================================================
// BufferReader + BufferWriter — float/double big-endian round-trip
// =========================================================================

TEST_CASE("Float big-endian round-trip through buffer")
{
    BufferWriter w(Endian::Big);
    w.Write<float>(2.71828f);
    w.Write<double>(1.41421356);
    const auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size(), Endian::Big);
    CHECK(r.Read<float>() == doctest::Approx(2.71828f));
    CHECK(r.Read<double>() == doctest::Approx(1.41421356));
}

// =========================================================================
// BufferReader + BufferWriter — bool round-trip
// =========================================================================

TEST_CASE("Bool round-trip through buffer")
{
    BufferWriter w;
    w.Write<bool>(true);
    w.Write<bool>(false);
    auto buf = w.Finalize();
    REQUIRE(buf.size() == 2);

    BufferReader r(buf.data(), buf.size());
    CHECK(r.Read<bool>() == true);
    CHECK(r.Read<bool>() == false);
}

// =========================================================================
// BufferReader ReadRaw + BufferWriter WriteRaw interop
// =========================================================================

TEST_CASE("WriteRaw / ReadRaw round-trip for arbitrary blob")
{
    const std::uint8_t blob[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    BufferWriter w;
    w.Write<std::uint32_t>(8); // length prefix
    w.WriteRaw(blob, 8);
    auto buf = w.Finalize();

    BufferReader r(buf.data(), buf.size());
    auto len = r.Read<std::uint32_t>();
    CHECK(len == 8);
    std::uint8_t out[8]{};
    r.ReadRaw(out, len);
    CHECK(std::memcmp(blob, out, 8) == 0);
}
