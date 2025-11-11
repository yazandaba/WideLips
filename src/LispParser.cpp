#include "LispParseTree.h"
#include "LispParser.h"

namespace WideLips {
    LispParser::LispParser(const std::string_view program, const bool conservative):
    _optionalAlignedFile(nullptr),
    _lexer(LispLexer::Make(program,conservative)),
    _parseNodesPool(ArenaSizeEstimate(program.size()/2,conservative)),
    _parseNodesAllocator(&_parseNodesPool),
    _endOfProgram(_parseNodesAllocator.new_object<LispAtom>(&PredefinedTokens::EndOfFile,
                LispParseNodeKind::EndOfProgram,
                nullptr,
                nullptr,
                this
            )){
    }

    LispParser::LispParser(const std::filesystem::path &filePath, const bool conservative):
    _optionalAlignedFile(AlignedFileReader::Read(filePath)),
    _lexer(LispLexer::Make(_optionalAlignedFile,filePath.native(),conservative)),
    _parseNodesPool(ArenaSizeEstimate(_lexer->GetFileSize(),conservative)),
    _parseNodesAllocator(&_parseNodesPool),
    _endOfProgram(_parseNodesAllocator.new_object<LispAtom>(&PredefinedTokens::EndOfFile,
                LispParseNodeKind::EndOfProgram,
                nullptr,
                nullptr,
                this
            )){
    }

    LispParseNodeBase* LispParser::Parse() {
        _lexer->Tokenize();
        const auto optRegion = _lexer->TokenizeFirstSExpr();
        if (!optRegion) {
            return nullptr;
        }
        const auto& [sexprBegin,sexprEnd] = *optRegion;
        return _parseNodesAllocator.new_object<LispList>(
                    sexprBegin,
                    sexprEnd,
                    nullptr,
                    nullptr,
                    nullptr,
                    this
                );
    }

    LispParseNodeBase* LispParser::Parse(const LispToken* sexprBegin,const LispToken* sexprEnd) {
        LispParseNodeBase* subExpressionsHead = nullptr;
        for (auto currentToken=sexprEnd; currentToken>=sexprBegin; --currentToken) [[likely]]{
            if (currentToken->Match(LispTokenKind::RightParenthesis)) {
                auto openParen = currentToken - 1; //skip to opening parenthesis
                subExpressionsHead =  _parseNodesAllocator.new_object<LispList>(
                    openParen,
                    currentToken--,
                    nullptr,
                    subExpressionsHead,
                    nullptr,
                    this
                );
            }
            else if (currentToken->IsOperator()) {
                subExpressionsHead = _parseNodesAllocator.new_object<LispAtom>(currentToken,
                     LispParseNodeKind::Operator,
                     subExpressionsHead,
                     nullptr,
                     this
                );
            }
            else if (currentToken->IsDialectSpecial()) [[unlikely]]{
                LispParseNodeBase* const dsNode = ParseDialectSpecial(currentToken);
                dsNode->Next = subExpressionsHead;
                subExpressionsHead = dsNode;
            }
            else if (currentToken->Match(LispTokenKind::Invalid)) [[unlikely]] {
                subExpressionsHead = _parseNodesAllocator.new_object<LispParseError>(
                    currentToken,
                    subExpressionsHead,
                    nullptr,
                    this
                );
            }
            else [[likely]]{
                subExpressionsHead = _parseNodesAllocator.new_object<LispAtom>(currentToken,
                    static_cast<LispParseNodeKind>(currentToken->Kind),
                    subExpressionsHead,
                    nullptr,
                    this
                );
            }
        }

        return subExpressionsHead;
    }

    std::wstring_view LispParser::OriginFile() const {
        return _lexer->GetFilePath();
    }

    LispLexer * LispParser::GetLexer() const {
        return _lexer.get();
    }

    LispAuxiliary * LispParser::MakeAuxiliary(const LispToken *auxBegin, const LispToken *auxEnd) {
        return _parseNodesAllocator.new_object<LispAuxiliary>(std::pair{auxBegin,auxEnd});
    }

    LispList * LispParser::MakeList(const LispToken *sexprBegin, const LispToken *sexprEnd) {
        return _parseNodesAllocator.new_object<LispList>(sexprBegin,sexprEnd,nullptr,nullptr,nullptr,this);
    }

    LispAtom * LispParser::MakeEndOfProgram() const {
        return _endOfProgram;
    }

    void LispParser::Reuse() const {
        _lexer->Reuse();
    }

    const BumpVector<Diagnostic::LispDiagnostic> &LispParser::GetDiagnostics() const {
        return _lexer->GetDiagnostics();
    }

    LispParseNodeBase* LispParser::ParseDialectSpecial(const LispToken *currentToken) {
        if (currentToken->Match(LispTokenKind::QuasiColumn) ||
            currentToken->Match(LispTokenKind::Comma) ||
            currentToken->Match(LispTokenKind::At)) {
            return _parseNodesAllocator.new_object<LispAtom>(currentToken,
                 LispParseNodeKind::Operator,
                 nullptr,
                 nullptr,
                 this);
        }

        return OnUnrecognizedToken(currentToken);
    }

    LispParseError* LispParser::OnUnrecognizedToken(const LispToken *currentToken) {
        GetDiagnosticsInternal().EmplaceBack(Diagnostic::DiagnosticFactory::UnrecognizedToken(
             OriginFile(),
             currentToken->Line,
             currentToken->Column,
             *currentToken));

        return _parseNodesAllocator.new_object<LispParseError>(currentToken,nullptr,nullptr,this);
    }

    BumpVector<Diagnostic::LispDiagnostic>& LispParser::GetDiagnosticsInternal() const {
        return _lexer->GetDiagnostics();
    }

}
