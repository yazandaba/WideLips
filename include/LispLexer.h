#ifndef LEXER_H
#define LEXER_H

#include <array>
#include <cstdint>
#include <memory>
#include "ADT/BumpVector.h"
#include "Diagnostic.h"
#include "Utilities/AlignedFileReader.h"
#include "AVX.h"
#include "MonoBumpVector.h"


namespace WideLips {

    std::size_t ArenaSizeEstimate(std::size_t fileSize,bool conservative);

    enum class LispTokenKind: std::uint8_t{
        EndOfFile           = '\0', // 0
        Not                 = '!',  // 33
        Hash                = '#',  // 35
        Dollar              = '$',  // 36
        Modulo              = '%',  // 37
        Ampersand           = '&',  // 38
        Quote               = '\'', // 39
        LeftParenthesis     = '(',  // 40
        RightParenthesis    = ')',  // 41
        Asterisk            = '*',  // 42
        Plus                = '+',  // 43
        Comma               = ',',  // 44
        Minus               = '-',  // 45
        Dot                 = '.',  // 46
        ForwardSlash        = '/',  // 47
        Column              = ':',  // 58
        LessThan            = '<',  // 60
        Equal               = '=',  // 61
        GreaterThan         = '>',  // 62
        At                  = '@',  // 64
        LeftBracket         = '[',  // 91
        BackwardSlash       = '\\', // 92
        RightBracket        = ']',  // 93
        BitwiseXor          = '^',  // 94
        QuasiColumn         = '`',  // 96
        BitwiseOr           = '|',  // 124
        Tilda               = '~',  // 126

        // Other tokens start from 128 to avoid collision with ASCII
        Identifier = 128,
        LeftBitShift,       // 129
        RightBitShift,      // 130
        LessThanOrEqual,    // 131
        GreaterThanOrEqual, // 132
        LogicalAnd,         // 133
        LogicalOr,          // 134
        RealLiteral,        // 135
        StringLiteral,      // 136
        BooleanLiteral,     // 137
        Let,                // 138
        Lambda,             // 139
        Nil,                // 140
        If,                 // 141
        Defun,              // 142
        Defmacro,           // 143
        Defvar,             // 144
        Comment,            // 145
        Fragment,           // 146
        Invalid             // 147
    };

    constexpr const char* TokenKindToString(const LispTokenKind kind) {
        switch (kind) {
            case LispTokenKind::Invalid: return "Invalid";
            case LispTokenKind::Fragment: return "Fragment";
            case LispTokenKind::Comment: return "Comment";
            case LispTokenKind::Let: return "Let";
            case LispTokenKind::BooleanLiteral: return "BooleanLiteral";
            case LispTokenKind::StringLiteral: return "StringLiteral";
            case LispTokenKind::RealLiteral: return "RealLiteral";
            case LispTokenKind::LogicalOr: return "||";
            case LispTokenKind::LogicalAnd: return "&&";
            case LispTokenKind::GreaterThanOrEqual: return ">=";
            case LispTokenKind::LessThanOrEqual: return "<=";
            case LispTokenKind::RightBitShift: return ">>";
            case LispTokenKind::LeftBitShift: return "<<";
            case LispTokenKind::Identifier: return "Identifier";
            case LispTokenKind::BitwiseOr: return "|";
            case LispTokenKind::BitwiseXor: return "^";
            case LispTokenKind::BackwardSlash: return "\\";
            case LispTokenKind::GreaterThan: return ">";
            case LispTokenKind::Equal: return "=";
            case LispTokenKind::LessThan: return "<";
            case LispTokenKind::ForwardSlash: return "/";
            case LispTokenKind::Dot: return ".";
            case LispTokenKind::Minus: return "-";
            case LispTokenKind::Plus: return "+";
            case LispTokenKind::Asterisk: return "*";
            case LispTokenKind::RightParenthesis: return ")";
            case LispTokenKind::LeftParenthesis: return "(";
            case LispTokenKind::Ampersand: return "&";
            case LispTokenKind::Quote: return "'";
            case LispTokenKind::Modulo: return "%";
            case LispTokenKind::Not: return "!";
            case LispTokenKind::Hash: return "#";
            case LispTokenKind::Comma: return ",";
            case LispTokenKind::LeftBracket: return "[";
            case LispTokenKind::RightBracket: return "]";
            case LispTokenKind::QuasiColumn : return "`";
            case LispTokenKind::Tilda : return "~";
            case LispTokenKind::Column : return ":";
            case LispTokenKind::At : return "@";
            case LispTokenKind::Dollar : return "$";
            case LispTokenKind::If: return "if";
            case LispTokenKind::Defmacro: return MacroKeyword;
            case LispTokenKind::Defun: return FuncKeyword;
            case LispTokenKind::Defvar: return VarKeyword;
            case LispTokenKind::Lambda: return LambdaKeyword;
            case LispTokenKind::Nil: return NilKeyword;
            case LispTokenKind::EndOfFile: return "EndOfFile";
            default: return "Unknown";
        }
    }

    struct WL_INTERNAL SExprIndex final {
        std::uint32_t Open;
        std::uint32_t OpenLine;
        std::uint32_t OpenColumn;
        std::uint32_t Close;
        std::uint32_t CloseLine;
        std::uint32_t CloseColumn;
        std::uint32_t Next;
    };

    struct WL_INTERNAL AuxiliaryIndex final {
        std::uint32_t At = 0;
        std::uint32_t Length = 0;
    };

    struct WL_INTERNAL alignas(32) TokenizationBlock final {
        const std::uint32_t FragmentsMask = 0;
        const std::uint32_t SExprAndOpsMask = 0;
        const std::uint32_t DigitsMask = 0;
        const std::uint32_t StringLiteralsMask = 0;
        const std::uint32_t NewLines = 0;
        const std::uint32_t IdentifierMask = 0;
    };

    struct WL_INTERNAL alignas(32) LispToken final {
    public:
        const char* TextPtr = nullptr;
        std::uint32_t Line = 1;
        std::uint32_t Length = 1;
        std::uint32_t AuxiliaryIndex = 0;
        std::uint32_t Column = 1;
        std::uint32_t IndexInSpecialStream = 0;
        LispTokenKind Kind = LispTokenKind::Invalid;
        std::uint8_t AuxiliaryLength = 0;
    public:
        NODISCARD ALWAYS_INLINE std::string_view GetText() const noexcept{
            return std::string_view{TextPtr, Length};
        }

        NODISCARD ALWAYS_INLINE std::wstring_view GetWText() const noexcept{
            return std::wstring_view{reinterpret_cast<std::wstring_view::const_pointer>(TextPtr), Length};
        }

        NODISCARD ALWAYS_INLINE PURE bool Match(const LispTokenKind kind) const noexcept{
            return Kind == kind;
        }

        NODISCARD ALWAYS_INLINE PURE std::uint32_t GetByteLocation(const char* textStream) const noexcept{
            return TextPtr - textStream;
        }

        NODISCARD ALWAYS_INLINE PURE bool IsOperator() const noexcept{
            switch (Kind) {
                case LispTokenKind::Not:
                case LispTokenKind::Modulo:
                case LispTokenKind::Ampersand:
                case LispTokenKind::Asterisk:
                case LispTokenKind::Quote:
                case LispTokenKind::Plus:
                case LispTokenKind::Minus:
                case LispTokenKind::Dot:
                case LispTokenKind::ForwardSlash:
                case LispTokenKind::BackwardSlash:
                case LispTokenKind::Equal:
                case LispTokenKind::LessThan:
                case LispTokenKind::GreaterThan:
                case LispTokenKind::BitwiseXor:
                case LispTokenKind::BitwiseOr:
                case LispTokenKind::LeftBitShift:
                case LispTokenKind::RightBitShift:
                case LispTokenKind::LessThanOrEqual:
                case LispTokenKind::GreaterThanOrEqual:
                    return true;
                default:
                    return false;
            }
        }

        NODISCARD ALWAYS_INLINE PURE bool IsDialectSpecial() const noexcept {
            switch (Kind) {
                default:
                    return false;
#ifdef EnableHash
                case LispTokenKind::Hash:
#endif
#ifdef EnableComma
                case LispTokenKind::Comma:
#endif
#ifdef EnableQuasiColumn
                case LispTokenKind::QuasiColumn:
#endif
#ifdef EnableColumn
                case LispTokenKind::Column:
#endif
#ifdef EnableAtSign
                case LispTokenKind::At:
#endif
#ifdef EnableBenjamin
                case LispTokenKind::Dollar:
#endif
#ifdef EnableTilda
                 case LispTokenKind::Tilda:
#endif

                    return true;
            }

        }

        NODISCARD PURE bool IsKeywordOperator() const noexcept {
            switch (Kind) {
                case LispTokenKind::Let:
                case LispTokenKind::LogicalAnd:
                case LispTokenKind::LogicalOr:
                case LispTokenKind::Not:
                case LispTokenKind::Lambda:
                case LispTokenKind::If:
                case LispTokenKind::Defun:
                case LispTokenKind::Defmacro:
                case LispTokenKind::Defvar:
                    return true;
                default:
                    return false;
            }
        }

        NODISCARD ALWAYS_INLINE PURE bool IsFragmentOrComment() const noexcept{
            return Kind == LispTokenKind::Fragment or Kind == LispTokenKind::Comment;
        }
    };

    class LispLexer {
        using TokenRegion = std::pair<const std::uint32_t, const std::uint32_t>;
        using StaticTokenRegion = std::pair<const char*, const std::uint32_t>;
        using RegionOfTokens = std::pair<const LispToken * const,const LispToken * const>;
        using OptRegionOfTokens = std::optional<RegionOfTokens>;
    private:
        struct ConstructorEnabler {};
        static const ConstructorEnabler CtorEnabler;
        static constexpr std::uint32_t TokensInBlock = sizeof(Vector256);
        static constexpr std::uint32_t TokensInBlockBoundary = TokensInBlock-1;
        static constexpr std::uint32_t TokensInBlockPopCnt = std::countr_zero(sizeof(Vector256));
    private:
        MonoBumpVector<TokenizationBlock> _blocks;
        MonoBumpVector<SExprIndex> _sexprIndices;
        MonoBumpVector<LispToken> _tokens;
        MonoBumpVector<AuxiliaryIndex> _auxiliaries;
        BumpVector<Diagnostic::LispDiagnostic> _diagnostics;
        std::wstring_view _filePath;
        std::string_view _text;
        std::uint32_t _currentTokenAuxiliary = 0;
        std::uint32_t _sexprIndex = 0;
        std::uint32_t _tokenStreamPos = 0;
        std::uint32_t _textStreamPos = 0;
        std::uint32_t _line = 1;
        std::uint32_t _column = 1;
        bool _tokenized = false;
        bool _reused = false;
    public:
        WL_API explicit LispLexer(UNUSED ConstructorEnabler enabler,std::string_view file,std::wstring_view filePath,bool conservative);
        LispLexer(const LispLexer&) = delete;
        LispLexer(LispLexer&&) = delete;
        LispLexer& operator = (LispLexer&&) = delete;
        LispLexer& operator = (const LispLexer&) = delete;
    public:
        NODISCARD static std::unique_ptr<LispLexer> Make(AlignedFileReadResult& alignedFile,
            std::wstring_view fileName,
            bool conservative = false);
        NODISCARD static std::unique_ptr<LispLexer> Make(std::string_view program, bool conservative = true);
    public:
        WL_API bool Tokenize() noexcept;
        WL_API OptRegionOfTokens TokenizeFirstSExpr() noexcept;
        WL_API OptRegionOfTokens TokenizeNext(const LispToken* token) noexcept;
        WL_API OptRegionOfTokens TokenizeSExpr(const LispToken* begin,bool csEmptySExpr=false) noexcept;
        NODISCARD WL_API OptRegionOfTokens GetTokenAuxiliary(const LispToken* token) noexcept;
        NODISCARD WL_API BumpVector<Diagnostic::LispDiagnostic>& GetDiagnostics() noexcept;
        NODISCARD WL_API std::wstring_view GetFilePath() const noexcept;
        NODISCARD WL_API std::size_t GetFileSize() const noexcept;
        NODISCARD WL_API const char* GetTextData() const noexcept;
        WL_API void Reuse() noexcept;
    private:
        void Classify();
        OptRegionOfTokens TokenizeSExprCore(const LispToken* begin,bool csEmptySExpr) noexcept;
        char NextChar() noexcept;
        char NextCharWithoutColumn() noexcept;
        NODISCARD char CurrentChar() const noexcept;
        NODISCARD char SkipToCharAt(std::size_t offset) noexcept;
        NODISCARD char SkipToCharAtWithoutColumn(std::size_t offset) noexcept;
        NODISCARD TokenRegion FetchStringRegion(std::uint32_t startingBlock,std::uint8_t posInBlock) noexcept;
        NODISCARD TokenRegion FetchCommentRegion(std::uint32_t startingBlock,std::uint8_t posInBlock) noexcept;
        NODISCARD TokenRegion FetchFragmentRegion(std::uint32_t startingBlock,
            std::uint8_t posInBlock,
            const TokenizationBlock* currentBlock) noexcept;
        NODISCARD TokenRegion FetchDigitRegion(std::uint32_t startingBlock,std::uint8_t posInBlock) noexcept;
        NODISCARD TokenRegion FetchIdentifierRegion(std::uint32_t startingBlock,std::uint8_t posInBlock) noexcept;
        void TokenizeOperatorsOrStructural(std::uint8_t fragLength) noexcept;
        NODISCARD TokenizationBlock* TokenizationBlockAt(std::uint32_t pos) noexcept;
        NODISCARD std::uint8_t OffsetInBlock() const noexcept;
        NODISCARD bool IsEndOfFile() const noexcept;
        bool TokenizeBlue();
        TokenRegion TokenizeRealBlue(std::uint32_t startingBlock,
            std::uint8_t posInBlock,
            const TokenizationBlock* currentBlock) noexcept;
        StaticTokenRegion TokenizeOperatorsOrStructuralBlue() noexcept;
        bool CheckAtomsAtTopLevelBlue() noexcept;
    private:
        NODISCARD PURE static bool IsOperator(char c) noexcept;
        NODISCARD PURE static bool IsDecimal(char c) noexcept;
        NODISCARD PURE static LispTokenKind IsKeyword(std::string_view identifier) noexcept;
        NODISCARD PURE static bool IsComment(char c) noexcept;
        NODISCARD PURE static bool IsFragment(char c) noexcept;
    };

    class WL_INTERNAL PredefinedTokens final {
    public:
        ~PredefinedTokens() = delete;
    public:
        static constexpr LispToken EndOfFile {nullptr,0,0,0,0,0,LispTokenKind::EndOfFile};
    };

    inline const LispLexer::ConstructorEnabler LispLexer::CtorEnabler{};
}

#endif //LEXER_H























