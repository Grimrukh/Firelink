#pragma once

#include <FirelinkCore/BinaryReadWrite.h>
#include <FirelinkCore/DCX.h>
#include <FirelinkCore/Endian.h>

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

namespace Firelink
{
    /// @brief Concept for a `GameFile` subclass.
    ///
    /// @details Must define `void Deserialize(BufferReader&)` and `void Serialize(BufferWriter&)`.
    template <typename T>
    concept GameFileType =
    requires(T t, BinaryReadWrite::BufferReader& r)
    {
        { t.Deserialize(r) } -> std::same_as<void>;
    } &&
    requires(const T t, BinaryReadWrite::BufferWriter& w)
    {
        { t.Serialize(w) } -> std::same_as<void>;
    } &&
    requires(const T t)
    {
        { t.GetEndian() } -> std::same_as<BinaryReadWrite::Endian>;
    } &&
    std::default_initializable<T>;

    /// @brief Base CRTP class for serializable game files,
    /// with DCX handling and basic read/write methods.
    template <typename T>  // `GameFileType` concept enforced below
    class GameFile
    {
    public:

        /// @brief Alias for a unique pointer of this GameFile subclass.
        using Ptr = std::unique_ptr<T>;

        /// @brief Alias for a unique pointer (to const) of this GameFile subclass.
        using CPtr = std::unique_ptr<const T>;

        /// @brief Default virtual destructor.
        virtual ~GameFile()
        {
            // Deferred static assertion for CRTP.
            static_assert(GameFileType<T>,
                "T must satisfy GameFileType: "
                "requires default constructor, "
                "void Deserialize(BufferReader&), "
                "void Serialize(BufferWriter&) const, "
                "and Endian GetEndian() const (all public).");
        }

        // --- READERS ---

        /// @brief Construct a new `GameFile` from raw data.
        static Ptr FromBytes(const std::byte* data, const size_t size)
        {
            Ptr instancePtr = std::make_unique<T>();
            auto [r, dcxType] = GetBufferReaderForDCX(data, size, BinaryReadWrite::Endian::Little);
            instancePtr->m_dcxType = dcxType;
            instancePtr->Deserialize(r);
            return std::move(instancePtr);
        }

        /// @brief Construct a new `GameFile` from managed storage (ownership passed to internal reader).
        static Ptr FromBytes(std::vector<std::byte>&& storage)
        {
            Ptr instancePtr = std::make_unique<T>();
            auto [r, dcxType] = GetBufferReaderForDCX(std::move(storage), BinaryReadWrite::Endian::Little);
            instancePtr->m_dcxType = dcxType;
            instancePtr->Deserialize(r);
            return std::move(instancePtr);
        }

        /// @brief Construct a new `GameFile` from a path.
        static Ptr FromPath(const std::filesystem::path& path)
        {
            Ptr instancePtr = std::make_unique<T>();
            auto [r, dcxType] = GetBufferReaderForDCX(path, BinaryReadWrite::Endian::Little);
            instancePtr->m_dcxType = dcxType;
            instancePtr->Deserialize(r);
            return std::move(instancePtr);
        }

        /// @brief Construct new `GameFile` instances from a list of managed data vectors in parallel.
        /// Ownership of data is passed to internal readers.
        /// `instanceCallback` is called on each successfully loaded instance.
        static std::vector<Ptr> FromBytesParallel(
            std::vector<std::vector<std::byte>>&& storages,
            const int maxThreads = 0,
            std::function<void(T&)> instanceCallback = nullptr)
        {
            const int count = static_cast<int>(storages.size());
            std::vector<Ptr> instances;
            instances.resize(count);
            std::vector<std::exception_ptr> errors;
            errors.resize(count);

            int hw_threads = static_cast<int>(std::thread::hardware_concurrency());
            if (hw_threads < 1) hw_threads = 4;
            const int num_threads = (maxThreads > 0)
                ? std::min(maxThreads, static_cast<int>(count))
                : std::min(hw_threads, static_cast<int>(count));

            std::atomic<std::size_t> next_index;
            auto worker = [&]
            {
                while (true)
                {
                    std::size_t idx = next_index.fetch_add(1);
                    if (idx >= count) break;
                    try
                    {
                        instances[idx] = T::FromBytes(std::move(storages[idx]));
                    }
                    catch (...)
                    {
                        errors[idx] = std::current_exception();
                    }

                    if (instanceCallback)
                        instanceCallback(*instances[idx]);
                }
            };

            std::vector<std::thread> threads;
            threads.reserve(num_threads);
            for (int t = 0; t < num_threads; ++t)
            {
                threads.emplace_back(worker);
            }
            for (auto& th : threads)
            {
                th.join();
            }

            // Check for errors from the parallel phase.
            for (std::size_t i = 0; i < count; ++i)
            {
                if (errors[i])
                {
                    try
                    {
                        std::rethrow_exception(errors[i]);
                    }
                    catch (const std::exception& e)
                    {
                        throw std::runtime_error(
                            "Error parsing GameFile at index " + std::to_string(i) + ": " + e.what());
                    }
                }
            }

            return instances;
        }

        /// @brief Construct new `GameFile` instances from a list of paths in parallel.
        /// `instanceCallback` is called on each successfully loaded instance.
        static std::vector<Ptr> FromPathsParallel(
            const std::vector<std::filesystem::path>& paths,
            const int maxThreads = 0,
            std::function<void(T&)> instanceCallback = nullptr)
        {
            const int count = static_cast<int>(paths.size());
            std::vector<Ptr> instances;
            instances.resize(count);
            std::vector<std::exception_ptr> errors;
            errors.resize(count);

            int hw_threads = static_cast<int>(std::thread::hardware_concurrency());
            if (hw_threads < 1) hw_threads = 4;
            const int num_threads = (maxThreads > 0)
                ? std::min(maxThreads, static_cast<int>(count))
                : std::min(hw_threads, static_cast<int>(count));

            std::atomic<std::size_t> next_index;
            auto worker = [&]
            {
                while (true)
                {
                    std::size_t idx = next_index.fetch_add(1);
                    if (idx >= count) break;
                    try
                    {
                        instances[idx] = T::FromPath(paths[idx]);
                    }
                    catch (...)
                    {
                        errors[idx] = std::current_exception();
                    }

                    if (instanceCallback)
                        instanceCallback(*instances[idx]);
                }
            };

            std::vector<std::thread> threads;
            threads.reserve(num_threads);
            for (int t = 0; t < num_threads; ++t)
            {
                threads.emplace_back(worker);
            }
            for (auto& th : threads)
            {
                th.join();
            }

            // Check for errors from the parallel phase.
            for (std::size_t i = 0; i < count; ++i)
            {
                if (errors[i])
                {
                    try
                    {
                        std::rethrow_exception(errors[i]);
                    }
                    catch (const std::exception& e)
                    {
                        throw std::runtime_error(
                            "Error parsing GameFile at index " + std::to_string(i) + ": " + e.what());
                    }
                }
            }

            return instances;
        }

        // --- WRITERS ---

        /// @brief Serialize to a byte buffer ready for writing to disk.
        [[nodiscard]] std::vector<std::byte> ToBytes() const
        {
            // TODO: Subclass should indicate how many writer bytes to reserve initially.
            const BinaryReadWrite::Endian endian = static_cast<const T*>(this)->GetEndian();
            BinaryReadWrite::BufferWriter w(endian);
            static_cast<const T*>(this)->Serialize(w);
            std::vector<std::byte> data = w.Finalize();

            if (m_dcxType == DCXType::Null)
                return data;

            // Compress DCX.
            return CompressDCX(data.data(), data.size(), m_dcxType);
        }

        /// @brief Serialize and write to `path`, or last known path if empty (default).
        void WriteToPath(std::filesystem::path path = std::filesystem::path{}) const
        {
            if (path.empty())
                path = m_path;

            const std::vector<std::byte> data = ToBytes();  // DCX handled
            if (m_dcxType != DCXType::Null)
            {
                // Add '.dcx' if not already present in path suffix.
                if (!path.has_filename() || !path.filename().string().ends_with(".dcx"))
                    path += ".dcx";
            }

            // Write to path.
            std::ofstream file(path, std::ios::binary);
            if (!file)
                throw std::runtime_error("Failed to open file for writing: " + path.string());
            file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            if (!file)
                throw std::runtime_error("Failed to write file: " + path.string());
        }

        // --- GETTERS / SETTERS ---

        /// @brief Get the last known/set path of the `GameFile`.
        [[nodiscard]] std::filesystem::path GetPath() const { return m_path; }

        /// @brief Set the path of the `GameFile`. Ignores any '.bak' suffix.
        void SetPath(const std::filesystem::path& path)
        {
            // Assign path. If `path` ends in '.bak', strip that first.
            if (path.has_filename() && path.filename().string().ends_with(".bak"))
                m_path = path.parent_path() / path.stem();
            else
                m_path = path;
        }

        /// @brief Get current DCXType of file instance that will be used upon write.
        [[nodiscard]] DCXType GetDCXType() const { return m_dcxType;}

        /// @brief Set DCXType of file instance to use upon write.
        void SetDCXType(const DCXType type) { m_dcxType = type; }

    protected:

        std::filesystem::path m_path;
        DCXType m_dcxType = DCXType::Null;
    };
}
