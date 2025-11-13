#include "LispParseTree.h"
#include "LispParser.h"

namespace WideLips {
    LispParser::LispParser(const std::string_view program, const bool conservative):
    _optionalAlignedFile(nullptr),
    Lexer(LispLexer::Make(program,conservative)),
    ParseNodesPool(ArenaSizeEstimate(program.size()/2,conservative)),
    ParseNodesAllocator(&ParseNodesPool),
    EndOfProgram(ParseNodesAllocator.new_object<LispAtom>(&PredefinedTokens::EndOfFile,
                LispParseNodeKind::EndOfProgram,
                nullptr,
                nullptr,
                this
            )){
    }

    LispParser::LispParser(const std::filesystem::path &filePath, const bool conservative):
    _optionalAlignedFile(AlignedFileReader::Read(filePath)),
    Lexer(LispLexer::Make(_optionalAlignedFile,filePath.native(),conservative)),
    ParseNodesPool(ArenaSizeEstimate(Lexer->GetFileSize(),conservative)),
    ParseNodesAllocator(&ParseNodesPool),
    EndOfProgram(ParseNodesAllocator.new_object<LispAtom>(&PredefinedTokens::EndOfFile,
                LispParseNodeKind::EndOfProgram,
                nullptr,
                nullptr,
                this
            )){
    }

    LispParseNodeBase* LispParser::Parse() {
        Lexer->Tokenize();
        const auto optRegion = Lexer->TokenizeFirstSExpr();
        if (!optRegion) {
            return nullptr;
        }
        const auto& [sexprBegin,sexprEnd] = *optRegion;
        return ParseNodesAllocator.new_object<LispList>(
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
                subExpressionsHead =  ParseNodesAllocator.new_object<LispList>(
                    openParen,
                    currentToken--,
                    nullptr,
                    subExpressionsHead,
                    nullptr,
                    this
                );
            }
            else if (currentToken->IsOperator()) {
                subExpressionsHead = ParseNodesAllocator.new_object<LispAtom>(currentToken,
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
                subExpressionsHead = ParseNodesAllocator.new_object<LispParseError>(
                    currentToken,
                    subExpressionsHead,
                    nullptr,
                    this
                );
            }
            else [[likely]]{
                subExpressionsHead = ParseNodesAllocator.new_object<LispAtom>(currentToken,
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
        return Lexer->GetFilePath();
    }

    LispLexer * LispParser::GetLexer() const {
        return Lexer.get();
    }

    LispAuxiliary * LispParser::MakeAuxiliary(const LispToken *auxBegin, const LispToken *auxEnd) {
        return ParseNodesAllocator.new_object<LispAuxiliary>(std::pair{auxBegin,auxEnd});
    }

    LispList * LispParser::MakeList(const LispToken *sexprBegin, const LispToken *sexprEnd) {
        return ParseNodesAllocator.new_object<LispList>(sexprBegin,sexprEnd,nullptr,nullptr,nullptr,this);
    }

    LispAtom * LispParser::MakeEndOfProgram() const {
        return EndOfProgram;
    }

    void LispParser::Reuse() const {
        Lexer->Reuse();
    }

    const BumpVector<Diagnostic::LispDiagnostic> &LispParser::GetDiagnostics() const {
        return Lexer->GetDiagnostics();
    }

    LispParseNodeBase* LispParser::ParseDialectSpecial(const LispToken *currentToken) {
        if (currentToken->Match(LispTokenKind::QuasiColumn) ||
            currentToken->Match(LispTokenKind::Comma) ||
            currentToken->Match(LispTokenKind::At)) {
            return ParseNodesAllocator.new_object<LispAtom>(currentToken,
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

        return ParseNodesAllocator.new_object<LispParseError>(currentToken,nullptr,nullptr,this);
    }

    BumpVector<Diagnostic::LispDiagnostic>& LispParser::GetDiagnosticsInternal() const {
        return Lexer->GetDiagnostics();
    }

}
