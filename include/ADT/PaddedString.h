#ifndef WIDELIPS_PADDEDSTRING_H
#define WIDELIPS_PADDEDSTRING_H
#include <string>

namespace WideLips {
    class PaddedString {
    private:
        std::string _str;
    public:
        PaddedString(const std::string& str,const char pad,const int padCount):
        _str(std::move(str + std::string(padCount, pad))){}

        PaddedString(const std::string_view str,const char pad,const int padCount):
        _str(std::move(std::string(str) + std::string(padCount, pad))){}

        PaddedString(const PaddedString&) = default;
        PaddedString(PaddedString&&) = default;
        PaddedString& operator=(const PaddedString&) = default;
        PaddedString& operator=(PaddedString&&) = default;
    public:
        NODISCARD std::string_view GetUnderlyingString() const noexcept {
            return _str;
        }

        NODISCARD std::string GetCopyOfUnderlyingString() const noexcept {
            return _str;
        }
    };

    class EmptyPaddedString {
    private:
    public:
        EmptyPaddedString() = delete;
    public:
        NODISCARD static const PaddedString& GetPaddedString() noexcept {
            //performance wise this is not the best code since some compilers like clang will lock each static
            //variable (for first usage only) before initializing/writing the variables. However,
            //we don't care since EmptyPaddedString is barely used in our code and only at initialization of
            //the paser (so it doesn't affect any hot path)
            static std::string emptyStr{};
            static PaddedString paddedStr{emptyStr,' ',0};
            return paddedStr;
        }
    };
}
#endif //WIDELIPS_PADDEDSTRING_H