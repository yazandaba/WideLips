#ifndef WIDELIPS_MONOBUMPVECTOR_H
#define WIDELIPS_MONOBUMPVECTOR_H
#include <type_traits>
#include "Config.h"

namespace WideLips {
    template<typename T>
    struct alignas(16) MonoBumpVector final {
        static_assert(std::is_trivially_copyable_v<T> && "element type must be trivially copyable");
        static_assert(std::is_trivially_destructible_v<T> && "element type must be trivially destructible");
    private:
        using PointerType = T*;
        using ConstPointerType = const T*;
        using ReferenceType = T&;
        using ConstReferenceType = const T&;
        using SizeType = std::size_t;
    private:
        T* _arena;
        T* _pin;
    public:
        explicit MonoBumpVector(const SizeType arenaSize) :
        _arena(static_cast<T*>(operator new [](arenaSize*sizeof(T),std::align_val_t{alignof(T)},std::nothrow)) ),
        _pin(_arena-1){}
        MonoBumpVector(const MonoBumpVector &monoBumpVector) = delete;
        MonoBumpVector(MonoBumpVector &&monoBumpVector) noexcept : _arena(monoBumpVector._arena), _pin(monoBumpVector._pin) {
            monoBumpVector._arena = nullptr;
            monoBumpVector._pin = nullptr;
        }
        MonoBumpVector& operator=(const MonoBumpVector &monoBumpVector) = delete;
        MonoBumpVector& operator=(MonoBumpVector &&monoBumpVector) noexcept {
            _arena = monoBumpVector._arena;
            _pin = monoBumpVector._pin;
            monoBumpVector._arena = nullptr;
            monoBumpVector._pin = nullptr;
            return *this;
        }
        ~MonoBumpVector() {
            operator delete(_arena,std::align_val_t{alignof(T)},std::nothrow);
        }
    public:
        NODISCARD PointerType begin() noexcept { return _arena; }
        NODISCARD PointerType end() noexcept { return _pin + 1; }
        NODISCARD ConstPointerType begin() const noexcept { return _arena; }
        NODISCARD ConstPointerType end() const noexcept { return _pin + 1; }
        NODISCARD ConstPointerType cbegin() const noexcept { return _arena; }
        NODISCARD ConstPointerType cend() const noexcept { return _pin + 1; }

        ALWAYS_INLINE PointerType EmplaceBack(T&& element) noexcept {
            auto mem = ++_pin;
            if constexpr (sizeof(T) == 8 or sizeof(T) == 4 or sizeof(T) == 2 or sizeof(T) == 1) {
                *mem = element;
            }
            else {
                std::memcpy(mem, &element, sizeof(T));
            }
            return mem;
        }

        ALWAYS_INLINE PointerType Preserve() noexcept {
            return ++_pin;
        }

        ALWAYS_INLINE PointerType At(SizeType index) noexcept {
            return _arena+index;
        }

        ALWAYS_INLINE ReferenceType operator[](SizeType index) const noexcept {
            return _arena[index];
        }

        ALWAYS_INLINE ReferenceType operator[](SizeType index) noexcept {
            return _arena[index];
        }

        ALWAYS_INLINE void PopBack() noexcept {
            --_pin;
        }

        NODISCARD ALWAYS_INLINE ConstReferenceType Back() noexcept {
            return *_pin;
        }

        NODISCARD ALWAYS_INLINE ConstReferenceType Back() const noexcept {
            return *_pin;
        }

        NODISCARD ALWAYS_INLINE SizeType Size() const noexcept {
            return (_pin - _arena)+1;
        }

        NODISCARD ALWAYS_INLINE bool Empty() const noexcept {
            return _pin < _arena;
        }

        ALWAYS_INLINE void Reuse() noexcept {
            _pin = _arena-1;
        }
    };
}
#endif //WIDELIPS_MONOBUMPVECTOR_H