#include "LispParseTree.h"
#include "SchemeParser.h"

namespace WideLips::Examples{

    SchemeParser::SchemeParser(const std::filesystem::path& filePath, bool conservative)
        : LispParser(filePath, conservative) {
    }

    SchemeParser::SchemeParser(std::string_view program, bool conservative)
        : LispParser(program, conservative) {
    }

    LispParseNodeBase* SchemeParser::Parse(const LispToken* sexprBegin, const LispToken* sexprEnd) {
        LispParseNodeBase* head = nullptr;
        LispParseNodeBase* tail = nullptr;

        for (auto currentToken = sexprBegin; currentToken <= sexprEnd; ++currentToken) [[likely]] {
            LispParseNodeBase* newNode = nullptr;

            if (currentToken->Match(LispTokenKind::LeftParenthesis)) {
                // Nested list - right paren is next token
                auto openParen = currentToken;
                auto closeParen = currentToken + 1;

                newNode = ParseNodesAllocator.new_object<LispList>(
                    openParen,
                    closeParen,
                    nullptr,
                    nullptr,
                    nullptr,
                    this
                );

                // If you want to eagerly parse its contents:
                // newNode->GetSubExpressions();

                // Skip past the closing paren
                currentToken = closeParen;
            }
            else if (currentToken->Match(LispTokenKind::Defun) || currentToken->Match(LispTokenKind::Defmacro)) {
                // (defun name (params) body...)
                // We're at defun, validate structure

                auto nameToken = currentToken + 1;
                if (nameToken > sexprEnd) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            currentToken->Line,
                            currentToken->Column,
                            *currentToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else if (!nameToken->Match(LispTokenKind::Identifier)) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            nameToken->Line,
                            nameToken->Column,
                            *nameToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else {
                    auto paramsToken = nameToken + 1;
                    if (paramsToken > sexprEnd) {
                        GetDiagnosticsInternal().EmplaceBack(
                            Diagnostic::DiagnosticFactory::UnrecognizedToken(
                                OriginFile(),
                                currentToken->Line,
                                currentToken->Column,
                                *currentToken
                            )
                        );
                        newNode = ParseNodesAllocator.new_object<LispParseError>(
                            currentToken,
                            nullptr,
                            nullptr,
                            this
                        );
                    } else if (!paramsToken->Match(LispTokenKind::LeftParenthesis)) {
                        GetDiagnosticsInternal().EmplaceBack(
                            Diagnostic::DiagnosticFactory::UnrecognizedToken(
                                OriginFile(),
                                paramsToken->Line,
                                paramsToken->Column,
                                *paramsToken
                            )
                        );
                        newNode = ParseNodesAllocator.new_object<LispParseError>(
                            currentToken,
                            nullptr,
                            nullptr,
                            this
                        );
                    } else {
                        newNode = ParseNodesAllocator.new_object<LispAtom>(
                            currentToken,
                            static_cast<LispParseNodeKind>(currentToken->Kind),
                            nullptr,
                            nullptr,
                            this
                        );
                    }
                }
            }
            else if (currentToken->Match(LispTokenKind::Lambda)) {
                // (lambda (params) body...)

                auto paramsToken = currentToken + 1;
                if (paramsToken > sexprEnd) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            currentToken->Line,
                            currentToken->Column,
                            *currentToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else if (!paramsToken->Match(LispTokenKind::LeftParenthesis)) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            paramsToken->Line,
                            paramsToken->Column,
                            *paramsToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else {
                    newNode = ParseNodesAllocator.new_object<LispAtom>(
                        currentToken,
                        LispParseNodeKind::Lambda,
                        nullptr,
                        nullptr,
                        this
                    );
                }
            }
            else if (currentToken->Match(LispTokenKind::If)) {
                // (if condition then else?)
                //dummy implementation, we assume that if expression terms consist of single token! but in reality
                //one should do something like
                //auto condition = ParseConditionalExpression(...)
                //auto then = ParseExpression(...)
                //auto els = ParseExpression(...)
                //then connect the full expression like [If]->[Condition]->[Then]->[Else]
                //it's up to the user to choose whether this should get done eagerly or lazily (if subexpressions are lists)

                newNode = ParseNodesAllocator.new_object<LispAtom>(
                        currentToken,
                        LispParseNodeKind::If,
                        nullptr,
                        nullptr,
                        this
                    );
            }
            else if (currentToken->Match(LispTokenKind::Let)) {
                // (let ((var val)...) body...)

                auto bindingsToken = currentToken + 1;
                if (bindingsToken > sexprEnd) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            currentToken->Line,
                            currentToken->Column,
                            *currentToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else if (!bindingsToken->Match(LispTokenKind::LeftParenthesis)) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            bindingsToken->Line,
                            bindingsToken->Column,
                            *bindingsToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else {
                    newNode = ParseNodesAllocator.new_object<LispAtom>(
                        currentToken,
                        LispParseNodeKind::Let,
                        nullptr,
                        nullptr,
                        this
                    );
                }
            }
            else if (currentToken->Match(LispTokenKind::Defvar)) {
                // (defvar name value?)

                auto nameToken = currentToken + 1;
                if (nameToken > sexprEnd) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::UnrecognizedToken(
                            OriginFile(),
                            currentToken->Line,
                            currentToken->Column,
                            *currentToken
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else if (!nameToken->Match(LispTokenKind::Identifier)) {
                    GetDiagnosticsInternal().EmplaceBack(
                        Diagnostic::DiagnosticFactory::SyntaxError(
                            OriginFile(),
                            nameToken->Line,
                            nameToken->Column,
                            TokenKindToString(nameToken->Kind)
                        )
                    );
                    newNode = ParseNodesAllocator.new_object<LispParseError>(
                        currentToken,
                        nullptr,
                        nullptr,
                        this
                    );
                } else {
                    newNode = ParseNodesAllocator.new_object<LispAtom>(
                        currentToken,
                        LispParseNodeKind::Defvar,
                        nullptr,
                        nullptr,
                        this
                    );
                }
            }
            else if (currentToken->IsOperator()) {
                newNode = ParseNodesAllocator.new_object<LispAtom>(
                    currentToken,
                    LispParseNodeKind::Operator,
                    nullptr,
                    nullptr,
                    this
                );
            }
            else if (currentToken->IsDialectSpecial()) [[unlikely]] {
                newNode = ParseDialectSpecial(currentToken);
            }
            else if (currentToken->Match(LispTokenKind::Invalid)) [[unlikely]] {
                newNode = ParseNodesAllocator.new_object<LispParseError>(
                    currentToken,
                    nullptr,
                    nullptr,
                    this
                );
            }
            else [[likely]] {
                newNode = ParseNodesAllocator.new_object<LispAtom>(
                    currentToken,
                    static_cast<LispParseNodeKind>(currentToken->Kind),
                    nullptr,
                    nullptr,
                    this
                );
            }

            if (newNode) {
                if (!head) {
                    head = newNode;
                    tail = newNode;
                } else {
                    tail->Next = newNode; //tail will not be nullptr (some static analyzers like Resharper may
                    //produce false positive here
                    tail = newNode;
                }
            }
        }

        return head;
    }

    LispParseNodeBase* SchemeParser::ParseDialectSpecial(const LispToken* currentToken) {
        if (currentToken->Match(LispTokenKind::Quote)) {
            return HandleQuote(currentToken);
        }
        if (currentToken->Match(LispTokenKind::QuasiColumn)) {
            return HandleQuasiquote(currentToken);
        }
        if (currentToken->Match(LispTokenKind::Comma)) {
            return HandleUnquote(currentToken);
        }
        if (currentToken->Match(LispTokenKind::At)) {
            return HandleUnquote(currentToken);
        }
        if (currentToken->Match(LispTokenKind::Hash)) {
            return HandleHash(currentToken);
        }

        return OnUnrecognizedToken(currentToken);
    }

    LispParseNodeBase* SchemeParser::HandleQuote(const LispToken* quoteToken) {
        return ParseNodesAllocator.new_object<LispAtom>(
            quoteToken,
            LispParseNodeKind::Operator,
            nullptr,
            nullptr,
            this
        );
    }

    LispParseNodeBase* SchemeParser::HandleQuasiquote(const LispToken* quasiquoteToken) {
        return ParseNodesAllocator.new_object<LispAtom>(
            quasiquoteToken,
            LispParseNodeKind::Operator,
            nullptr,
            nullptr,
            this
        );
    }

    LispParseNodeBase* SchemeParser::HandleUnquote(const LispToken* unquoteToken) {
        return ParseNodesAllocator.new_object<LispAtom>(
            unquoteToken,
            LispParseNodeKind::Operator,
            nullptr,
            nullptr,
            this
        );
    }

    LispParseNodeBase* SchemeParser::HandleHash(const LispToken* hashToken) {
        return ParseNodesAllocator.new_object<LispAtom>(
            hashToken,
            LispParseNodeKind::Operator,
            nullptr,
            nullptr,
            this
        );
    }

}