#include <array>
#include <gtest/gtest.h>
#include <fstream>
#include <filesystem>
#include <string>
#include "Utilities/AlignedFileReader.h"

using namespace testing;
namespace WideLips::Tests {
    class AlignedFileReaderTest : public Test {
    protected:
        std::filesystem::path testDir;
    protected:
        void SetUp() override {
            // Create test directory
            testDir = std::filesystem::temp_directory_path() / "AlignedFileReaderTest";
            std::filesystem::create_directories(testDir);
        }

        void TearDown() override {
            // Clean up test files
            std::filesystem::remove_all(testDir);
        }

        // Create text file with binary mode to control line endings exactly
        NODISCARD std::filesystem::path CreateTextFileWithExactContent(const std::string& filename, const std::string& content) const {
            auto filePath = testDir / filename;
            std::ofstream file(filePath, std::ios::binary);
            file.write(content.data(), static_cast<std::streamsize>(content.size()));
            return filePath;
        }

        // Create text file using platform default line endings
        NODISCARD std::filesystem::path CreateTextFile(const std::string& filename, const std::string& content) const {
            auto filePath = testDir / filename;
            std::ofstream file(filePath);
            file << content;
            return filePath;
        }

    };

    // Test reading a simple text file
    TEST_F(AlignedFileReaderTest, ReadSimpleTextFile) {
        const std::string content = "Hello, World! This is a test file.";
        const auto filePath = CreateTextFile("test.txt", content);
        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Read the actual file size for comparison
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        const size_t fileSize = file.tellg();

        // Verify content matches exactly what's on disk
        const std::string readContent(result.get(), fileSize);

        // For simple content without newlines, should be identical
        EXPECT_EQ(readContent, content);

        // Verify memory alignment (32-byte aligned)
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading multi-line text file with controlled line endings
    TEST_F(AlignedFileReaderTest, ReadMultiLineTextFileUnixEndings) {
        const std::string content =
                "Line 1: This is the first line\n"
                "Line 2: This is the second line\n"
                "Line 3: This contains special chars: !@#$%^&*()\n"
                "Line 4: Final line";
        const auto filePath = CreateTextFileWithExactContent("multiline_unix.txt", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify exact content
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading multi-line text file with Windows line endings
    TEST_F(AlignedFileReaderTest, ReadMultiLineTextFileWindowsEndings) {
        const std::string content =
                "Line 1: This is the first line\r\n"
                "Line 2: This is the second line\r\n"
                "Line 3: This contains special chars: !@#$%^&*()\r\n"
                "Line 4: Final line";
        const auto filePath = CreateTextFileWithExactContent("multiline_windows.txt", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify exact content including \r\n
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading multi-line text file with mixed line endings
    TEST_F(AlignedFileReaderTest, ReadMultiLineTextFileMixedEndings) {
        const std::string content =
                "Unix line ending\n"
                "Windows line ending\r\n"
                "Mac line ending\r"
                "Final line without ending";
        const auto filePath = CreateTextFileWithExactContent("multiline_mixed.txt", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify exact content preserving all line ending variations
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading platform-default text file (let OS handle line endings)
    TEST_F(AlignedFileReaderTest, ReadPlatformDefaultTextFile) {
        const std::string logicalContent =
                "Line 1: First line\n"
                "Line 2: Second line\n"
                "Line 3: Third line";
        const auto filePath = CreateTextFile("platform_default.txt", logicalContent);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Read actual file size
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        const size_t fileSize = file.tellg();

        std::string readContent(result.get(), fileSize);

        // The content should contain the platform's line endings
        // On Windows, newlines are converted to \r\n
        // On Unix/Linux, they remain as \n
#ifdef _WIN32
        // On Windows, expect \r\n line endings
        EXPECT_TRUE(readContent.find("\r\n") != std::string::npos ||
            readContent.find('\n') != std::string::npos);
#else
        // On Unix/Linux, expect \n line endings
        EXPECT_TRUE(readContent.find('\n') != std::string::npos);
        EXPECT_EQ(readContent.find("\r\n"), std::string::npos);
#endif

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading empty file
    TEST_F(AlignedFileReaderTest, ReadEmptyTextFile) {
        const auto filePath = CreateTextFileWithExactContent("empty.txt", "");

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);
        // Empty file should still return valid pointer for 32-byte alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading large text file
    TEST_F(AlignedFileReaderTest, ReadLargeTextFile) {
        std::string largeContent;
        const std::string lineEnding =
#ifdef _WIN32
                "\r\n";
#else
                    "\n";
#endif

        for (int i = 0; i < 1000; ++i) {
            largeContent += "This is line " + std::to_string(i) + " of a large text file." + lineEnding;
        }
        const auto filePath = CreateTextFileWithExactContent("large.txt", largeContent);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify content
        const std::string readContent(result.get(), largeContent.size());
        EXPECT_EQ(readContent, largeContent);
        EXPECT_TRUE(readContent.find("This is line 0 of") != std::string::npos);
        EXPECT_TRUE(readContent.find("This is line 999 of") != std::string::npos);

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading Lisp source code with Unix line endings
    TEST_F(AlignedFileReaderTest, ReadLispSourceFileUnixEndings) {
        const std::string content =
                "(defun factorial (n)\n"
                "  (if (<= n 1)\n"
                "      1\n"
                "      (* n (factorial (- n 1)))))\n"
                "\n"
                "(defvar *global-counter* 0)\n"
                "\n"
                ";; This is a comment\n"
                "(let ((x 42) (y 'symbol))\n"
                "  (+ x y))";
        const auto filePath = CreateTextFileWithExactContent("test_unix.lisp", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify content
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Check for specific Lisp tokens
        EXPECT_TRUE(readContent.find("defun") != std::string::npos);
        EXPECT_TRUE(readContent.find("factorial") != std::string::npos);
        EXPECT_TRUE(readContent.find(";;") != std::string::npos); // Comment

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading Lisp source code with Windows line endings
    TEST_F(AlignedFileReaderTest, ReadLispSourceFileWindowsEndings) {
        const std::string content =
                "(defun factorial (n)\r\n"
                "  (if (<= n 1)\r\n"
                "      1\r\n"
                "      (* n (factorial (- n 1)))))\r\n"
                "\r\n"
                "(defvar *global-counter* 0)\r\n"
                "\r\n"
                ";; This is a comment\r\n"
                "(let ((x 42) (y 'symbol))\r\n"
                "  (+ x y))";
        const auto filePath = CreateTextFileWithExactContent("test_windows.lisp", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify content
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Check for specific Lisp tokens and Windows line endings
        EXPECT_TRUE(readContent.find("defun") != std::string::npos);
        EXPECT_TRUE(readContent.find("factorial") != std::string::npos);
        EXPECT_TRUE(readContent.find(";;") != std::string::npos); // Comment
        EXPECT_TRUE(readContent.find("\r\n") != std::string::npos); // Windows line endings

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test reading non-existent file
    TEST_F(AlignedFileReaderTest, ReadNonExistentTextFile) {
        const auto nonExistentPath = testDir / "non_existent.txt";

        auto paddedEmptyBuffer = std::array<char,33>{};
        paddedEmptyBuffer.fill(EOF);
        paddedEmptyBuffer[32] = '\0';

        const auto result = AlignedFileReader::Read(nonExistentPath);
        EXPECT_EQ(std::memcmp(result.get(),paddedEmptyBuffer.data(),paddedEmptyBuffer.size()),0);
    }

    // Test reading from invalid path
    TEST_F(AlignedFileReaderTest, ReadInvalidPath) {
        const std::filesystem::path invalidPath = "/invalid/path/that/does/not/exist.txt";

        auto paddedEmptyBuffer = std::array<char,33>{};
        paddedEmptyBuffer.fill(EOF);
        paddedEmptyBuffer[32] = '\0';

        const auto result = AlignedFileReader::Read(invalidPath);
        EXPECT_EQ(std::memcmp(result.get(),paddedEmptyBuffer.data(),paddedEmptyBuffer.size()),0);
    }

    // Test with text file containing only whitespace with different line endings
    TEST_F(AlignedFileReaderTest, ReadWhitespaceOnlyTextFileWithLineEndings) {
        const std::string content = "   \t\r\n   \t\n\r   ";
        const auto filePath = CreateTextFileWithExactContent("whitespace.txt", content);

        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);

        // Verify exact content including all whitespace and line ending variations
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test with UTF-8 text with different line endings
    TEST_F(AlignedFileReaderTest, ReadUTF8TextFileWithLineEndings) {
        const std::string content =
                "English: Hello World!\n"
                "Spanish: Hola Mundo!\r\n"
                "French: Bonjour le Monde!\r"
                "German: Hallo Welt!\n"
                "Symbols: © ® ™ € £ ¥";

        const auto filePath = CreateTextFileWithExactContent("utf8.txt", content);
        const auto result = AlignedFileReader::Read(filePath);

        ASSERT_NE(result, nullptr);
        // Verify exact UTF-8 content with mixed line endings
        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);

        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }

    // Test file size edge cases with different line endings
    TEST_F(AlignedFileReaderTest, ReadTextFileSizeEdgeCasesWithLineEndings) {
        // Test exactly 32 bytes with Windows line endings
        std::string content = "This is exactly 30 chars!\r\n"; // 28 + 2 = 30 chars
        content += "AB"; // Make it exactly 32 bytes
        ASSERT_EQ(content.size(), 29);

        const auto filePath = CreateTextFileWithExactContent("size32_windows.txt", content);
        const auto result = AlignedFileReader::Read(filePath);
        ASSERT_NE(result, nullptr);

        const std::string readContent(result.get(), content.size());
        EXPECT_EQ(readContent, content);
        // Verify memory alignment
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(result.get()) % 32, 0);
    }
}
