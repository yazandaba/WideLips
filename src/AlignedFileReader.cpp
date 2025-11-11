#include <fstream>
#include "AlignedFileReader.h"

namespace WideLips {
    void AlignedFileReaderDelete::operator()(void *block) const noexcept {
        operator delete [](block,std::align_val_t{AlignedFileReader::Alignment},std::nothrow);
    }

    AlignedFileReadResult AlignedFileReader::Read(const std::filesystem::path &filePath) {
        if (!exists(filePath)) {
            auto* block = static_cast<char*>(operator new[](PaddingSize + 1,
                 std::align_val_t{Alignment}, std::nothrow));
            std::memset(block, EOF, PaddingSize);
            block[PaddingSize] = '\0';
            return AlignedFileReadResult{block, AlignedFileReaderDelete{}};
        }

        std::ifstream file(filePath,std::ios::binary|std::ios::ate);
        if (!file) {
            std::puts("Failed to open file");
        }

        const std::streamoff size = file.tellg();
        file.seekg(0, std::ios::beg);

        // Handle empty files: return a valid aligned, null-terminated block
        if (size <= 0) {
            auto* block = static_cast<char*>(operator new[](PaddingSize + 1,
                std::align_val_t{Alignment}, std::nothrow));
            std::memset(block, EOF, PaddingSize);
            block[PaddingSize] = '\0';
            return AlignedFileReadResult{block, AlignedFileReaderDelete{}};
        }

        // Allocate an extra byte to allow optional null-termination without overflow
        const auto block = static_cast<char *>(operator new []((static_cast<std::size_t>(size) + PaddingSize + 1),
            std::align_val_t{Alignment},std::nothrow));
        file.read(block,size);
        std::memset(block + size, EOF, PaddingSize);
        block[size+PaddingSize] = '\0'; //for string termination if needed
        return AlignedFileReadResult{block,AlignedFileReaderDelete{}};
    }
}
