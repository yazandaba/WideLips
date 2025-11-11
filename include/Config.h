#ifndef MACROS_H
#define MACROS_H

#ifdef WIDELIPS_SHARED
    #ifdef _WIN32
        #ifdef WIDELIPS_EXPORTS
            #define WL_API __declspec(dllexport)
        #else
            #define WL_API __declspec(dllimport)
        #endif
    #else
        #if defined(__GNUC__) || defined(__clang__)
            #ifdef WIDELIPS_EXPORTS
                #define WL_API __attribute__((visibility("default")))
            #else
                #define WL_API
            #endif
        #else
            #define WL_API
        #endif
    #endif
#else
    #define WL_API
#endif


#if defined(__GNUC__) || defined(__clang__)
    #define WL_EXPORT __attribute__((visibility("default")))
    #define WL_HIDDEN __attribute__((visibility("hidden")))
    #define WL_INTERNAL __attribute__((visibility("internal")))
#ifdef NDEBUG
    //all symbols annotated with aggressive inlining are internal, so the compiler
    //can actually inline them instead of generating an indirect call sequence from PLT, or it's equivalence
    //in other formats.
    //NOTE: this is important for ELF specifically for COFF and Windows, symbols are exposed or exported by default
    #define ALWAYS_INLINE WL_INTERNAL __attribute__((always_inline)) inline
#else
    #define ALWAYS_INLINE inline
#endif
    #define NOINLINE __attribute__((noinline))
    #define PURE __attribute__((pure))
    #define ASSUME_ALIGNED(n) __attribute__((assume_aligned(n)))
#elif _MSC_VER
    #ifdef BUILD_SHARED_LIB
        #define WL_EXPORT __declspec(dllexport)
    #else
        #define WL_EXPORT __declspec(dllimport)
    #endif
    #define WL_HIDDEN
    #define WL_INTERNAL
#ifdef NDEBUG
    #define ALWAYS_INLINE __forceinline
#else
    #define ALWAYS_INLINE inline
#endif
    #define NOINLINE __declspec(noinline)
    #define PURE
#else
    #define WL_EXPORT
    #define WL_HIDDEN
    #define WL_INTERNAL
    #define ALWAYS_INLINE inline
    #define NOINLINE
    #define PURE
    #define ASSUME_ALIGNED(n) __declspec(align_value(n))
#endif


#define NODISCARD [[nodiscard]]
#define UNUSED [[maybe_unused]]
#define FALLTHROUGH [[fallthrough]]


#ifdef EnableHash
    inline constexpr char HashChar = '#';
#else
    inline constexpr char HashChar = '\0';
#endif

#ifdef EnableComma
    inline constexpr char CommaChar = ',';
#else
    inline constexpr char CommaChar = '\0';
#endif

#ifdef EnableBrackets
    inline constexpr char LeftBracketChar = '[';
    inline constexpr char RightBracketChar = ']';
#else
    inline constexpr char LeftBracketChar = '\0';
    inline constexpr char RightBracketChar = '\0';
#endif

#ifdef EnableQuasiColumn
    inline constexpr char QuasiColumnChar = '`';
#else
    inline constexpr char QuasiColumnChar = '\0';
#endif

#ifdef EnableColumn
    inline constexpr char ColumnChar = ':';
#else
    inline constexpr char ColumnChar = '\0';
#endif

#ifdef EnableAtSign
    inline constexpr char AtChar = '@';
#else
    inline constexpr char AtChar = '\0';
#endif

#ifdef EnableBenjamin
    inline constexpr char DollarChar = '$';
#else
    inline constexpr char DollarChar = '\0';
#endif

#ifdef EnableDashInID
    inline constexpr char DashInId = '-';
#else
    inline constexpr char DashInId = '\0';
#endif

#ifndef EnableTilda
    inline constexpr char TildaChar = '~';
#else
    inline constexpr char TildaChar = '\0';
#endif

#if defined EnableHash || EnableComma || EnableBrackets || EnableQuasiColumn || EnableColumn || EnableAtSign || EnableBenjamin
    #define EnableDialectSpecials
#endif

#ifdef InvalidateEmptySExpr
    inline constexpr bool DisallowEmptySExpr = true;
#else
    inline constexpr bool DisallowEmptySExpr = false;
#endif

#ifndef FuncKeyword
    #define FuncKeyword "defun"
#endif

#ifndef MacroKeyword
    #define MacroKeyword "defmacro"
#endif

#ifndef VarKeyword
    #define VarKeyword "defvar"
#endif

#ifndef LambdaKeyword
    #define LambdaKeyword "lambda"
#endif

#ifndef TrueLiteral
    #define TrueLiteral "true"
#endif

#ifndef FalseLiteral
    #define FalseLiteral "false"
#endif

#ifndef NilKeyword
    #define NilKeyword "nil"
#endif

constexpr std::uint32_t PaddingSize = 32;

#endif //MACROS_H