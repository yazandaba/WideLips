#ifndef BUMPVECTOR_H
#define BUMPVECTOR_H

#include <cassert>
#include <bit>
#include <new>
#include <immintrin.h>
#include <source_location>
#include "Config.h"

namespace WideLips {

    template<typename T>
    class BumpVector;

    template<typename T>
    struct BumpVectorIterator;

    template<typename T>
    struct BumpAllocatorTypeTraits {
        using ReferenceType = T&;
        using ConstReferenceType = const T&;
        using PointerType = T*;
        using ConstPointerType = const T*;
        using SizeType = std::size_t;
        using OffsetType = std::ptrdiff_t;
    };

    template<typename T,std::uint16_t alignment>
    struct BumpArena {
        static_assert(std::has_single_bit(alignment),"Alignment must be power of 2");
    private:
        using PointerType = BumpAllocatorTypeTraits<T>::PointerType;
        using SizeType = BumpAllocatorTypeTraits<T>::SizeType;
        using ReferenceType = BumpAllocatorTypeTraits<T>::ReferenceType;
        using OffsetType = BumpAllocatorTypeTraits<T>::OffsetType;
    private:
        SizeType _arenaSize;
        SizeType _areanCapacity;
        PointerType _arena;
        PointerType _pin;
    public:
        /**
         * @brief
         * Constructs a BumpArena with the specified size where size is the number of objects (i.e. arenaSize*sizeof(T)).
         *
         * @param arenaSize The size of the arena in terms of the number of elements.
         *                  It specifies the total size of memory to allocate for the arena.
         *                  Must be large enough to hold elements of the specified type `T`.
         *                  The memory is aligned to the specified alignment.
         * @return A constructed instance of `BumpArena` initialized with the allocated memory block.
         */
        explicit BumpArena(const SizeType arenaSize) :
        _arenaSize(arenaSize),
        _areanCapacity(arenaSize*sizeof(T)),
        _arena(static_cast<PointerType>(operator new [](arenaSize*sizeof(T),std::align_val_t{alignment},std::nothrow))),
        _pin(_arena-1){}
        BumpArena(BumpArena const &arena) = delete;
        BumpArena(BumpArena&& arena) noexcept:
        _arenaSize(arena._arenaSize),
        _areanCapacity(arena._areanCapacity),
        _arena(arena._arena),
        _pin(arena._pin) {
            arena._arena = nullptr;
            arena._pin = nullptr;
        }
        BumpArena& operator=(BumpArena const &arena) = delete;
        BumpArena& operator=(BumpArena &&arena) noexcept {
            _arenaSize = arena._arenaSize;
            _areanCapacity = arena._areanCapacity;
            _arena = arena._arena;
            _pin = arena._pin;
            arena._arena = nullptr;
            arena._pin = nullptr;
            return *this;
        }
    public:
        NODISCARD ALWAYS_INLINE PointerType Begin() const noexcept {
            return _arena;
        }

        NODISCARD ALWAYS_INLINE PointerType End() const noexcept {
            return _arena+_arenaSize;
        }

        NODISCARD ALWAYS_INLINE SizeType Size() const noexcept {
            return _arenaSize;
        }

        NODISCARD ALWAYS_INLINE PointerType Allocate() noexcept{
            PointerType ptr = ++_pin;
            if (_pin-_arena >= _arenaSize) [[unlikely]]{
                return nullptr;
            }
            return ptr;
        }

        ALWAYS_INLINE void Reuse() noexcept {
            _pin = _arena-1;
        }

        NODISCARD ALWAYS_INLINE PointerType At(const SizeType index) const noexcept{
            PointerType ptr = _arena+index;
            if (ptr > _pin) [[unlikely]]{
                return nullptr;
            }
            return ptr;
        }

        template<typename...Args>
        NODISCARD PointerType Construct(Args&&... args) {
            PointerType ptr = Allocate();
            if (ptr != nullptr) [[likely]] {
                return new (ptr) T{std::forward<Args>(args)...};
            }
            return nullptr;
        }

        template<typename...Args>
        NODISCARD ALWAYS_INLINE PointerType ConstructValue(Args&&... args) {
            PointerType ptr = Allocate();
            if (ptr != nullptr) [[likely]] {
                return new (ptr) T{std::forward<Args>(args)...};
            }
            return nullptr;
        }

        NODISCARD ALWAYS_INLINE OffsetType Position() const {
            return _pin -_arena;
        }

        ~BumpArena() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                if (_arena - _pin == 0) {
                    _pin->~T();
                    return;
                }
                for (T* elementPtr = _arena; elementPtr < _pin; ++elementPtr) [[likely]]{
                    elementPtr->~T();
                }
            }
            operator delete[](_arena,std::align_val_t{alignment},std::nothrow);
        }
    };

    template<typename T>
    struct BumpAllocator : BumpAllocatorTypeTraits<T>{
        using typename BumpAllocatorTypeTraits<T>::PointerType;
        using typename BumpAllocatorTypeTraits<T>::ConstPointerType;
        using typename BumpAllocatorTypeTraits<T>::SizeType;
        using typename BumpAllocatorTypeTraits<T>::ReferenceType;
        using ArenaType = BumpArena<T,alignof(T)>;
    private:
        static constexpr std::size_t BackupArenaSizeBytes = 65536; //64kB
    private:
        std::size_t _currentArena = 0;
        std::size_t _arenasSize = 1;
        /**
         * when there are no more arenas to use, and we need to add new arenas to the list of arenas,
         * this capacity will get used for each new arena
         */
        const SizeType _arenaAllocSize;
        const SizeType _arenaAllocSizeBound;
        /**
         * root of arenas
         */
        ArenaType* _arenas;
        const std::uint8_t _dividend;
    public:
        explicit BumpAllocator(const SizeType arenaSize) :
        _arenaAllocSize(arenaSize),
        _arenaAllocSizeBound(arenaSize-1),
        _arenas(static_cast<ArenaType*>(operator new [](_arenasSize * sizeof(ArenaType),std::align_val_t{alignof(ArenaType)},std::nothrow))),
        _dividend(std::countr_zero(arenaSize)){
            assert(std::has_single_bit(arenaSize) && "arenaSize must be power of 2");
            if (_arenas == nullptr) [[unlikely]]{
                std::puts("FATAL: Bump allocator failed to allocate memory");
                std::exit(-1);
            }
            new(&_arenas[0])ArenaType{arenaSize == 0 ? BackupArenaSizeBytes : arenaSize};
        }
        BumpAllocator(const BumpAllocator&) = delete;
        BumpAllocator(BumpAllocator&&) noexcept = delete;
        BumpAllocator& operator=(const BumpAllocator&) = delete;
        BumpAllocator& operator=(BumpAllocator&&) noexcept = delete;
    public:

        NODISCARD ALWAYS_INLINE PointerType Allocate() {
            auto ptr = _arenas[_currentArena].Allocate();
            if (ptr == nullptr) [[unlikely]]{
                if (NextArenaIsEmpty()) {
                    ++_currentArena;
                }
                else {
                    _arenas = ReallocateArenas();
                    if (_arenas == nullptr) [[unlikely]]{
                        std::puts("FATAL: Bump allocator failed to allocate memory");
                        std::exit(-1);
                    }
                }

                ptr = _arenas[_currentArena].Allocate();
            }

            return ptr;
        }

        template<typename...Args>
        ALWAYS_INLINE ReferenceType Construct(Args&&... args) {
            auto ptr = _arenas[_currentArena].Construct(std::forward<Args>(args)...);
            if (ptr == nullptr) [[unlikely]]{
                if (NextArenaIsEmpty()) {
                    ++_currentArena;
                }
                else {
                    _arenas = ReallocateArenas();
                    if (_arenas == nullptr) [[unlikely]]{
                        std::puts("FATAL: Bump allocator failed to allocate memory");
                        std::exit(-1);
                    }
                }

                ptr = _arenas[_currentArena].Construct(std::forward<Args>(args)...);
            }

            return *ptr;
        }

        template<typename...Args>
        ALWAYS_INLINE ReferenceType ConstructValue(Args&&... args) {
            auto ptr = _arenas[_currentArena].ConstructValue(std::forward<Args>(args)...);
            if (ptr == nullptr) [[unlikely]]{
                if (NextArenaIsEmpty()) {
                    ++_currentArena;
                }
                else {
                    _arenas = ReallocateArenas();
                    if (_arenas == nullptr) [[unlikely]]{
                        std::puts("FATAL: Bump allocator failed to allocate memory");
                        std::exit(-1);
                    }
                }

                ptr = _arenas[_currentArena].ConstructValue(std::forward<Args>(args)...);
            }

            return *ptr;
        }

        NODISCARD ALWAYS_INLINE PointerType At(const SizeType index) {
#ifndef NDEBUG
            assert(index < _arenasSize*_arenaAllocSize && "out of range memory access");
#endif
            auto& belongToBlock = _arenas[index >> _dividend];
            auto element = belongToBlock.At(index&_arenaAllocSizeBound);
            return element;
        }

        NODISCARD ALWAYS_INLINE ConstPointerType At(const SizeType index) const {
#ifndef NDEBUG
            assert(index < _arenasSize*_arenaAllocSize && "out of range memory access");
#endif
            auto& belongToBlock = _arenas[index >> _dividend];
            auto element = belongToBlock.At(index&_arenaAllocSizeBound);
            return element;
        }

        ALWAYS_INLINE void Reuse() {
            for (int i=0;i<_arenasSize;++i) {
                _arenas[i].Reuse();
            }
            _currentArena = 0;
        }

        void Release() {
            for (int i=0;i< _arenasSize; ++i) {
                _arenas[i].~ArenaType();
            }
            operator delete [](_arenas,std::align_val_t{alignof(ArenaType)},std::nothrow);
        }
    private:
        NODISCARD bool NextArenaIsEmpty() const{
            if (const auto nextArenaIndex = _currentArena+1; nextArenaIndex < _arenasSize) {
                ArenaType& nextArena = _arenas[nextArenaIndex];
                return nextArena.Position() == -1;//arena pin start at position N-1 where N is the beginning of the arena
            }

            return false;
        }

        NODISCARD ArenaType* ReallocateArenas(const bool jumpToNextArena = true) {
            auto oldArenas = _arenas;
            const auto oldSize = _arenasSize;
            _arenasSize <<= 1;
            auto* arenas = static_cast<ArenaType*>(operator new [](_arenasSize*sizeof(ArenaType),std::align_val_t{alignof(ArenaType)},std::nothrow));
            if (arenas == nullptr) [[unlikely]]{
                return arenas;
            }
            std::size_t i = 0;
            for (; i < oldSize; ++i) [[likely]]{
                new(&arenas[i])ArenaType{std::move(oldArenas[i])};
            }
            for (; i < _arenasSize; ++i) [[likely]]{
                new(&arenas[i])ArenaType{_arenaAllocSize};
            }
            operator delete[](oldArenas,std::align_val_t{alignof(ArenaType)},std::nothrow);
            if (jumpToNextArena) {
                ++_currentArena;
            }
            return arenas;
        }
    };

    template<typename T>
    struct BumpVectorIterator {
        friend class BumpVector<T>;
    public:
        //type aliases for std::ranges compatibility
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = std::size_t;
        using iterator_category = std::random_access_iterator_tag;
        using difference_type = std::ptrdiff_t;
    private:
        const BumpVector<T> * _vector;
        std::size_t _offset = 0;
    private:
        explicit BumpVectorIterator(const BumpVector<T>* vector,const std::size_t offset=0) :
        _vector(vector),
        _offset(offset){}
    public:
        BumpVectorIterator() = default;
        BumpVectorIterator(const BumpVectorIterator&) = default;
        BumpVectorIterator(BumpVectorIterator&&) = default;
    public:
        BumpVectorIterator& operator ++() noexcept {
            ++_offset;
            return *this;
        }

        BumpVectorIterator operator ++(int) noexcept {
            return BumpVectorIterator{_vector,_offset++};
        }

        BumpVectorIterator operator +(const difference_type offset) const noexcept{
            return {_vector,_offset+offset};
        }

        BumpVectorIterator& operator --() noexcept {
            --_offset;
            return *this;
        }

        BumpVectorIterator operator --(int) noexcept {
            return BumpVectorIterator{_vector,_offset--};
        }

        pointer operator - (const difference_type offset) const noexcept{
            return {_vector,_offset-offset};
        }

        bool operator == (const BumpVectorIterator& other) const noexcept {
            return _vector == other._vector && _offset == other._offset;
        }

        bool operator != (const BumpVectorIterator& other) const noexcept {
            return !(*this == other);
        }

        const_reference operator *() const noexcept {
#ifndef NDEBUG
            assert(_offset < _vector->Size() && "out of range memory access");
#endif
            return *_vector->At(_offset);
        }

        const_pointer operator ->() const noexcept {
            return _vector->At(_offset);
        }
    };


    /**
    * @brief Represents an arena-based vector-like container using a bump allocator for fast memory allocation.
    * This class is designed for use cases where memory is allocated once and managed linearly.
    * @remarks vector internal allocator will allocate 64KB sized arenas in case memory usage exceeded the allocated
    * memory of the main arena.
    * @tparam T The type of elements stored in the vector.
    */
    template<typename T>
    class BumpVector final {
    public:
        using SizeType = BumpAllocator<T>::SizeType;
        using ReferenceType = BumpAllocator<T>::ReferenceType;
        using ConstReferenceType = BumpAllocator<T>::ConstReferenceType;
        using PointerType = BumpAllocator<T>::PointerType;
        using ConstPointerType = BumpAllocator<T>::ConstPointerType;
        using IteratorType = BumpVectorIterator<T>;
        using ConstIteratorType = const BumpVectorIterator<T>;
    private:
        BumpAllocator<T> _allocator;
        std::size_t _size = 0;
    public:
        /**
         * @brief Constructs a `BumpVector` with the specified arena size
         *
         * @param arenaSize The size of the arena in terms of the number of elements or objects, not bytes.
         *                  Allocates enough memory to accommodate the specified number
         *                  of elements using the bump allocator.
         * @return A constructed instance of `BumpVector` initialized with the specified memory allocation.
         */
        explicit BumpVector(const SizeType arenaSize) : _allocator(arenaSize){}
        BumpVector(const BumpVector&) = delete;
        BumpVector(BumpVector&& vector) = delete;
        BumpVector& operator=(const BumpVector&) = delete;
        BumpVector& operator=(BumpVector&&) = delete;
        ~BumpVector() {
            _allocator.Release();
        }
    public:

        IteratorType begin() noexcept {
            return IteratorType{this};
        }

        IteratorType end() noexcept {
            return IteratorType{this,_size};
        }

        ConstIteratorType begin() const noexcept {
            return ConstIteratorType{this};
        }

        ConstIteratorType end() const noexcept {
            return ConstIteratorType{this,_size};
        }

        ConstIteratorType cbegin() const noexcept {
            return ConstIteratorType{this};
        }

        ConstIteratorType cend() const noexcept {
            return ConstIteratorType{this,_size};
        }

        template<typename...Args>
        void EmplaceBack(Args&&...args) {
            _allocator.Construct(std::forward<Args>(args)...);
            ++_size;
        }

        template<typename...Args>
        ALWAYS_INLINE void EmplaceBackValue(Args...args) {
            _allocator.ConstructValue(std::forward<Args>(args)...);
            ++_size;
        }

        ALWAYS_INLINE void EmplaceBackTrivial(T&& obj) noexcept requires (std::is_trivially_copyable_v<T>){
            T* ptr = _allocator.Allocate();
            if constexpr (sizeof(T) == 8 or sizeof(T) == 4 or sizeof(T) == 2 or sizeof(T) == 1) {
                *ptr = obj;
            }
            else if constexpr (sizeof(T) == 32) {
                const __m256i temp = _mm256_load_si256(reinterpret_cast<const __m256i*>(&obj));
                _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), temp);
            }
            else {
                std::memcpy(ptr, &obj, sizeof(T));
            }
            ++_size;
        }

        ALWAYS_INLINE void PopBack() noexcept {
            --_size;
        }

        NODISCARD ALWAYS_INLINE ReferenceType Back() {
            return *_allocator.At(_size-1);
        }

        NODISCARD ALWAYS_INLINE ConstReferenceType Back() const {
            return *_allocator.At(_size-1);
        }

        NODISCARD ALWAYS_INLINE PointerType At(const SizeType index) {
            return _allocator.At(index);
        }

        NODISCARD ALWAYS_INLINE ConstPointerType At(const SizeType index) const {
            return _allocator.At(index);
        }

        NODISCARD ALWAYS_INLINE ReferenceType operator[](const SizeType index) {
            auto elementPtr = _allocator.At(index);
#ifndef NDEBUG
            assert(elementPtr != nullptr && "out of range memory access" );
#endif
            return *elementPtr;
        }

        NODISCARD ALWAYS_INLINE ConstReferenceType operator[](const SizeType index) const {
            auto elementPtr = _allocator.At(index);
#ifndef NDEBUG
            assert(elementPtr != nullptr && "out of range memory access");
#endif
            return *elementPtr;
        }

        NODISCARD ALWAYS_INLINE SizeType Size() const noexcept {
            return _size;
        }

        NODISCARD ALWAYS_INLINE bool Empty() const noexcept {
            return _size == 0;
        }

        ALWAYS_INLINE void Reuse() noexcept {
            _size = 0;
            _allocator.Reuse();
        }

        ALWAYS_INLINE void Release() noexcept {
            _allocator.Release();
        }
    };


}

#endif //BUMPVECTOR_H
