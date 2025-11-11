#ifndef ALIGNEDFILEREADER_H
#define ALIGNEDFILEREADER_H
#include <filesystem>

#include "AVX.h"
#include "Config.h"

namespace WideLips {
    struct AlignedFileReaderDelete;
    using AlignedFileReadResult = std::unique_ptr<char[],AlignedFileReaderDelete>;

    struct WL_INTERNAL AlignedFileReaderDelete final {
        void operator ()(void* block) const noexcept;
    };

    class WL_INTERNAL AlignedFileReader final {
    public:
        constexpr static std::uint64_t Alignment = std::alignment_of_v<Vector256>;
    public:
        ~AlignedFileReader() = delete;
    public:
        NODISCARD static AlignedFileReadResult Read(const std::filesystem::path & filePath);
    };
}

#endif //ALIGNEDFILEREADER_H
