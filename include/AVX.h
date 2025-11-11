#ifndef WIDEDLIPS_AVX_H
#define WIDEDLIPS_AVX_H
#include <cstdint>
#include <immintrin.h>
#include "Config.h"

#ifdef __AVX2__
namespace WideLips {

    class Avx2;
    struct Vector256;

    class WL_INTERNAL Avx2 final{
    public:
        struct Custom final {
        public:
            ~Custom() = delete;
        public:
            template<std::uint8_t shift>
            static Vector256 RightShift8(Vector256 vec) requires (shift < 8);
        };
    public:
        ~Avx2() = delete;
    public:
        static Vector256 LoadFromAddress(const std::uint8_t * address,std::ptrdiff_t offset = 0);

        static Vector256 CompareEqual(Vector256 lhs, Vector256 rhs);

        static std::uint32_t MoveMask(Vector256 vec);

        static Vector256 ShuffleBytes(Vector256 lookupTable,Vector256 vec);

        static Vector256 SubtractSaturated(Vector256 lhs,Vector256 rhs);

        static Vector256 Propagate(std::uint8_t value);

        static Vector256 Or(Vector256 lhs,Vector256 rhs);

        template<std::uint8_t shift>
        static Vector256 RightShift(Vector256 vec);
    };

    struct WL_INTERNAL Vector256 final {
    private:
        const __m256i _vec{};
    public:
        Vector256(const Vector256&) = default;
        Vector256(Vector256&&) noexcept = default;
        Vector256& operator=(const Vector256&) = delete;
        Vector256& operator=(Vector256&&) noexcept = delete;
    public:
        template<typename...Args>
        explicit constexpr Vector256(Args...args) requires (sizeof...(args) == 32 and (std::convertible_to<Args,std::uint8_t> && ...)) :
        _vec{_mm256_setr_epi8(args...)}{
        }

        explicit constexpr Vector256(const __m256i init) : _vec(init) {

        }

        explicit operator __m256i () const {
            return _vec;
        }
    };

    NODISCARD ALWAYS_INLINE Vector256 Avx2::LoadFromAddress(const std::uint8_t *const address, const std::ptrdiff_t offset) {
        return Vector256 {_mm256_loadu_si256(reinterpret_cast<__m256i const *>(address+offset))};
    }

    NODISCARD ALWAYS_INLINE Vector256 Avx2::CompareEqual(const Vector256 lhs, const Vector256 rhs) {
        return Vector256 {_mm256_cmpeq_epi8(static_cast<__m256i>(lhs), static_cast<__m256i>(rhs))};
    }

    NODISCARD ALWAYS_INLINE std::uint32_t Avx2::MoveMask(const Vector256 vec) {
        return static_cast<uint32_t>(_mm256_movemask_epi8(static_cast<__m256i>(vec)));
    }

    NODISCARD ALWAYS_INLINE Vector256 Avx2::ShuffleBytes(const Vector256 lookupTable,const Vector256 vec) {
        return Vector256{_mm256_shuffle_epi8(static_cast<__m256i>(lookupTable),static_cast<__m256i>(vec))};
    }

    NODISCARD ALWAYS_INLINE Vector256 Avx2::SubtractSaturated(const Vector256 lhs,const Vector256 rhs) {
        return Vector256{_mm256_subs_epu8(static_cast<__m256i>(lhs), static_cast<__m256i>(rhs))};
    }

    NODISCARD ALWAYS_INLINE Vector256 Avx2::Propagate(const std::uint8_t value) {
        return Vector256{_mm256_set1_epi8(static_cast<char>(value))};
    }

    NODISCARD ALWAYS_INLINE Vector256 Avx2::Or(const Vector256 lhs, const Vector256 rhs) {
        return Vector256{_mm256_or_si256(static_cast<__m256i>(lhs), static_cast<__m256i>(rhs))};
    }

    template<std::uint8_t shift>
    NODISCARD ALWAYS_INLINE Vector256 Avx2::RightShift(const Vector256 vec) {
        return Vector256{_mm256_srli_si256(static_cast<__m256i>(vec), shift)};
    }

    template<std::uint8_t shift>
    NODISCARD ALWAYS_INLINE Vector256 Avx2::Custom::RightShift8(const Vector256 vec) requires (shift < 8){
        //AVX2 doesn't 8bit or single byte lane bit shift! the closest thing to that is the 16bit variant
        //with bit of work and bit twiddling we can achieve the per byte right shift semantic by doing the following
        //(to ease this let's assume we have a vector with 0x5A and 0x7A as the first two bytes)
        //1- first for each 16 bits or 2 bytes we isolate the even bytes (so we are left with 0x7A and other even bytes)
        //(from 16bits perspective we have 0x007A ....)
        //2- same as first one but we here we isolate odd bytes (so we are left with 0x5A and other odd bytes)
        //(from 16bits perspective we have 0x005A ....)
        //3- right shift both odd and even bytes by 'shift' amount (0x7A will be 0x1E and 0x5A will be 0x16)
        //(from 16bits perspective we now have 0x001E ... and  0x0016 ...)
        //4- now merge the even bytes and odd bytes , we do this by left shifting the odd bytes by 8 and do
        //bitwise or so we have every byte back in place.
        //(so in our example we left shift the right shifted odd bytes from step 3 by 8 so 0x0016 will be 0x1600 ...
        //and now we do bitwise or with the right shifted even bytes from step 3 giving us: 0x1600 ... | 0x001E ... =
        //0x161E ...)

        const auto bytes = static_cast<__m256i>(vec);
        //1
        const __m256i even_bytes = _mm256_and_si256(bytes, _mm256_set1_epi16(0x00FF));
        //2
        const __m256i odd_bytes = _mm256_srli_epi16(bytes, 8);
        //3
        const __m256i shifted_evens = _mm256_srli_epi16(even_bytes, shift);
        const __m256i shifted_odds = _mm256_srli_epi16(odd_bytes, shift);
        //4
        const __m256i combined = _mm256_or_si256(shifted_evens, _mm256_slli_epi16(shifted_odds, 8));
        return Vector256{combined};
    }
}
#endif // __AVX2__
#endif //SIMDVECTOR_H

























