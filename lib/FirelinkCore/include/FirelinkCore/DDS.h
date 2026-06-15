#pragma once

#include <FirelinkCore/Export.h>

#include <cstddef>
#include <vector>

#ifdef _WIN32
#include <dxgiformat.h>    // DXGI_FORMAT enum
#endif

namespace Firelink
{
    /// @brief DDS container class with conversion methods and de/swizzling.
    /// @note It is up to the user to track whether the DDS data is swizzled or not,
    ///       based on the source of the data (e.g. dumped PS4 files are swizzled).
    ///       Trying to export swizzled data or view it on PC will look garbled.
    class FIRELINK_CORE_API DDS
    {
    public:

        // --- Constructors ---

        /// @brief Default (empty) constructor.
        DDS() = default;

        /// @brief Construct DDS from stored bytes (must be moved in).
        explicit DDS(std::vector<std::byte>&& data)
            : m_storage{std::move(data)}
        {
        }

        /// @brief Construct DDS from raw data and size.
        explicit DDS(const std::byte* data, size_t size);

        /// @brief Convert TGA data to DDS with the given format.
        /// @note Block-compressed formats (BC1–BC7) are supported; the source pixels are
        /// automatically compressed. Uses TEX_COMPRESS_PARALLEL internally for speed.
        [[nodiscard]] static DDS FromTGA(const std::byte* data, size_t size, DXGI_FORMAT targetFormat);
        /// @brief Convert PNG data to DDS with the given format.
        /// @note Block-compressed formats (BC1–BC7) are supported; the source pixels are
        /// automatically compressed. Uses TEX_COMPRESS_PARALLEL internally for speed.
        [[nodiscard]] static DDS FromPNG(const std::byte* data, size_t size, DXGI_FORMAT targetFormat);

        /// @brief Get a view of the DDS bytes.
        [[nodiscard]] const std::vector<std::byte>& GetBytes() const noexcept { return m_storage; }
        /// @brief Set the DDS bytes (must be moved in).
        void SetBytes(std::vector<std::byte>&& data) { m_storage = std::move(data); }

        /// @brief Raw byte array access.
        [[nodiscard]] std::byte* GetData() noexcept { return m_storage.data(); }
        /// @brief Raw byte array size.
        [[nodiscard]] size_t GetSize() const noexcept { return m_storage.size(); }
        /// @brief Check if DDS bytes are empty.
        [[nodiscard]] bool IsEmpty() const noexcept { return m_storage.empty(); }

        // --- DDS -> Image -----------------------------------------------------------

        /// @brief Convert DDS to TGA data. Will be garbled if not deswizzled.
        [[nodiscard]] std::vector<std::byte> ToTGA() const;
        /// @brief Convert DDS to PNG data. Will be garbled if not deswizzled.
        [[nodiscard]] std::vector<std::byte> ToPNG() const;

        // --- PS4 GNF swizzle --------------------------------------------------------
        // FromSoftware PS4 DDS files store pixel data in the AMD GNF macro-tile layout
        // rather than the linear row-major layout that DirectXTex and GPU upload expect.
        //
        // Both functions keep the DDS header unchanged and only replace the pixel data
        // section.  The format and dimensions needed to drive the swizzle algorithm are
        // derived automatically from the DDS header.
        //
        // DeswizzlePS4: PS4 tiled DDS  → linear DDS  (use before converting to TGA/PNG)
        // SwizzlePS4:   linear DDS     → PS4 tiled DDS  (use when writing back to TPF)

        /// @brief Construct a deswizzled copy of this swizzled PS4 DDS.
        [[nodiscard]] DDS DeswizzlePS4() const;
        /// @brief Construct a swizzled PS4 copy of this non-swizzled DDS.
        [[nodiscard]] DDS SwizzlePS4() const;

    private:
        std::vector<std::byte> m_storage{};

    };

}