#include <bit>
#include <memory_resource>
#include <filesystem>
#include "../include/AVX.h"
#include "../include/LispLexer.h"
#include "../include/Utilities/AlignedFileReader.h"
#include "Config.h"

namespace WideLips {

    WL_API constexpr std::uint64_t AlignToPowOfTow(const std::uint64_t x) {
        return 1U << ((sizeof(x)*8)-std::countl_zero(x));
    }

    std::size_t ArenaSizeEstimate(const std::size_t fileSize, const bool conservative) {
        constexpr std::size_t kiloByte = 1024;
        constexpr std::size_t megaByte = 1024 * kiloByte;

        // File size tiers
        constexpr std::size_t fileTier1 = 16 * kiloByte;
        constexpr std::size_t fileTier2 = 256 * kiloByte;

        // Arena sizes
        constexpr std::size_t arenaTier1 = 64 * kiloByte;
        constexpr std::size_t arenaTier2_Conservative = 256 * kiloByte;
        constexpr std::size_t arenaTier2_Default = 512 * kiloByte;
        constexpr std::size_t arenaTier3 = 1 * megaByte;

        // For files smaller than 16KB
        if (fileSize <= fileTier1) {
            return arenaTier1;
        }

        // For files between 16KB and 256KB
        if (fileSize <= fileTier2) {
            return conservative ? arenaTier2_Conservative : arenaTier2_Default;
        }

        // For files larger than 256KB
        return fileSize >= arenaTier3 ? fileSize : arenaTier3;
    }

    LispLexer::LispLexer(UNUSED ConstructorEnabler enabler,
        const std::string_view file,
        const std::wstring_view filePath,
        const bool conservative):
    _blocks(AlignToPowOfTow(file.size() / 32 == 0 ? 1 : file.size() / 32 + 1)),
    _sexprIndices(AlignToPowOfTow(ArenaSizeEstimate(file.size(), conservative)/2)),
    _tokens(AlignToPowOfTow(ArenaSizeEstimate(file.size(), conservative))),
    _auxiliaries(AlignToPowOfTow(ArenaSizeEstimate(file.size(), conservative)/2)),
    _diagnostics(1024),
    _filePath(filePath),
    _text(file) {

    }

    std::unique_ptr<LispLexer> LispLexer::Make(AlignedFileReadResult& alignedFile,
        const std::wstring_view fileName,
        const bool conservative) {
        if (!alignedFile) {
            return nullptr;
        }
        return std::make_unique<LispLexer>(CtorEnabler,std::string_view(alignedFile.get()),fileName,conservative);
    }

    std::unique_ptr<LispLexer> LispLexer::Make(std::string_view program, const bool conservative) {
        return std::make_unique<LispLexer>(CtorEnabler,program,L"memory",conservative);
    }

    bool LispLexer::Tokenize() noexcept{
        if (_tokenized && !_reused) {
#ifndef NDEBUG
            assert(!"Calling LispLexer::Tokenize more than once is prohibited");
#else
            return false;
#endif
        }
        return TokenizeBlue();
    }

    LispLexer::OptRegionOfTokens LispLexer::TokenizeFirstSExpr() noexcept{
        if (_sexprIndices.Size() == 0) {
            _diagnostics.EmplaceBack(Diagnostic::DiagnosticFactory::ProgramMustStartWithSExpression(
                _filePath,
                0,
                0));
            return std::nullopt;
        }
        const SExprIndex& firstSExpr = _sexprIndices[0];
        const char optSegOrComment = *_text.data();
        LispToken* firstSExprBegin = nullptr;
        if (IsComment(optSegOrComment) or IsFragment(optSegOrComment)) {
            //span of first auxiliary is from 0 to first SExpr in file
            _auxiliaries.EmplaceBack({0,firstSExpr.Open});
            firstSExprBegin = _tokens.EmplaceBack(LispToken{
                _text.data()+firstSExpr.Open,
                firstSExpr.OpenLine,
                1,
                static_cast<std::uint32_t>(_auxiliaries.Size()-1),
                firstSExpr.OpenColumn,
                0,
                LispTokenKind::LeftParenthesis,
                1
            });
        }
        else {
            firstSExprBegin = _tokens.EmplaceBack(LispToken{
                _text.data()+firstSExpr.Open,
                firstSExpr.OpenLine,
                1,
                0,
                firstSExpr.OpenColumn,
                0,
                LispTokenKind::LeftParenthesis,
                0
            });
        }

        const LispToken* const firstSExprEnd = _tokens.EmplaceBack(LispToken{
               _text.data()+firstSExpr.Close,
               firstSExpr.CloseLine,
               1,
               0,
               firstSExpr.CloseColumn,
               0,
               LispTokenKind::RightParenthesis,
            std::numeric_limits<std::uint8_t>::max() //special value indicating that auxiliary of SExpr is not yet computaed
           });
        return std::make_optional<RegionOfTokens>(firstSExprBegin,firstSExprEnd);
    }

    LispLexer::OptRegionOfTokens LispLexer::TokenizeNext(const LispToken *token) noexcept {
#ifndef NDEBUG
        assert(token->Kind == LispTokenKind::LeftParenthesis);
#endif
        const SExprIndex& currentSExprIndex = _sexprIndices[token->IndexInSpecialStream];
        if (currentSExprIndex.Next >= _sexprIndices.Size()) {
            return std::nullopt;
        }
        const std::uint32_t nextSExprPos = currentSExprIndex.Next;
        const SExprIndex& nextSExpr = _sexprIndices[nextSExprPos];
        const LispToken* nextSExprBegin = nullptr;
        const std::uint32_t optSegOrCommentIndex = currentSExprIndex.Close+1;
        const char optSegOrComment = *(_text.data() + optSegOrCommentIndex);

        if (IsComment(optSegOrComment) or IsFragment(optSegOrComment)) {
            _auxiliaries.EmplaceBack({optSegOrCommentIndex,nextSExpr.Open-optSegOrCommentIndex});
            nextSExprBegin = _tokens.EmplaceBack(LispToken{
                _text.data()+nextSExpr.Open,
                nextSExpr.OpenLine,
                1,
                static_cast<std::uint32_t>(_auxiliaries.Size()-1),
                nextSExpr.OpenColumn,
                nextSExprPos,
                LispTokenKind::LeftParenthesis,
                1
            });
        }
        else {
            nextSExprBegin = _tokens.EmplaceBack(LispToken{
                _text.data()+nextSExpr.Open,
                nextSExpr.OpenLine,
                1,
                0,
                nextSExpr.OpenColumn,
                nextSExprPos,
                LispTokenKind::LeftParenthesis,
                std::numeric_limits<std::uint8_t>::max()
            });
        }

        const LispToken* const nextSExprEnd = _tokens.EmplaceBack(LispToken{
               _text.data()+nextSExpr.Close,
               nextSExpr.CloseLine,
               1,
               0,
               nextSExpr.CloseColumn,
               nextSExprPos,
               LispTokenKind::RightParenthesis,
               std::numeric_limits<std::uint8_t>::max()
           });

        return std::make_optional<RegionOfTokens>(nextSExprBegin,nextSExprEnd);
    }

    LispLexer::OptRegionOfTokens LispLexer::TokenizeSExpr(const LispToken *begin,const bool csEmptySExpr) noexcept {
        return TokenizeSExprCore(begin,csEmptySExpr);
    }

    LispLexer::OptRegionOfTokens LispLexer::GetTokenAuxiliary(const LispToken* token) noexcept {
        const auto auxiliaryLength = token->AuxiliaryLength;
        if (auxiliaryLength == 0 or auxiliaryLength == std::numeric_limits<std::uint8_t>::max()) {
            return std::nullopt;
        }
        const auto auxiliaryIndex = token->AuxiliaryIndex;
        const auto auxiliaryTokenBegin = _tokens.Size();
        for (int i=0;i<auxiliaryLength;++i) {
            const auto [at, length] = _auxiliaries[auxiliaryIndex+i];
            _tokens.EmplaceBack(LispToken{
                _text.data()+at,
                std::numeric_limits<std::uint32_t>::max(),
                length,
                std::numeric_limits<std::uint32_t>::max(),
                std::numeric_limits<std::uint32_t>::max(),
                0,
                LispTokenKind::Fragment,
                0
            });
        }
        return std::make_pair(&_tokens[auxiliaryTokenBegin],&_tokens[auxiliaryTokenBegin+auxiliaryLength-1]);
    }

    BumpVector<Diagnostic::LispDiagnostic> &LispLexer::GetDiagnostics() noexcept {
        return _diagnostics;
    }

    std::size_t LispLexer::GetFileSize() const noexcept {
        return _text.size()-32;
    }

    const char * LispLexer::GetTextData() const noexcept {
        return _text.data();
    }

    void LispLexer::Reuse() noexcept {
        _reused = true;
        _textStreamPos = 0;
        _blocks.Reuse();
        _sexprIndices.Reuse();
        _auxiliaries.Reuse();
    }

    std::wstring_view LispLexer::GetFilePath() const noexcept {
        return _filePath;
    }

    NODISCARD ALWAYS_INLINE PURE std::uint32_t ComputeNonEscapingDoubleQuotes(const std::uint32_t backslashMask,
       const std::uint32_t doubleQuoteMask) {
        const auto escapeCheckMask = backslashMask << 1;
        const auto oddEscapeCheckMask = escapeCheckMask | 0xAAAAAAAAU;
        const auto escapeDetectionMask = oddEscapeCheckMask - backslashMask;
        const auto escapeAndNonEscapeMask = escapeDetectionMask ^ 0xAAAAAAAAU;
        return ~(escapeAndNonEscapeMask ^ backslashMask) & doubleQuoteMask;
    }

    ALWAYS_INLINE void LispLexer::Classify() {
        const auto address = reinterpret_cast<const std::uint8_t*>(_text.data());
        std::uint32_t prevTileEndWithOddBackslash = 0x0;
        const std::size_t textSize = _text.size();
        const std::size_t vectorAlignedSize = textSize & ~31u;

        alignas(32) static const Vector256 sexprAndOpsTable { //not all operators are covered only those fallen within the targeted range
            '=','/','.','-',CommaChar,'+','*',')',
            '(','\'','&','%',DollarChar,HashChar,0,'!',
            '=','/','.','-',CommaChar,'+','*',')',
            '(','\'','&','%',DollarChar,HashChar,0,'!'
        };

        alignas(32) static const Vector256 otherOpsAndStructTable {
            AtChar,TildaChar,0,0,0,0,LeftBracketChar,RightBracketChar,
            QuasiColumnChar,0,0,0,0,0,ColumnChar,'|',
            AtChar,TildaChar,0,0,0,0,LeftBracketChar,RightBracketChar,
            QuasiColumnChar,0,0,0,0,0,ColumnChar,'|'
        };

        alignas(32) static const Vector256 fragmentsTable {
            ' ',0,0,0,0,0,0,0,
            0,'\t','\n',0,0,'\r',0,0,
            ' ',0,0,0,0,0,0,0,
            0,'\t','\n',0,0,'\r',0,0,
        };

        alignas(32) static const Vector256 digitsTable {
            '0','1','2','3','4','5','6','7',
            '8','9',0,0,0,0,0,0,
            '0','1','2','3','4','5','6','7',
            '8','9',0,0,0,0,0,0,
        };

        alignas(32) static const Vector256 smallIdentifierTable {
            'p','a','b','c','d','e','f','g',
            'h','i','j','k','l','m','n','o',
            'p','a','b','c','d','e','f','g',
            'h','i','j','k','l','m','n','o',
        };

        alignas(32) static const Vector256 smallIdentifierTable2 {
            0,'q','r','s','t','u','v','w',
            'x','y','z',0,0,0,0,0,
            0,'q','r','s','t','u','v','w',
            'x','y','z',0,0,0,0,0,
        };

        alignas(32) static const Vector256 capitalIdentifierTable {
            'P','A','B','C','D','E','F','G',
            'H','I','J','K','L','M','N','O',
            'P','A','B','C','D','E','F','G',
            'H','I','J','K','L','M','N','O',
        };

        alignas(32) static const Vector256 capitalIdentifierTable2 {
            0,'Q','R','S','T','U','V','W',
            'X','Y','Z',0,0,DashInId,0,'_',
            0,'Q','R','S','T','U','V','W',
            'X','Y','Z',0,0,DashInId,0,'_'
        };

        std::ptrdiff_t i = 0;
        for (; i < vectorAlignedSize ; i+=sizeof(Vector256) ) {
            const Vector256 fetchedChars = Avx2::LoadFromAddress(address,i);
            //string literal matching
            const Vector256 doubleQuotationEquality =  Avx2::CompareEqual(fetchedChars,Avx2::Propagate('\"'));
            const Vector256 backwardSlashEquality = Avx2::CompareEqual(fetchedChars,Avx2::Propagate('\\'));
            const std::uint32_t doubleQuoteMask = Avx2::MoveMask(doubleQuotationEquality);
            const std::uint32_t backSlashMask = Avx2::MoveMask(backwardSlashEquality);
            //sexpr parentheses matching
            const Vector256 hashedSexprChars = Avx2::SubtractSaturated(Avx2::Propagate(0x30),fetchedChars);
            const Vector256 lookedSexprAndOpsChars = Avx2::ShuffleBytes(sexprAndOpsTable,hashedSexprChars);
            const Vector256 sexprAndOpsChars = Avx2::CompareEqual(lookedSexprAndOpsChars,fetchedChars);
            std::uint32_t sexprAndOpsMask = Avx2::MoveMask(sexprAndOpsChars);
            //other operators and structural matching
            const Vector256 hashedOtherOpsAndStructSymbols = Avx2::Custom::RightShift8<2>(fetchedChars);
            const Vector256 lookedOtherOpsAndStructSymbols = Avx2::ShuffleBytes(otherOpsAndStructTable,hashedOtherOpsAndStructSymbols);
            const Vector256 matchingOtherOpsAndStruct = Avx2::CompareEqual(lookedOtherOpsAndStructSymbols,fetchedChars);
            sexprAndOpsMask |= Avx2::MoveMask(matchingOtherOpsAndStruct);
            //numbers
            const Vector256 lookedDigits = Avx2::ShuffleBytes(digitsTable,fetchedChars);
            const Vector256 digits = Avx2::CompareEqual(lookedDigits,fetchedChars);
            const std::uint32_t digitsMask = Avx2::MoveMask(digits);
            //identifiers
            const Vector256 lookedSmallIdentifier = Avx2::ShuffleBytes(smallIdentifierTable,fetchedChars);
            const Vector256 lookedSmallIdentifier2 = Avx2::ShuffleBytes(smallIdentifierTable2,fetchedChars);
            const Vector256 lookedCapitalIdentifier = Avx2::ShuffleBytes(capitalIdentifierTable,fetchedChars);
            const Vector256 lookedCapitalIdentifier2 = Avx2::ShuffleBytes(capitalIdentifierTable2,fetchedChars);
            const Vector256 lookedSmallIdentifierChars = Avx2::CompareEqual(lookedSmallIdentifier,fetchedChars);
            const Vector256 lookedSmallIdentifier2Chars = Avx2::CompareEqual(lookedSmallIdentifier2,fetchedChars);
            const Vector256 lookedCapitalIdentifierChars = Avx2::CompareEqual(lookedCapitalIdentifier,fetchedChars);
            const Vector256 lookedCapitalIdentifier2Chars = Avx2::CompareEqual(lookedCapitalIdentifier2,fetchedChars);
            //lexer will match digits mask before identifier mask so it doesn't get mis-tokenized
            const Vector256 lookedIdentifier = Avx2::Or(digits,
                Avx2::Or( //balancing is important here as it optimizes data dependency
                Avx2::Or(lookedCapitalIdentifierChars,lookedCapitalIdentifier2Chars),
                Avx2::Or(lookedSmallIdentifierChars,lookedSmallIdentifier2Chars)));
            const std::uint32_t identifierMask = Avx2::MoveMask(lookedIdentifier);
            //fragmentation chars matching
            //no need to hash anything '_mm256_shuffle_epi8' implicit bitwise-and with 0x0F is enough for classification here
            const Vector256 lookedFragmentChars = Avx2::ShuffleBytes(fragmentsTable,fetchedChars);
            const Vector256 fragmentChars = Avx2::CompareEqual(lookedFragmentChars,fetchedChars);
            const Vector256 newLines = Avx2::CompareEqual(fetchedChars,Avx2::Propagate('\n'));
            const std::uint32_t newLineMask = Avx2::MoveMask(newLines);
            const std::uint32_t fragmentMask = Avx2::MoveMask(fragmentChars);
            //pushing result
            const std::uint32_t nonEscapingQuotationMask = ComputeNonEscapingDoubleQuotes(backSlashMask,doubleQuoteMask^prevTileEndWithOddBackslash);
            prevTileEndWithOddBackslash = std::countl_one(backSlashMask) & 0x00000001U;
            _blocks.EmplaceBack(TokenizationBlock{
                .FragmentsMask = fragmentMask,
                .SExprAndOpsMask = sexprAndOpsMask,
                .DigitsMask = digitsMask,
                .StringLiteralsMask = nonEscapingQuotationMask,
                .NewLines = newLineMask,
                .IdentifierMask = identifierMask
            });
        }

        _blocks.EmplaceBack(TokenizationBlock{
            .NewLines = 1U //this is a trick to remove the check that next block is not a sentinel block (NULL)
        });
    }

    ALWAYS_INLINE LispLexer::OptRegionOfTokens LispLexer::TokenizeSExprCore(const LispToken *begin,
        const bool csEmptySExpr) noexcept {
#ifndef NDEBUG
        assert(begin->Kind == LispTokenKind::LeftParenthesis);
#endif
        const SExprIndex& parentSExprIndex = _sexprIndices[begin->IndexInSpecialStream];
        _textStreamPos = parentSExprIndex.Open+1;
        _line = begin->Line;
        _column = begin->Column + 1;
        const std::uint32_t endPos = parentSExprIndex.Close;
        std::uint32_t peekSExprIndex = begin->IndexInSpecialStream+1;
        const auto startTokensSize = _tokens.Size();
        const LispToken* atomsBegin = _tokens.At(_tokens.Size());
        const char* text = _text.data();
        char ch = CurrentChar();
        std::uint8_t fragLength = 0;
        while (_textStreamPos < endPos) {
            if (ch == '(') {
                const auto& currentSExprIndex = _sexprIndices[peekSExprIndex];
                _tokens.EmplaceBack(LispToken{text+currentSExprIndex.Open,
                    currentSExprIndex.OpenLine,
                    1U,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    currentSExprIndex.OpenColumn,
                    peekSExprIndex,
                    LispTokenKind::LeftParenthesis,
                    fragLength
                });
                _tokens.EmplaceBack(LispToken{text+currentSExprIndex.Close,
                      currentSExprIndex.CloseLine,
                      1U,
                      0,
                      currentSExprIndex.CloseColumn,
                      0,
                      LispTokenKind::RightParenthesis,
                       std::numeric_limits<std::uint8_t>::max()
                });
                peekSExprIndex = currentSExprIndex.Next;
                _textStreamPos = currentSExprIndex.Close+1; //skip to first char after SExpr
                _line = currentSExprIndex.OpenLine;
                _column = currentSExprIndex.OpenColumn;
                if (_textStreamPos >= endPos) {
                    goto loopExit;
                }
                ch = CurrentChar(); //due to reassigning '_textStreamPos' above
                fragLength = 0;
                continue;
            }

            const std::size_t blockIndex = _textStreamPos >> 5;
            const TokenizationBlock& block = _blocks[blockIndex];
            const std::uint8_t posInBlock = OffsetInBlock();
            //comments
            if (const auto targetNewlineBlock = block.NewLines >> (posInBlock + 1); IsComment(ch)){
                const auto [startOfComment,endOfCommentOffset] = FetchCommentRegion(targetNewlineBlock,posInBlock);
                ch = SkipToCharAtWithoutColumn(endOfCommentOffset);
                ++_line;
                _column = 1;
                _auxiliaries.EmplaceBack(AuxiliaryIndex{startOfComment,endOfCommentOffset});
                ++fragLength;
                continue;
            }
            //fragments
            if (const std::uint32_t fragmentsBlock = block.FragmentsMask >> posInBlock;fragmentsBlock & 1U)[[likely]]{
                const TokenizationBlock* currentBlock = &block;
                const auto startLine = _line;
                auto [startOfFragment,endOfFragmentOffset] = FetchFragmentRegion(fragmentsBlock,posInBlock,currentBlock);
                ch = SkipToCharAtWithoutColumn(endOfFragmentOffset);
                if (_line != startLine) {
                    currentBlock = _blocks.At(_textStreamPos >> 5);
                    const std::uint8_t offsetInBlock = OffsetInBlock(); //position where last fragment is
                    const std::uint32_t posOfLastNewLine =
                        (TokensInBlock- std::countl_zero(~(0xFFFFFFFFU << offsetInBlock) & currentBlock->NewLines))
                        & TokensInBlockBoundary;
                    _column = std::countr_one(currentBlock->FragmentsMask >> posOfLastNewLine) + 1;
                }
                else {
                    _column += endOfFragmentOffset;
                }
                _auxiliaries.EmplaceBack(AuxiliaryIndex{startOfFragment,endOfFragmentOffset});
                ++fragLength;
                continue;
            }
            //sexpr and operators (most of them)
            if (const std::uint32_t sexprOpsBlock = block.SExprAndOpsMask >> posInBlock; sexprOpsBlock & 1U) [[likely]]{
                _tokens.EmplaceBack(LispToken{text+_textStreamPos,
                    _line,
                    1U,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    _column,
                    0,
                    static_cast<LispTokenKind>(CurrentChar()),
                    fragLength
                });
                ch = NextChar();
            }
            //reals
            else if (const auto digitsBlock = block.DigitsMask >> posInBlock; digitsBlock & 1U) {
                const auto [startOfReal,endOfRealOffset] = TokenizeRealBlue(digitsBlock,posInBlock,&block);
                _tokens.EmplaceBack(LispToken{text+startOfReal,
                    _line,
                    endOfRealOffset,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    _column,
                    0,
                    LispTokenKind::RealLiteral,
                    fragLength
                });
                _column += endOfRealOffset;
                ch = CurrentChar();
            }
            //identifiers
            else if (const std::uint32_t idBlock = block.IdentifierMask >> posInBlock; idBlock & 1U) [[likely]]{
                const auto [startOfId,endOfIdOffset] = FetchIdentifierRegion(idBlock,posInBlock);
                const LispTokenKind keywordOrId = IsKeyword(std::string_view{text+startOfId,endOfIdOffset});
                _tokens.EmplaceBack(LispToken{text+startOfId,
                    _line,
                    endOfIdOffset,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    _column,
                    0,
                    keywordOrId,
                    fragLength
                });
                ch = SkipToCharAt(endOfIdOffset);
            }
            //string literals
            else if (const auto stringBlock = block.StringLiteralsMask >> posInBlock; stringBlock & 1U) {
                const auto [startOfString,endOfStringOffset] = FetchStringRegion(stringBlock,posInBlock);
                _tokens.EmplaceBack(LispToken{text+startOfString,
                   _line,
                   endOfStringOffset,
                   static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                   _column,
                   0,
                   LispTokenKind::StringLiteral,
                   fragLength
               });
                ch = SkipToCharAt(endOfStringOffset);
            }
            //rest of operators
            else if (IsOperator(ch)) {
                TokenizeOperatorsOrStructural(fragLength);
                ch = CurrentChar();
            }
            else if (IsEndOfFile()) {
                _tokens.EmplaceBack(LispToken{"\0",
                    _line,
                    1,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    _column,
                    0,
                    LispTokenKind::EndOfFile,
                    fragLength
                });
                break;
            }
            else {
                _tokens.EmplaceBack(LispToken{_text.data()+_textStreamPos,
                    _line,
                    1U,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    _column,
                    0,
                    LispTokenKind::Invalid,
                    fragLength
                });
                ch = NextChar();
            }
            fragLength = 0;
        }
        loopExit:
        if (startTokensSize == _tokens.Size() && !csEmptySExpr) { //empty SExpr
            return std::nullopt;
        }
        const LispToken* atomsEnd = &_tokens.Back();
        //SExpr closing parenthesis (which is always next to the opening parenthesis in the token stream);
        auto*const end = const_cast<LispToken *>(begin+1);
        //update SExpr closing parenthesis auxiliary info because it cannot be computed before this point
        end->AuxiliaryIndex = static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength);
        end->AuxiliaryLength = fragLength;
        return std::make_optional<RegionOfTokens>(atomsBegin,atomsEnd);
    }

    ALWAYS_INLINE char LispLexer::NextChar() noexcept {
        ++_column;
        return _text[++_textStreamPos];
    }

    ALWAYS_INLINE char LispLexer::NextCharWithoutColumn() noexcept {
        return _text[++_textStreamPos];
    }

    ALWAYS_INLINE char LispLexer::CurrentChar() const noexcept {
        return _text[_textStreamPos];
    }

    ALWAYS_INLINE char LispLexer::SkipToCharAt(const std::size_t offset) noexcept {
        _column += offset;
        return _text[_textStreamPos += offset];
    }

    ALWAYS_INLINE char LispLexer::SkipToCharAtWithoutColumn(const std::size_t offset) noexcept {
        return _text[_textStreamPos += offset];
    }

    ALWAYS_INLINE bool LispLexer::TokenizeBlue() {
        using namespace Diagnostic;
        Classify();
        MonoBumpVector<std::uint32_t> stack{static_cast<std::uint32_t>(AlignToPowOfTow(_text.size()/2))};
        char ch = CurrentChar();
        while (true) {
            switch (ch) {
                case '(': {
                    stack.EmplaceBack(static_cast<std::uint32_t>(_sexprIndices.Size()));
                    _sexprIndices.EmplaceBack(SExprIndex{_textStreamPos,_line,_column});
                    ch = NextChar();
                    continue;
                }
                case ')': {
                    if (!stack.Empty()) [[likely]]{
                        const auto sexprIndexPos = stack.Back();
                        stack.PopBack();
                        auto& index = _sexprIndices[sexprIndexPos];
                        index.Close = _textStreamPos;
                        index.CloseLine = _line;
                        index.CloseColumn = _column;
                        index.Next = static_cast<std::uint32_t>(_sexprIndices.Size());
                    }
                    else {
                        _diagnostics.EmplaceBack(DiagnosticFactory::NoMatchingOpenParenthesis(_filePath,_line,_column,
                            LispToken{")",_line,1,0,_column,0,LispTokenKind::RightParenthesis}));
                    }
                    ch = NextChar();
                    continue;
                }
                default:
                    break;
            }
            const std::size_t blockIndex = _textStreamPos >> 5;
            const TokenizationBlock& block = *_blocks.At(blockIndex);
            const std::uint8_t posInBlock = OffsetInBlock();

            //comments
            if (const auto targetNewlineBlock = block.NewLines >> (posInBlock + 1); IsComment(ch)) {
                const auto [_,endOfCommentOffset] = FetchCommentRegion(targetNewlineBlock,posInBlock);
                ch = SkipToCharAtWithoutColumn(endOfCommentOffset);
                ++_line;
                _column = 1;
                continue;
            }
            //fragments
            if (const std::uint32_t fragmentsBlock = block.FragmentsMask >> posInBlock;fragmentsBlock & 1U) {
                const TokenizationBlock* currentBlock = &block;
                const auto startLine = _line;
                auto [startOfRegion,lengthOfRegion] = FetchFragmentRegion(fragmentsBlock,posInBlock,currentBlock);
                ch = SkipToCharAtWithoutColumn(lengthOfRegion);
                if (_line != startLine) {
                    currentBlock = _blocks.At(_textStreamPos >> 5);
                    const std::uint8_t offsetInBlock = OffsetInBlock(); //position where last fragment is
                    const std::uint32_t posOfLastNewLine =
                        (TokensInBlock- std::countl_zero(~(0xFFFFFFFFU << offsetInBlock) & currentBlock->NewLines))
                        & TokensInBlockBoundary;
                    _column = std::countr_one(currentBlock->FragmentsMask >> posOfLastNewLine) + 1;
                }
                else {
                    _column += lengthOfRegion;
                }
                continue;
            }
            //sexpr and operators (most of them)
            if (const std::uint32_t sexprOpsBlock = block.SExprAndOpsMask >> posInBlock; sexprOpsBlock & 1U) {
                ch = NextChar();
            }
            //digits
            else if (const auto digitsBlock = block.DigitsMask >> posInBlock; digitsBlock & 1U) [[likely]]{
                const auto [startOfDigit,endOfDigitOffset] = TokenizeRealBlue(digitsBlock,posInBlock,&block);
                _column += endOfDigitOffset;
                ch = CurrentChar();
            }
            //identifiers
            else if (const std::uint32_t idBlock = block.IdentifierMask >> posInBlock; idBlock & 1U) [[likely]]{
                const auto [startOfId,endOfIdOffset] = FetchIdentifierRegion(idBlock,posInBlock);
                ch = SkipToCharAt(endOfIdOffset);
            }
            //string literals
            else if (const auto stringBlock = block.StringLiteralsMask >> posInBlock; stringBlock & 1U) {
                const auto [startOfString,endOfStringOffset] = FetchStringRegion(stringBlock,posInBlock);
                ch = SkipToCharAt(endOfStringOffset);
            }
            //rest of operators
            else if (IsOperator(ch)) {
                const auto [_,opLength] = TokenizeOperatorsOrStructuralBlue();
                ch = SkipToCharAt(opLength);
            }
            else if (IsEndOfFile()) {
                break;
            }
            else {
                _diagnostics.EmplaceBack(DiagnosticFactory::UnrecognizedToken(_filePath,
                    _line,
                    _column,
                    LispToken{_text.data()+_tokenStreamPos,_line,1,0,_column,0,LispTokenKind::Invalid}));
                ch = NextChar();
            }

            if (stack.Empty()) [[unlikely]]{
                //there cannot be any top level token other than S-expression tokens aka '(' and ')'
                _diagnostics.EmplaceBack(DiagnosticFactory::UnexpectedTopLevelToken(_filePath,
                    _line,
                    _column));
            }
        }

        auto bufferSize = stack.Size();
        while (bufferSize > 0) {
            const auto sexprIndicesPos = *stack.At(--bufferSize);
            const auto sexprIndex = _sexprIndices[sexprIndicesPos];
            _diagnostics.EmplaceBack(DiagnosticFactory::NoMatchingCloseParenthesis(_filePath,
                sexprIndex.OpenLine,
                sexprIndex.OpenColumn,
                LispToken{")"}
                ));
        }

        const auto noError = std::all_of(_diagnostics.begin(),
            _diagnostics.end(),
            [](const LispDiagnostic& diagnostic) {return diagnostic.GetSeverity() != Severity::Error;});

        _tokenized = true;
        _reused = false;
        return noError && _diagnostics.Empty();
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::TokenizeRealBlue(std::uint32_t startingBlock,
        std::uint8_t posInBlock,
        const TokenizationBlock* currentBlock) noexcept {
        std::uint32_t realLength = 0;
        auto [realStart,realInitLength] = FetchDigitRegion(startingBlock,posInBlock);
        char ch = SkipToCharAtWithoutColumn(realInitLength);
        if (ch != '.') [[likely]]{
            return {realStart,realInitLength};
        }
        realLength += realInitLength+1;/*+1 for the floating point '.'*/
        currentBlock = &_blocks[_textStreamPos++ >> TokensInBlockPopCnt];
        startingBlock = currentBlock->DigitsMask;
        posInBlock = _textStreamPos & 0x1F;
        auto [mantissaStart,mantissaLength] = FetchDigitRegion(startingBlock >> posInBlock,posInBlock);
        realLength += mantissaLength;
        ch = SkipToCharAtWithoutColumn(mantissaLength);

        if (ch == 'e' or ch == 'E') {
            ++realLength;
            ch = NextCharWithoutColumn();
            if (ch == '+' or ch == '-') {
                ++realLength;
                ch = NextCharWithoutColumn();
            }
            if (!IsDecimal(ch)) {
                _diagnostics.EmplaceBack(Diagnostic::DiagnosticFactory::MalformedFloatingPointLiteral(
                    _filePath,_line,_column+realLength,_text.substr(realStart, realLength) ));
                return {realStart,realInitLength};
            }
            currentBlock = &_blocks[_textStreamPos >> TokensInBlockPopCnt];
            startingBlock = currentBlock->DigitsMask;
            posInBlock = _textStreamPos & 0x1F;
            auto [_,exponentLength] = FetchDigitRegion(startingBlock >> posInBlock,posInBlock);
            realLength += exponentLength;
            (void)SkipToCharAtWithoutColumn(exponentLength);
        }
        return {realStart,realLength};
    }

    ALWAYS_INLINE LispLexer::StaticTokenRegion LispLexer::TokenizeOperatorsOrStructuralBlue() noexcept {
        const char ch = CurrentChar();
        switch (ch) {
            case '<': {
                switch (const char nextChar [[maybe_unused]] = _text[_textStreamPos+1]) {
                    case '=':
                        return {"<=",2};
                    case '<':
                        return {"<<",2};
                    default:
                        return {"<",1};
                }
            }
            case '>':{
                switch (const char nextChar [[maybe_unused]] = _text[_textStreamPos+1]) {
                    case '=':
                        return {">=",2};
                    case '>':
                        return {">>",2};
                    default:
                        return {">",1};
                }
            }

            case '\\':
            case '|':
            case '^':
#ifdef EnableHash
            case '#':
#endif
#ifdef EnableComma
            case ',':
#endif
#ifdef EnableBrackets
            case '[':
            case ']':
#endif
#ifdef EnableQuasiColumn
            case '`':
#endif
#ifdef EnableColumn
            case ':':
#endif
#ifdef EnableAtSign
            case '@':
#endif
#ifdef EnableBenjamin
            case '$':
#endif
#ifdef EnableTilda
            case '~':
#endif
            default:
                return {_text.data()+_textStreamPos,1};
        }
    }

    ALWAYS_INLINE bool LispLexer::IsOperator(const char c) noexcept {
        switch (c) {
            case '^':
            case '|':
            case '<':
            case '>':
            case '\\':
                return true;
            default:
                return false;
        }
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::FetchStringRegion(std::uint32_t startingBlock,const std::uint8_t posInBlock) noexcept {
        const std::uint32_t startOfRegion = _textStreamPos;
        startingBlock = startingBlock >> 1u; //skip the start of string '"'
        std::uint32_t endOfRegion = std::countr_zero(startingBlock);//+1 due to the right shift above
        std::uint32_t pos = startOfRegion;
        //if the starting block doesn't have the terminating char of the region, then we should subtract the partition
        //to get the right offset
        std::uint32_t optionalOffset = 0;
        if (endOfRegion == 0) { //empty string
            return {startOfRegion,2};
        }
        if (endOfRegion == TokensInBlock) {
            optionalOffset = posInBlock + 1; //+1 to count for skipping starting literal
            pos = ((pos & ~TokensInBlockBoundary) + TokensInBlock); //jump to next tokenization block
        }
        std::uint32_t count = 1U;
        while ((endOfRegion & TokensInBlockBoundary) == 0 && count) {
            const TokenizationBlock* nextBlock = TokenizationBlockAt(pos);
            if (nextBlock == nullptr) { //non terminating string literal
                _diagnostics.EmplaceBack(Diagnostic::DiagnosticFactory::UnterminatedStringLiteral(
                       _filePath,_line,_column));
                return {startOfRegion,static_cast<std::uint32_t>(_text.size() - startOfRegion - 1)};
            }
            count = std::countr_zero(nextBlock->StringLiteralsMask);
            endOfRegion += count;
            pos += count;
        }
        //+1 due to the right shift above and another +1 to take the terminating double quote char
        endOfRegion = endOfRegion + 2 - optionalOffset;
        return {startOfRegion,endOfRegion};
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::FetchCommentRegion(const std::uint32_t startingBlock,const std::uint8_t posInBlock) noexcept {
        const std::uint32_t startOfRegion = _textStreamPos;
        std::uint32_t endOfRegion = std::countr_zero(startingBlock);
        std::uint32_t pos = startOfRegion;
        std::uint32_t optionalOffset = 0;
        if (endOfRegion == 0) {
            return {startOfRegion,2};
        }
        if (endOfRegion == TokensInBlock) {
            optionalOffset = posInBlock + 1; //+1 to count for skipping the start of comment region
            pos = ((pos & ~TokensInBlockBoundary) + TokensInBlock);//jump to the next tokenization block
        }
        std::uint32_t count = 1U;
        while ((endOfRegion & TokensInBlockBoundary) == 0 && count) {
            const TokenizationBlock* nextBlock = TokenizationBlockAt(pos);
            count = std::countr_zero(nextBlock->NewLines);
            endOfRegion += count;
            pos += count;
        }
        //+1 due to starting block skipping comment start ';'.
        //the another +1 to take \n which terminates single line comment (also help in reducing number of parse tree nodes)
        endOfRegion = endOfRegion + 2 - optionalOffset;
        return {startOfRegion,endOfRegion};
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::FetchFragmentRegion(const std::uint32_t startingBlock,
        std::uint8_t posInBlock,
        const TokenizationBlock* currentBlock) noexcept {

        const std::uint32_t startOfRegion = _textStreamPos;
        const std::uint32_t block = startingBlock;
        std::uint32_t offset = std::countr_one(block);
        //assuming we are at tokenization block N and position M in block, if starting from M till the end of the block
        //(let's call it E) if all bits are set/on then it's possible that tokenization block N+1... is a continuation
        //of fragments from current block (N), if this is the case then we go to the do-while loop! if bits between
        //M and E are not all set, then we don't have to fetch a tokenization block and we can return immediately
        if (offset + posInBlock < TokensInBlock) {
            _line += std::popcount(~(0xFFFFFFFFULL << offset) & currentBlock->NewLines >> posInBlock);
            return {startOfRegion,offset};
        }

        const std::uint32_t pos = startOfRegion+posInBlock;
        std::uint32_t fragMask = offset;
        const TokenizationBlock* nextBlock = currentBlock;
        do {
            _line += std::popcount(~(0xFFFFFFFFULL << fragMask) & nextBlock->NewLines >> posInBlock);
            nextBlock = TokenizationBlockAt(pos+offset);
            fragMask = std::countr_one(nextBlock->FragmentsMask);
            offset += fragMask;
            posInBlock = 0;
        }while ((fragMask & TokensInBlockBoundary) == 0 && fragMask);

        _line += std::popcount(~(0xFFFFFFFFULL << fragMask) & nextBlock->NewLines >> posInBlock);
        return {startOfRegion,offset};
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::FetchDigitRegion(const std::uint32_t startingBlock,
        const std::uint8_t posInBlock) noexcept {

        const std::uint32_t startOfRegion = _textStreamPos;
        const std::uint32_t block = startingBlock;
        std::uint32_t offset = std::countr_one(block);
        if (const std::uint32_t pos = posInBlock+offset; pos < TokensInBlock) [[likely]]{
            return {startOfRegion,offset};
        }
        std::uint32_t digitsMask;
        const std::uint32_t pos = startOfRegion+posInBlock;
        do{
            const TokenizationBlock* nextBlock = TokenizationBlockAt(pos+offset);
            digitsMask = std::countr_one(nextBlock->DigitsMask);
            offset += digitsMask;
        }while ((digitsMask & TokensInBlockBoundary) == 0 && digitsMask);
        return {startOfRegion,offset};
    }

    ALWAYS_INLINE LispLexer::TokenRegion LispLexer::FetchIdentifierRegion(const std::uint32_t startingBlock,
        const std::uint8_t posInBlock) noexcept {

        const std::uint32_t startOfRegion = _textStreamPos;
        const std::uint32_t block = startingBlock;
        std::uint32_t offset = std::countr_one(block);
        //if the last bit in pos is set then it's possible that the remaining parts of the identifier are
        //in blocks N+K (where K is >= 1)
        if (const std::uint32_t pos = posInBlock+offset; pos < TokensInBlock) [[likely]]{
            return {startOfRegion,offset};
        }
        std::uint32_t idMask;
        const std::uint32_t pos = startOfRegion+posInBlock;
        do{
            const TokenizationBlock* nextBlock = TokenizationBlockAt(pos+offset);
            idMask = std::countr_one(nextBlock->IdentifierMask);
            offset += idMask;
        }while ((idMask & TokensInBlockBoundary) == 0 && idMask);
        return {startOfRegion,offset};
    }

    ALWAYS_INLINE void LispLexer::TokenizeOperatorsOrStructural(std::uint8_t fragLength) noexcept {
        const char ch = CurrentChar();
        const std::uint32_t column = _column;
        switch (ch) {
            case '<': {
                switch (const char nextChar [[maybe_unused]] = NextChar()) {
                    case '=':
                        _tokens.EmplaceBack(LispToken{"<=",
                            _line,
                            2U,
                            static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                            column,
                            0,
                            LispTokenKind::LessThanOrEqual,
                            fragLength
                        });
                        NextChar();
                        break;
                    case '<':
                        _tokens.EmplaceBack(LispToken{"<<",
                            _line,
                            2U,
                            static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                            column,
                            0,
                            LispTokenKind::LeftBitShift,
                            fragLength
                        });
                        NextChar();
                        break;
                    default:
                        _tokens.EmplaceBack(LispToken{"<",
                           _line,
                           1U,
                           static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                           column,
                            0,
                           LispTokenKind::LessThan,
                           fragLength
                        });
                        break;
                }
                break;
            }

            case '>':{
                switch (const char nextChar [[maybe_unused]] = NextChar()) {
                    case '=':
                        _tokens.EmplaceBack(LispToken{">=",
                            _line,
                            2U,
                            static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                            column,
                            0,
                            LispTokenKind::GreaterThanOrEqual,
                            fragLength
                        });
                        NextChar();
                        break;
                    case '>':
                        _tokens.EmplaceBack(LispToken{">>",
                            _line,
                            2U,
                            static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                            column,
                            0,
                            LispTokenKind::RightBitShift,
                            fragLength
                        });
                        NextChar();
                        break;
                    default:
                        _tokens.EmplaceBack(LispToken{">",
                           _line,
                           1U,
                           static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                           column,
                            0,
                           LispTokenKind::GreaterThan,
                           fragLength
                        });
                        break;
                }
                break;
            }

            case '\\'://backward slash in most dialects doesn't appear out of string literal (to escape chars),
                      //but still some dialects may have other purposes for it
            case '|':
            case '^':
#ifdef EnableHash
            case '#':
#endif
#ifdef EnableComma
            case ',':
#endif
#ifdef EnableBrackets
            case '[':
            case ']':
#endif
#ifdef EnableQuasiColumn
            case '`':
#endif
#ifdef EnableColumn
            case ':':
#endif
#ifdef EnableAtSign
            case '@':
#endif
#ifdef EnableBenjamin
            case '$':
#endif
#ifdef EnableTild
            case '~':
#endif

                _tokens.EmplaceBack(LispToken{_text.data()+_textStreamPos,
                    _line,
                    1U,
                    static_cast<std::uint32_t>(_auxiliaries.Size()-fragLength),
                    column,
                    0,
                    static_cast<LispTokenKind>(ch),
                    fragLength
                });
                NextChar();
                break;
            default:
               break;
        }
    }

    ALWAYS_INLINE TokenizationBlock * LispLexer::TokenizationBlockAt(const std::uint32_t pos) noexcept {
        const std::size_t nextBlockIndex = (pos & ~TokensInBlockBoundary) >> TokensInBlockPopCnt;
        if (nextBlockIndex >= _blocks.Size()) {
            return nullptr;
        }
        return _blocks.At(nextBlockIndex);
    }

    ALWAYS_INLINE std::uint8_t LispLexer::OffsetInBlock() const noexcept {
        return _textStreamPos & 0x1Fu;
    }

    ALWAYS_INLINE bool LispLexer::IsEndOfFile() const noexcept {
        return _text[_textStreamPos] == EOF || _text[_textStreamPos] == '\0' || _textStreamPos >= _text.size();
    }

    ALWAYS_INLINE bool LispLexer::IsDecimal(const char c) noexcept {
        return c >= '0' && c <= '9';
    }

    ALWAYS_INLINE LispTokenKind LispLexer::IsKeyword(const std::string_view identifier) noexcept {
        //we do SWAR (SIMD-Within-A-Register) and match the value against compile-time computed values
        //to check if an identifier is in fact a keyword
        if (identifier.size() <= 8) {
            constexpr auto qwordStrEvaluator = [](const char* s, const std::size_t size) consteval {
                std::array<char, sizeof(std::uintptr_t)> arr{};
                for (std::size_t i = 0; i < size && i < sizeof(std::uintptr_t); ++i) {
                    arr[i] = s[i];
                }
                return std::bit_cast<std::uintptr_t>(arr);
            };

            constexpr auto ConstStrlen = [](const char* s) consteval {
                return std::char_traits<char>::length(s);
            };

            const auto op = *reinterpret_cast<const std::uintptr_t *>(identifier.data());
            const auto operand = std::bit_cast<std::uintptr_t>( op &
                ~(std::numeric_limits<std::uintptr_t>::max() << (identifier.size() << 3U)));

            constexpr std::uintptr_t let = qwordStrEvaluator("let",3);
            constexpr std::uintptr_t andKeyword = qwordStrEvaluator("and",3);
            constexpr std::uintptr_t notKeyword = qwordStrEvaluator("not",3);
            constexpr std::uintptr_t orKeyword = qwordStrEvaluator("or",2);
            constexpr std::uintptr_t ifKeyword = qwordStrEvaluator("if",2);
            constexpr std::uintptr_t funcKeyword = qwordStrEvaluator(FuncKeyword,ConstStrlen(FuncKeyword));
            constexpr std::uintptr_t macroKeyword = qwordStrEvaluator(MacroKeyword,ConstStrlen(MacroKeyword));
            constexpr std::uintptr_t varKeyword = qwordStrEvaluator(VarKeyword,ConstStrlen(VarKeyword));
            constexpr std::uintptr_t lambdaKeyword = qwordStrEvaluator(LambdaKeyword,ConstStrlen(LambdaKeyword));
            constexpr std::uintptr_t trueLiteral = qwordStrEvaluator(TrueLiteral,ConstStrlen(TrueLiteral));
            constexpr std::uintptr_t falseLiteral = qwordStrEvaluator(FalseLiteral,ConstStrlen(FalseLiteral));
            constexpr std::uintptr_t nilKeyword = qwordStrEvaluator(NilKeyword,ConstStrlen(NilKeyword));

            switch (operand) {
                case let:
                    return LispTokenKind::Let;
                case andKeyword:
                    return LispTokenKind::LogicalAnd;
                case notKeyword:
                    return LispTokenKind::Not;
                case orKeyword:
                    return LispTokenKind::LogicalOr;
                case ifKeyword:
                    return LispTokenKind::If;
                case funcKeyword:
                    if constexpr (ConstStrlen(FuncKeyword) <= 8)
                        return LispTokenKind::Defun;
                    else
                        return identifier == FuncKeyword ? LispTokenKind::Defun : LispTokenKind::Invalid;
                case macroKeyword:
                    if constexpr (ConstStrlen(MacroKeyword) <= 8)
                        return LispTokenKind::Defmacro;
                    else
                        return identifier == MacroKeyword ? LispTokenKind::Defmacro : LispTokenKind::Invalid;
                case varKeyword:
                    if constexpr (ConstStrlen(VarKeyword) <= 8)
                        return LispTokenKind::Defvar;
                    else
                        return identifier == VarKeyword ? LispTokenKind::Defvar : LispTokenKind::Invalid;
                case lambdaKeyword:
                    if constexpr (ConstStrlen(LambdaKeyword) <= 8)
                        return LispTokenKind::Lambda;
                    else
                        return identifier == LambdaKeyword ? LispTokenKind::Lambda : LispTokenKind::Invalid;
                case trueLiteral:
                    if constexpr (ConstStrlen(TrueLiteral) <= 8)
                        return LispTokenKind::BooleanLiteral;
                    else
                        return identifier == TrueLiteral ? LispTokenKind::BooleanLiteral : LispTokenKind::Invalid;
                case falseLiteral:
                    if constexpr (ConstStrlen(FalseLiteral) <= 8)
                        return LispTokenKind::BooleanLiteral;
                    else
                        return identifier == FalseLiteral ? LispTokenKind::BooleanLiteral : LispTokenKind::Invalid;
                case nilKeyword:
                    if constexpr (ConstStrlen(NilKeyword) <= 8)
                        return LispTokenKind::Nil;
                    else
                        return identifier == NilKeyword ? LispTokenKind::Nil : LispTokenKind::Invalid;
                default:
                    break;
            }
        }
        if (identifier == FuncKeyword) {
            return LispTokenKind::Defun;
        }
        if (identifier == MacroKeyword) {
            return LispTokenKind::Defmacro;
        }
        if (identifier == VarKeyword) {
            return LispTokenKind::Defvar;
        }
        if (identifier == LambdaKeyword) {
            return LispTokenKind::Lambda;
        }
        if (identifier == TrueLiteral) {
            return LispTokenKind::BooleanLiteral;
        }
        if (identifier == FalseLiteral) {
            return LispTokenKind::BooleanLiteral;
        }
        if (identifier == NilKeyword) {
            return LispTokenKind::Nil;
        }
        return LispTokenKind::Identifier;
    }

    bool LispLexer::IsComment(const char c) noexcept {
        return c == ';';
    }

    bool LispLexer::IsFragment(char c) noexcept {
        return c== ' ' or c == '\n' or c == '\t' or c == '\r';
    }

}




























