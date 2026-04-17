#include <FirelinkCore/DDS.h>
#include <FirelinkCore/Logging.h>

#include <mutex>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <combaseapi.h>
#include <wincodec.h>      // GUID_ContainerFormatPng
#include <d3d11.h>
#include <wrl/client.h>    // Microsoft::WRL::ComPtr
#endif

#include <DirectXTex.h>

namespace Firelink
{
    namespace
    {
#ifdef _WIN32
        /// Initialise COM once for the lifetime of the process.
        /// SaveToWICMemory (PNG output) and D3D11 device creation require COM.
        void EnsureCOMInitialised()
        {
            static std::once_flag flag;
            std::call_once(flag, []
            {
                const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
                if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
                    throw std::runtime_error("ConvertDDS: CoInitializeEx failed");
            });
        }

        /// Lazily create and cache a D3D11 device for GPU-accelerated
        /// BC6H/BC7 compression via DirectCompute.  Returns nullptr if
        /// no suitable GPU is available (caller falls back to CPU).
        ID3D11Device* GetD3D11Device()
        {
            static Microsoft::WRL::ComPtr<ID3D11Device> device;
            static std::once_flag flag;
            std::call_once(flag, []
            {
                static constexpr D3D_FEATURE_LEVEL featureLevels[] = {
                    D3D_FEATURE_LEVEL_11_0,
                    D3D_FEATURE_LEVEL_10_1,
                    D3D_FEATURE_LEVEL_10_0,
                };
                const HRESULT hr = D3D11CreateDevice(
                    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                    featureLevels, static_cast<UINT>(std::size(featureLevels)),
                    D3D11_SDK_VERSION,
                    device.GetAddressOf(), nullptr, nullptr);
                if (FAILED(hr))
                    device.Reset();   // no GPU — will fall back to CPU
            });
            return device.Get();
        }
#endif

        /// Format an HRESULT as a hex string for error messages.
        std::string HResultToString(HRESULT hr)
        {
            std::ostringstream ss;
            ss << "0x" << std::hex << static_cast<unsigned int>(hr);
            return ss.str();
        }

        /// Load DDS from memory, decompress/convert to B8G8R8A8_UNORM.
        /// Returns only the base mip level (mip 0, array index 0, slice 0).
        DirectX::ScratchImage LoadAndDecompressDDS(const std::byte* data, size_t size)
        {
#ifdef _WIN32
            EnsureCOMInitialised();
#endif
            using namespace DirectX;

            // --- Load the DDS from the raw byte buffer ---
            TexMetadata metadata{};
            ScratchImage ddsImage;
            HRESULT hr = LoadFromDDSMemory(
                reinterpret_cast<const uint8_t*>(data), size,
                DDS_FLAGS_NONE, &metadata, ddsImage);
            if (FAILED(hr))
                throw std::runtime_error(
                    "ConvertDDS: LoadFromDDSMemory failed (" + HResultToString(hr) + ")");

            constexpr DXGI_FORMAT targetFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

            // Grab the base mip image (mip 0, item 0, slice 0).
            const Image* baseImage = ddsImage.GetImage(0, 0, 0);
            if (!baseImage)
                throw std::runtime_error("ConvertDDS: failed to get base mip image");

            // --- Decompress block-compressed formats (BC1–BC7) ---
            if (IsCompressed(metadata.format))
            {
                ScratchImage decompressed;
                hr = Decompress(*baseImage, DXGI_FORMAT_UNKNOWN, decompressed);
                if (FAILED(hr))
                    throw std::runtime_error(
                        "ConvertDDS: Decompress failed (" + HResultToString(hr) + ")");

                // If the decompressed format still isn't our target, convert.
                if (decompressed.GetMetadata().format != targetFormat)
                {
                    const Image* decompImg = decompressed.GetImage(0, 0, 0);
                    if (!decompImg)
                        throw std::runtime_error("ConvertDDS: failed to get decompressed image");

                    ScratchImage converted;
                    hr = Convert(*decompImg, targetFormat,
                                 TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted);
                    if (FAILED(hr))
                        throw std::runtime_error(
                            "ConvertDDS: Convert failed (" + HResultToString(hr) + ")");
                    return converted;
                }
                return decompressed;
            }

            // --- Non-compressed but wrong format — convert directly ---
            if (metadata.format != targetFormat)
            {
                ScratchImage converted;
                hr = Convert(*baseImage, targetFormat,
                             TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, converted);
                if (FAILED(hr))
                    throw std::runtime_error(
                        "ConvertDDS: Convert failed (" + HResultToString(hr) + ")");
                return converted;
            }

            // Already B8G8R8A8_UNORM — return as-is (move the whole ScratchImage).
            return ddsImage;
        }

        /// Copy a DirectXTex Blob into a std::vector<std::byte>.
        std::vector<std::byte> BlobToVector(const DirectX::Blob& blob)
        {
            const auto* p = reinterpret_cast<const std::byte*>(blob.GetBufferPointer());
            return std::vector<std::byte>(p, p + blob.GetBufferSize());
        }

        /// Compress or convert an uncompressed image to the target format and
        /// save as an in-memory DDS blob.  Uses GPU-accelerated DirectCompute
        /// for BC6H/BC7 when available, falling back to CPU + OpenMP.
        std::vector<std::byte> CompressAndSaveDDS(
            DirectX::ScratchImage& image, DXGI_FORMAT targetFormat)
        {
            using namespace DirectX;

            const Image* img = image.GetImage(0, 0, 0);
            if (!img)
                throw std::runtime_error("CompressAndSaveDDS: failed to get source image");

            ScratchImage result;
            const DXGI_FORMAT srcFormat = image.GetMetadata().format;

            if (srcFormat != targetFormat)
            {
                if (IsCompressed(targetFormat))
                {
                    HRESULT hr = E_FAIL;

#ifdef _WIN32
                    // BC6H / BC7 benefit enormously from GPU compression.
                    const bool isBC6or7 =
                        targetFormat == DXGI_FORMAT_BC6H_TYPELESS ||
                        targetFormat == DXGI_FORMAT_BC6H_UF16 ||
                        targetFormat == DXGI_FORMAT_BC6H_SF16 ||
                        targetFormat == DXGI_FORMAT_BC7_TYPELESS ||
                        targetFormat == DXGI_FORMAT_BC7_UNORM ||
                        targetFormat == DXGI_FORMAT_BC7_UNORM_SRGB;

                    if (isBC6or7)
                    {
                        if (ID3D11Device* dev = GetD3D11Device())
                        {
                            hr = Compress(dev, *img, targetFormat,
                                          TEX_COMPRESS_DEFAULT, 1.0f, result);
                        }
                    }
#endif
                    // CPU fallback (or non-BC6H/BC7 formats like BC1–BC5).
                    if (FAILED(hr))
                    {
                        hr = Compress(*img, targetFormat,
                                      TEX_COMPRESS_PARALLEL, TEX_THRESHOLD_DEFAULT, result);
                    }

                    if (FAILED(hr))
                        throw std::runtime_error(
                            "CompressAndSaveDDS: Compress failed (" + HResultToString(hr) + ")");
                }
                else
                {
                    const HRESULT hr = Convert(
                        *img, targetFormat,
                        TEX_FILTER_DEFAULT, TEX_THRESHOLD_DEFAULT, result);
                    if (FAILED(hr))
                        throw std::runtime_error(
                            "CompressAndSaveDDS: Convert failed (" + HResultToString(hr) + ")");
                }

                img = result.GetImage(0, 0, 0);
                if (!img)
                    throw std::runtime_error(
                        "CompressAndSaveDDS: failed to get converted image");
            }

            Blob blob;
            const HRESULT hr = SaveToDDSMemory(*img, DDS_FLAGS_NONE, blob);
            if (FAILED(hr))
                throw std::runtime_error(
                    "CompressAndSaveDDS: SaveToDDSMemory failed (" + HResultToString(hr) + ")");

            return BlobToVector(blob);
        }
    } // anonymous namespace

    std::vector<std::byte> ConvertDDSToTGA(const std::byte* data, size_t size)
    {
        using namespace DirectX;

        ScratchImage image = LoadAndDecompressDDS(data, size);

        const Image* img = image.GetImage(0, 0, 0);
        if (!img)
            throw std::runtime_error("ConvertDDSToTGA: failed to get image for save");

        Blob blob;
        const HRESULT hr = SaveToTGAMemory(*img, TGA_FLAGS_NONE, blob);
        if (FAILED(hr))
            throw std::runtime_error(
                "ConvertDDSToTGA: SaveToTGAMemory failed (" + HResultToString(hr) + ")");

        return BlobToVector(blob);
    }

    std::vector<std::byte> ConvertDDSToPNG(const std::byte* data, size_t size)
    {
        using namespace DirectX;

        ScratchImage image = LoadAndDecompressDDS(data, size);

        const Image* img = image.GetImage(0, 0, 0);
        if (!img)
            throw std::runtime_error("ConvertDDSToPNG: failed to get image for save");

        Blob blob;
        const HRESULT hr = SaveToWICMemory(*img, WIC_FLAGS_NONE,
                                           GUID_ContainerFormatPng, blob);
        if (FAILED(hr))
            throw std::runtime_error(
                "ConvertDDSToPNG: SaveToWICMemory failed (" + HResultToString(hr) + ")");

        return BlobToVector(blob);
    }

    // --- Image -> DDS -----------------------------------------------------------

    std::vector<std::byte> ConvertTGAToDDS(
        const std::byte* data, size_t size, DXGI_FORMAT targetFormat)
    {
        using namespace DirectX;

        TexMetadata metadata{};
        ScratchImage image;
        HRESULT hr = LoadFromTGAMemory(
            reinterpret_cast<const uint8_t*>(data), size,
            TGA_FLAGS_NONE, &metadata, image);
        if (FAILED(hr))
            throw std::runtime_error(
                "ConvertTGAToDDS: LoadFromTGAMemory failed (" + HResultToString(hr) + ")");

        return CompressAndSaveDDS(image, targetFormat);
    }

    std::vector<std::byte> ConvertPNGToDDS(
        const std::byte* data, size_t size, DXGI_FORMAT targetFormat)
    {
#ifdef _WIN32
        EnsureCOMInitialised();
#endif
        using namespace DirectX;

        TexMetadata metadata{};
        ScratchImage image;
        HRESULT hr = LoadFromWICMemory(
            reinterpret_cast<const uint8_t*>(data), size,
            WIC_FLAGS_NONE, &metadata, image);
        if (FAILED(hr))
            throw std::runtime_error(
                "ConvertPNGToDDS: LoadFromWICMemory failed (" + HResultToString(hr) + ")");

        return CompressAndSaveDDS(image, targetFormat);
    }
} // namespace Firelink
