// DCX compression/decompression implementation for FromSoftware file formats.

#include <FirelinkCore/DCX.h>
#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/Oodle.h>

#include <zlib.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace Firelink
{
    using BinaryReadWrite::BufferReader;
    using BinaryReadWrite::BufferWriter;
    using BinaryReadWrite::Endian;

    // ========================================================================
    // DCX version info table (mirrors Python DCX_VERSION_INFO)
    // ========================================================================

    struct DCXVersionInfo
    {
        char compression_type[5]; // e.g. "DFLT", "EDGE", "KRAK", "ZSTD"
        std::uint32_t version1;
        std::uint32_t version2;
        std::int32_t  version3;    // -1 means "variable" (EDGE)
        std::int32_t  compression_level; // -1 means "variable" (ZSTD)
        std::uint32_t version5;
        std::uint32_t version6;
        std::uint32_t version7;
    };

    // Table indexed by DCXType values 4..11 (DCX_EDGE through DCX_ZSTD).
    // DCP_DFLT (3) has no version info (uses its own header).
    // DCP_EDGE (2) is not in this table either (uses DCP header).
    static constexpr DCXVersionInfo DCX_VERSIONS[] = {
        // DCX_EDGE (4)
        {"EDGE", 0x10000, 0x24, -1,   9,  0x10000, 0,         0x100100},
        // DCX_DFLT_10000_24_9 (5)
        {"DFLT", 0x10000, 0x24, 0x2C, 9,  0,       0,         0x010100},
        // DCX_DFLT_10000_44_9 (6)
        {"DFLT", 0x10000, 0x44, 0x4C, 9,  0,       0,         0x010100},
        // DCX_DFLT_11000_44_8 (7)
        {"DFLT", 0x11000, 0x44, 0x4C, 8,  0,       0,         0x010100},
        // DCX_DFLT_11000_44_9 (8)
        {"DFLT", 0x11000, 0x44, 0x4C, 9,  0,       0,         0x010100},
        // DCX_DFLT_11000_44_9_15 (9)
        {"DFLT", 0x11000, 0x44, 0x4C, 9,  0,       0xF000000, 0x010100},
        // DCX_KRAK (10)
        {"KRAK", 0x11000, 0x44, 0x4C, 6,  0,       0,         0x010100},
        // DCX_ZSTD (11)
        {"ZSTD", 0x11000, 0x44, 0x4C, -1, 0,       0,         0x010100},
    };

    static const DCXVersionInfo& GetVersionInfo(DCXType type)
    {
        const int idx = static_cast<int>(type) - static_cast<int>(DCXType::DCX_EDGE);
        if (idx < 0 || idx >= static_cast<int>(std::size(DCX_VERSIONS)))
            throw DCXError("No version info for DCXType " + std::to_string(static_cast<int>(type)));
        return DCX_VERSIONS[idx];
    }

    // ========================================================================
    // DCX header structs
    // ========================================================================

    struct DCPHeader
    {
        std::uint32_t decompressed_size;
        std::uint32_t compressed_size;
    };

    struct DCXHeader
    {
        std::uint32_t version1;
        std::uint32_t version2;
        std::uint32_t version3;
        std::uint32_t decompressed_size;
        std::uint32_t compressed_size;
        char compression_type[5];
        std::uint8_t  compression_level;
        std::uint32_t version5;
        std::uint32_t version6;
        std::uint32_t version7;
    };

    struct EdgeSubheader
    {
        std::uint32_t dca_size;
        std::uint32_t last_block_decompressed_size;
        std::uint32_t egdt_size;
        std::uint32_t chunk_count;
    };

    // ========================================================================
    // Internal: read DCP header
    // ========================================================================

    static DCPHeader ReadDCPHeader(BufferReader& r)
    {
        r.AssertBytes("DCP", 4, "DCP magic");
        r.AssertBytes("DFLT", 4, "DCP DFLT magic");
        r.AssertValue<std::uint32_t>(0x20, "DCP unk[0]");
        r.AssertValue<std::uint32_t>(0x9000000, "DCP unk[1]");
        r.AssertValue<std::uint32_t>(0, "DCP unk[2]");
        r.AssertValue<std::uint32_t>(0, "DCP unk[3]");
        r.AssertValue<std::uint32_t>(0, "DCP unk[4]");
        r.AssertValue<std::uint32_t>(0x10100, "DCP unk[5]");
        r.AssertBytes("DCS", 4, "DCS magic");

        DCPHeader h{};
        h.decompressed_size = r.Read<std::uint32_t>();
        h.compressed_size = r.Read<std::uint32_t>();
        return h;
    }

    // ========================================================================
    // Internal: read DCX header
    // ========================================================================

    static DCXHeader ReadDCXHeader(BufferReader& r)
    {
        r.AssertBytes("DCX", 4, "DCX magic");

        DCXHeader h{};
        h.version1 = r.Read<std::uint32_t>();
        r.AssertValue<std::uint32_t>(0x18, "DCX unk1");
        r.AssertValue<std::uint32_t>(0x24, "DCX unk2");
        h.version2 = r.Read<std::uint32_t>();
        h.version3 = r.Read<std::uint32_t>();
        r.AssertBytes("DCS", 4, "DCS magic");
        h.decompressed_size = r.Read<std::uint32_t>();
        h.compressed_size = r.Read<std::uint32_t>();
        r.AssertBytes("DCP", 4, "DCP magic");
        r.ReadRaw(h.compression_type, 4);
        h.compression_type[4] = '\0';
        r.AssertValue<std::uint32_t>(0x20, "DCX unk3");
        h.compression_level = static_cast<std::uint8_t>(r.Read<std::uint8_t>());
        r.Skip(3);
        h.version5 = r.Read<std::uint32_t>();
        h.version6 = r.Read<std::uint32_t>();
        r.AssertValue<std::uint32_t>(0, "DCX unk5");
        h.version7 = r.Read<std::uint32_t>();
        return h;
    }

    // ========================================================================
    // Internal: read EDGE subheader
    // ========================================================================

    static EdgeSubheader ReadEdgeSubheader(BufferReader& r)
    {
        r.AssertBytes("DCA", 4, "EDGE DCA magic");
        EdgeSubheader sh{};
        sh.dca_size = r.Read<std::uint32_t>();
        r.AssertBytes("EgdT", 4, "EDGE EgdT magic");
        r.AssertValue<std::uint32_t>(0x10100, "EDGE unk1");
        r.AssertValue<std::uint32_t>(0x24, "EDGE unk2");
        r.AssertValue<std::uint32_t>(0x10, "EDGE unk3");
        r.AssertValue<std::uint32_t>(0x10000, "EDGE unk4");
        sh.last_block_decompressed_size = r.Read<std::uint32_t>();
        sh.egdt_size = r.Read<std::uint32_t>();
        sh.chunk_count = r.Read<std::uint32_t>();
        r.AssertValue<std::uint32_t>(0x100000, "EDGE unk5");
        return sh;
    }

    // ========================================================================
    // Internal: zlib helpers
    // ========================================================================

    static Bytef* ToZlibIn(const std::byte* src)
    {
        return const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    }

    static std::vector<std::byte> ZlibInflate(
        const std::byte* src, const std::size_t src_size, const std::size_t expected_size)
    {
        std::vector<std::byte> out(expected_size);
        z_stream strm{};
        strm.next_in = ToZlibIn(src);
        strm.avail_in = static_cast<uInt>(src_size);
        strm.next_out = reinterpret_cast<Bytef*>(out.data());
        strm.avail_out = static_cast<uInt>(expected_size);

        int ret = inflateInit(&strm);
        if (ret != Z_OK)
            throw DCXError("zlib inflateInit failed: " + std::to_string(ret));

        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (ret != Z_STREAM_END)
            throw DCXError("zlib inflate failed: " + std::to_string(ret));

        out.resize(strm.total_out);
        return out;
    }

    static std::vector<std::byte> ZlibInflateUnknown(
        const std::byte* src, const std::size_t src_size)
    {
        std::vector<std::byte> out;
        out.resize(src_size * 4);

        z_stream strm{};
        strm.next_in = ToZlibIn(src);
        strm.avail_in = static_cast<uInt>(src_size);

        int ret = inflateInit(&strm);
        if (ret != Z_OK)
            throw DCXError("zlib inflateInit failed: " + std::to_string(ret));

        while (true)
        {
            strm.next_out = reinterpret_cast<Bytef*>(out.data() + strm.total_out);
            strm.avail_out = static_cast<uInt>(out.size() - strm.total_out);

            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_END) break;
            if (ret != Z_OK)
            {
                inflateEnd(&strm);
                throw DCXError("zlib inflate failed: " + std::to_string(ret));
            }
            if (strm.avail_out == 0)
                out.resize(out.size() * 2);
        }
        inflateEnd(&strm);
        out.resize(strm.total_out);
        return out;
    }

    static std::vector<std::byte> RawInflate(
        const std::byte* src, const std::size_t src_size, const std::size_t expected_size)
    {
        std::vector<std::byte> out(expected_size);
        z_stream strm{};
        strm.next_in = ToZlibIn(src);
        strm.avail_in = static_cast<uInt>(src_size);
        strm.next_out = reinterpret_cast<Bytef*>(out.data());
        strm.avail_out = static_cast<uInt>(expected_size);

        int ret = inflateInit2(&strm, -MAX_WBITS);
        if (ret != Z_OK)
            throw DCXError("zlib inflateInit2 (raw) failed: " + std::to_string(ret));

        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);

        if (ret != Z_STREAM_END && ret != Z_OK)
            throw DCXError("zlib raw inflate failed: " + std::to_string(ret));

        out.resize(strm.total_out);
        return out;
    }

    // ========================================================================
    // Internal: zlib deflate helpers
    // ========================================================================

    static std::vector<std::byte> ZlibDeflate(
        const std::byte* src, const std::size_t src_size, const int level = 7)
    {
        const uLong bound = compressBound(static_cast<uLong>(src_size));
        std::vector<std::byte> out(bound);

        z_stream strm{};
        strm.next_in = ToZlibIn(src);
        strm.avail_in = static_cast<uInt>(src_size);
        strm.next_out = reinterpret_cast<Bytef*>(out.data());
        strm.avail_out = static_cast<uInt>(bound);

        int ret = deflateInit(&strm, level);
        if (ret != Z_OK)
            throw DCXError("zlib deflateInit failed: " + std::to_string(ret));

        ret = deflate(&strm, Z_FINISH);
        deflateEnd(&strm);

        if (ret != Z_STREAM_END)
            throw DCXError("zlib deflate failed: " + std::to_string(ret));

        out.resize(strm.total_out);
        return out;
    }

    static std::vector<std::byte> RawDeflate(
        const std::byte* src, const std::size_t src_size, const int level = 9)
    {
        uLong bound = deflateBound(nullptr, static_cast<uLong>(src_size));
        if (bound == 0) bound = compressBound(static_cast<uLong>(src_size));
        std::vector<std::byte> out(bound + 64);

        z_stream strm{};
        strm.next_in = ToZlibIn(src);
        strm.avail_in = static_cast<uInt>(src_size);
        strm.next_out = reinterpret_cast<Bytef*>(out.data());
        strm.avail_out = static_cast<uInt>(out.size());

        int ret = deflateInit2(&strm, level, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        if (ret != Z_OK)
            throw DCXError("zlib deflateInit2 (raw) failed: " + std::to_string(ret));

        ret = deflate(&strm, Z_FINISH);
        deflateEnd(&strm);

        if (ret != Z_STREAM_END)
            throw DCXError("zlib raw deflate failed: " + std::to_string(ret));

        out.resize(strm.total_out);
        return out;
    }

    // ========================================================================
    // Internal: ZSTD helpers
    // ========================================================================

    static std::vector<std::byte> ZstdDecompress(
        const std::byte* src, const std::size_t src_size, const std::size_t expected_size)
    {
        std::vector<std::byte> out(expected_size);
        const std::size_t result = ZSTD_decompress(out.data(), expected_size, src, src_size);
        if (ZSTD_isError(result))
            throw DCXError(std::string("ZSTD decompression failed: ") + ZSTD_getErrorName(result));
        out.resize(result);
        return out;
    }

    static std::vector<std::byte> ZstdCompress(
        const std::byte* src, const std::size_t src_size, const int level = 15)
    {
        const std::size_t bound = ZSTD_compressBound(src_size);
        std::vector<std::byte> out(bound);

        ZSTD_CCtx* cctx = ZSTD_createCCtx();
        if (!cctx) throw DCXError("Failed to create ZSTD compression context");

        ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, level);
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, 16);
        ZSTD_CCtx_setParameter(cctx, ZSTD_c_contentSizeFlag, 0);

        const std::size_t result = ZSTD_compress2(cctx, out.data(), bound, src, src_size);
        ZSTD_freeCCtx(cctx);

        if (ZSTD_isError(result))
            throw DCXError(std::string("ZSTD compression failed: ") + ZSTD_getErrorName(result));
        out.resize(result);
        return out;
    }

    // ========================================================================
    // Internal: EDGE decompression
    // ========================================================================

    static std::vector<std::byte> DecompressEdge(
        BufferReader& r, const std::uint32_t decompressed_size, const std::uint32_t version3)
    {
        const std::size_t dca_start = r.Position();
        const EdgeSubheader sh = ReadEdgeSubheader(r);

        if (version3 != 0x50 + sh.chunk_count * 0x10)
            throw DCXError("DCX_EDGE version3 mismatch");

        std::uint32_t expected_last = decompressed_size % 0x10000;
        if (expected_last == 0) expected_last = 0x10000;
        if (sh.last_block_decompressed_size != expected_last && sh.last_block_decompressed_size != 0x10000)
            throw DCXError("DCX_EDGE last_block_decompressed_size mismatch");

        if (sh.egdt_size != 0x24 + sh.chunk_count * 0x10)
            throw DCXError("DCX_EDGE egdt_size mismatch");

        const std::size_t chunks_offset = dca_start + sh.dca_size;

        std::vector<std::byte> decompressed;
        decompressed.reserve(decompressed_size);

        for (std::uint32_t i = 0; i < sh.chunk_count; ++i)
        {
            const std::uint32_t zero = r.Read<std::uint32_t>();
            const std::uint32_t offset = r.Read<std::uint32_t>();
            const std::uint32_t chunk_size = r.Read<std::uint32_t>();
            const std::uint32_t is_compressed = r.Read<std::uint32_t>();

            if (zero != 0)
                throw DCXError("DCX_EDGE chunk zero field is not 0");
            if (is_compressed > 1)
                throw DCXError("DCX_EDGE chunk is_compressed field is not 0 or 1");

            const std::byte* chunk_data = r.RawAt(chunks_offset + offset);
            const std::uint32_t expected_decompressed =
                (i < sh.chunk_count - 1) ? 0x10000u : sh.last_block_decompressed_size;

            if (is_compressed)
            {
                auto chunk = RawInflate(chunk_data, chunk_size, expected_decompressed);
                if (chunk.size() < expected_decompressed)
                    chunk.resize(expected_decompressed, std::byte{0});
                decompressed.insert(decompressed.end(), chunk.begin(), chunk.end());
            }
            else
            {
                decompressed.insert(decompressed.end(), chunk_data, chunk_data + chunk_size);
            }
        }

        return decompressed;
    }

    // ========================================================================
    // Internal: EDGE compression
    // ========================================================================

    static std::vector<std::byte> CompressEdge(
        const std::byte* data, const std::size_t size)
    {
        BufferWriter w(Endian::Big);

        std::uint32_t chunk_count = static_cast<std::uint32_t>(size / 0x10000);
        std::uint32_t last_block_size = static_cast<std::uint32_t>(size % 0x10000);
        if (last_block_size > 0)
            chunk_count += 1;
        else
            last_block_size = 0x10000;

        const auto& vi = GetVersionInfo(DCXType::DCX_EDGE);

        // --- DCX Header ---
        w.WriteRaw("DCX", 4);
        w.Write<std::uint32_t>(vi.version1);
        w.Write<std::uint32_t>(0x18);
        w.Write<std::uint32_t>(0x24);
        w.Write<std::uint32_t>(vi.version2);
        w.Write<std::uint32_t>(0x50 + chunk_count * 0x10);
        w.WriteRaw("DCS", 4);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(size));
        w.Reserve<std::uint32_t>("compressed_size");
        w.WriteRaw("DCP", 4);
        w.WriteRaw(vi.compression_type, 4);
        w.Write<std::uint32_t>(0x20);
        w.Write<std::uint8_t>(static_cast<std::uint8_t>(vi.compression_level));
        w.WritePad(3);
        w.Write<std::uint32_t>(vi.version5);
        w.Write<std::uint32_t>(vi.version6);
        w.Write<std::uint32_t>(0);
        w.Write<std::uint32_t>(vi.version7);

        // --- DCA + EgdT subheader ---
        const std::size_t dca_start = w.Position();
        w.WriteRaw("DCA", 4);
        w.Reserve<std::uint32_t>("dca_size");
        const std::size_t egdt_start = w.Position();
        w.WriteRaw("EgdT", 4);
        w.Write<std::uint32_t>(0x10100);
        w.Write<std::uint32_t>(0x24);
        w.Write<std::uint32_t>(0x10);
        w.Write<std::uint32_t>(0x10000);
        w.Write<std::uint32_t>(last_block_size);
        w.Reserve<std::uint32_t>("egdt_size");
        w.Write<std::uint32_t>(chunk_count);
        w.Write<std::uint32_t>(0x100000);

        // --- Chunk table ---
        for (std::uint32_t i = 0; i < chunk_count; ++i)
        {
            w.Write<std::uint32_t>(0);
            w.Reserve<std::uint32_t>("chunk_offset_" + std::to_string(i));
            w.Reserve<std::uint32_t>("chunk_size_" + std::to_string(i));
            w.Reserve<std::uint32_t>("chunk_compressed_" + std::to_string(i));
        }

        w.Fill<std::uint32_t>("dca_size",
            static_cast<std::uint32_t>(w.Position() - dca_start));
        w.Fill<std::uint32_t>("egdt_size",
            static_cast<std::uint32_t>(w.Position() - egdt_start));

        // --- Chunk data ---
        const std::size_t data_start = w.Position();
        std::uint32_t total_compressed = 0;

        for (std::uint32_t i = 0; i < chunk_count; ++i)
        {
            const std::uint32_t raw_offset = i * 0x10000;
            const std::uint32_t chunk_raw_size = (i < chunk_count - 1) ? 0x10000u : last_block_size;
            auto compressed = RawDeflate(data + raw_offset, chunk_raw_size, 9);

            const bool is_smaller = compressed.size() < chunk_raw_size;

            w.Fill<std::uint32_t>("chunk_offset_" + std::to_string(i),
                static_cast<std::uint32_t>(w.Position() - data_start));
            w.Fill<std::uint32_t>("chunk_size_" + std::to_string(i),
                static_cast<std::uint32_t>(is_smaller ? compressed.size() : chunk_raw_size));
            w.Fill<std::uint32_t>("chunk_compressed_" + std::to_string(i),
                is_smaller ? 1u : 0u);

            if (is_smaller)
            {
                total_compressed += static_cast<std::uint32_t>(compressed.size());
                w.WriteRaw(compressed.data(), compressed.size());
            }
            else
            {
                total_compressed += chunk_raw_size;
                w.WriteRaw(data + raw_offset, chunk_raw_size);
            }
            w.PadAlign(16);
        }

        w.Fill<std::uint32_t>("compressed_size", total_compressed);
        return w.Finalize();
    }

    // ========================================================================
    // Helper: write a DCX_DFLT-style or DCX_ZSTD-style wrapper
    // ========================================================================

    static std::vector<std::byte> WriteDCXWrapper(
        const DCXVersionInfo& vi, const std::byte* payload, const std::size_t payload_size,
        const std::size_t decompressed_size, const std::uint8_t compression_level_override = 0)
    {
        BufferWriter w(Endian::Big);
        w.WriteRaw("DCX", 4);
        w.Write<std::uint32_t>(vi.version1);
        w.Write<std::uint32_t>(0x18);
        w.Write<std::uint32_t>(0x24);
        w.Write<std::uint32_t>(vi.version2);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(vi.version3));
        w.WriteRaw("DCS", 4);
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(decompressed_size));
        w.Write<std::uint32_t>(static_cast<std::uint32_t>(payload_size));
        w.WriteRaw("DCP", 4);
        w.WriteRaw(vi.compression_type, 4);
        w.Write<std::uint32_t>(0x20);
        w.Write<std::uint8_t>(compression_level_override
            ? compression_level_override
            : static_cast<std::uint8_t>(vi.compression_level));
        w.WritePad(3);
        w.Write<std::uint32_t>(vi.version5);
        w.Write<std::uint32_t>(vi.version6);
        w.Write<std::uint32_t>(0);
        w.Write<std::uint32_t>(vi.version7);
        w.WriteRaw("DCA", 4);
        w.Write<std::uint32_t>(8);
        w.WriteRaw(payload, payload_size);
        return w.Finalize();
    }

    // ========================================================================
    // IsDCX
    // ========================================================================

    bool IsDCX(const std::byte* data, const std::size_t size) noexcept
    {
        if (size < 4) return false;
        if (std::memcmp(data, "DCX", 4) == 0) return true;
        if (std::memcmp(data, "DCP", 4) == 0) return true;
        const auto b0 = static_cast<std::uint8_t>(data[0]);
        const auto b1 = static_cast<std::uint8_t>(data[1]);
        if (b0 == 0x78 && (b1 == 0x01 || b1 == 0x5E || b1 == 0x9C || b1 == 0xDA))
            return true;
        return false;
    }

    // ========================================================================
    // DetectDCX
    // ========================================================================

    DCXType DetectDCX(const std::byte* data, const std::size_t size)
    {
        if (size < 4) return DCXType::Unknown;

        if (std::memcmp(data, "DCP", 4) == 0)
        {
            if (size < 8) return DCXType::Unknown;
            if (std::memcmp(data + 4, "DFLT", 4) == 0) return DCXType::DCP_DFLT;
            if (std::memcmp(data + 4, "EDGE", 4) == 0) return DCXType::DCP_EDGE;
            return DCXType::Unknown;
        }

        if (std::memcmp(data, "DCX", 4) != 0)
        {
            const auto b0 = static_cast<std::uint8_t>(data[0]);
            const auto b1 = static_cast<std::uint8_t>(data[1]);
            if (b0 == 0x78 && (b1 == 0x01 || b1 == 0x5E || b1 == 0x9C || b1 == 0xDA))
                return DCXType::Zlib;
            return DCXType::Unknown;
        }

        if (size < 68) return DCXType::Unknown;

        BufferReader r(data, size, Endian::Big);
        try
        {
            const DCXHeader h = ReadDCXHeader(r);

            for (int i = 0; i < static_cast<int>(std::size(DCX_VERSIONS)); ++i)
            {
                const auto& vi = DCX_VERSIONS[i];
                if (std::strncmp(h.compression_type, vi.compression_type, 4) != 0) continue;
                if (h.version1 != vi.version1) continue;
                if (h.version2 != vi.version2) continue;
                if (vi.version3 >= 0 && h.version3 != static_cast<std::uint32_t>(vi.version3)) continue;
                if (vi.compression_level >= 0 && h.compression_level != static_cast<std::uint8_t>(vi.compression_level)) continue;
                if (h.version5 != vi.version5) continue;
                if (h.version6 != vi.version6) continue;
                if (h.version7 != vi.version7) continue;

                return static_cast<DCXType>(
                    static_cast<int>(DCXType::DCX_EDGE) + i);
            }
        }
        catch (...) {}
        return DCXType::Unknown;
    }

    // ========================================================================
    // DecompressDCX
    // ========================================================================

    DCXResult DecompressDCX(const std::byte* data, const std::size_t size)
    {
        const DCXType type = DetectDCX(data, size);

        if (type == DCXType::Unknown)
            throw DCXError("Unknown or unsupported DCX type. Cannot decompress.");
        if (type == DCXType::DCX_KRAK)
        {
            BufferReader kr(data, size, Endian::Big);
            const DCXHeader kh = ReadDCXHeader(kr);
            kr.AssertBytes("DCA", 4, "DCA magic");
            kr.AssertValue<std::int32_t>(8, "DCA compressed header size");
            const std::byte* compressed = kr.RawAt(kr.Position());
            auto decompressed = Oodle::Decompress(compressed, kh.compressed_size, kh.decompressed_size);
            if (decompressed.size() != kh.decompressed_size)
                throw DCXError("Oodle decompressed size mismatch");
            return {std::move(decompressed), type};
        }

        if (type == DCXType::Zlib)
            return {ZlibInflateUnknown(data, size), type};

        BufferReader r(data, size, Endian::Big);

        if (type == DCXType::DCP_DFLT)
        {
            const DCPHeader h = ReadDCPHeader(r);
            r.AssertBytes("DCA", 4, "DCA magic");
            r.AssertValue<std::int32_t>(8, "DCA compressed header size");
            const std::byte* compressed = r.RawAt(r.Position());
            return {ZlibInflate(compressed, h.compressed_size, h.decompressed_size), type};
        }

        if (type == DCXType::DCP_EDGE)
            throw DCXError("DCP_EDGE decompression is not yet supported.");

        const DCXHeader h = ReadDCXHeader(r);

        if (type == DCXType::DCX_EDGE)
            return {DecompressEdge(r, h.decompressed_size, h.version3), type};

        r.AssertBytes("DCA", 4, "DCA magic");
        r.AssertValue<std::int32_t>(8, "DCA compressed header size");
        const std::byte* compressed = r.RawAt(r.Position());

        if (type == DCXType::DCX_ZSTD)
        {
            auto decompressed = ZstdDecompress(compressed, h.compressed_size, h.decompressed_size);
            if (decompressed.size() != h.decompressed_size)
                throw DCXError("ZSTD decompressed size mismatch");
            return {std::move(decompressed), type};
        }

        auto decompressed = ZlibInflate(compressed, h.compressed_size, h.decompressed_size);
        if (decompressed.size() != h.decompressed_size)
            throw DCXError("DCX_DFLT decompressed size mismatch");
        return {std::move(decompressed), type};
    }

    // ========================================================================
    // CompressDCX
    // ========================================================================

    std::vector<std::byte> CompressDCX(
        const std::byte* data, const std::size_t size, const DCXType type)
    {
        if (type == DCXType::Unknown || type == DCXType::Null)
            throw DCXError("Cannot compress with DCXType Unknown or Null.");
        if (type == DCXType::DCX_KRAK)
        {
            const auto compressed = Oodle::Compress(data, size);
            return WriteDCXWrapper(GetVersionInfo(type), compressed.data(), compressed.size(), size);
        }
        if (type == DCXType::DCP_EDGE)
            throw DCXError("DCP_EDGE compression is not supported.");

        if (type == DCXType::Zlib)
            return ZlibDeflate(data, size, 7);

        if (type == DCXType::DCX_EDGE)
            return CompressEdge(data, size);

        if (type == DCXType::DCX_ZSTD)
        {
            const auto compressed = ZstdCompress(data, size);
            return WriteDCXWrapper(GetVersionInfo(type), compressed.data(), compressed.size(), size, 15);
        }

        if (type == DCXType::DCP_DFLT)
        {
            const auto compressed = ZlibDeflate(data, size, 7);
            BufferWriter w(Endian::Big);
            w.WriteRaw("DCP", 4);
            w.WriteRaw("DFLT", 4);
            w.Write<std::uint32_t>(0x20);
            w.Write<std::uint32_t>(0x9000000);
            w.Write<std::uint32_t>(0);
            w.Write<std::uint32_t>(0);
            w.Write<std::uint32_t>(0);
            w.Write<std::uint32_t>(0x10100);
            w.WriteRaw("DCS", 4);
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(size));
            w.Write<std::uint32_t>(static_cast<std::uint32_t>(compressed.size()));
            w.WriteRaw("DCA", 4);
            w.Write<std::uint32_t>(8);
            w.WriteRaw(compressed.data(), compressed.size());
            return w.Finalize();
        }

        // All DCX_DFLT_* variants
        {
            const auto compressed = ZlibDeflate(data, size, 7);
            return WriteDCXWrapper(GetVersionInfo(type), compressed.data(), compressed.size(), size);
        }
    }

    std::pair<BufferReader, DCXType> GetBufferReaderForDCX(
        const std::byte* data, const size_t size, const Endian endian)
    {
        if (IsDCX(data, size))
        {
            auto [decompressedStorage, type] = DecompressDCX(data, size);
            DCXType dcxType = type;
            BufferReader reader(std::move(decompressedStorage), endian);
            return {std::move(reader), dcxType};
        }

        // Not compressed. Caller must maintain lifetime of `data`.
        return {BufferReader(data, size, endian), DCXType::Null};
    }

    std::pair<BufferReader, DCXType> GetBufferReaderForDCX(
        std::vector<std::byte>&& storage, const Endian endian)
    {
        if (IsDCX(storage.data(), storage.size()))
        {
            auto [decompressedStorage, type] = DecompressDCX(storage.data(), storage.size());
            DCXType dcxType = type;
            BufferReader reader(std::move(decompressedStorage), endian);
            return {std::move(reader), dcxType};
        }

        // Not compressed.
        return {BufferReader(std::move(storage), endian), DCXType::Null};
    }

    std::pair<BufferReader, DCXType> GetBufferReaderForDCX(
        const std::filesystem::path& path, const Endian endian)
    {
        // Read entire file into memory.
        std::ifstream stream(path, std::ios::binary | std::ios::ate);
        if (!stream)
            throw BinaryReadWrite::BinaryReadError("Could not open file: " + path.string());
        const auto fileSize = static_cast<size_t>(stream.tellg());
        stream.seekg(0);
        std::vector<std::byte> storage(fileSize);
        stream.read(reinterpret_cast<char*>(storage.data()), static_cast<std::streamsize>(fileSize));
        stream.close();

        return GetBufferReaderForDCX(std::move(storage), endian);
    }
} // namespace Firelink
