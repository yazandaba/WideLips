#include <format>

#include "Diagnostic.h"
#include "LispLexer.h"


namespace WideLips::Diagnostic {

    NODISCARD ALWAYS_INLINE std::wstring WidenString(const std::string& str) {
        return std::wstring{str.begin(), str.end()};
    }

    NODISCARD ALWAYS_INLINE std::wstring WidenString(std::string_view str) {
        return std::wstring{str.begin(), str.end()};
    }

    const DiagnosticSourceLocation DiagnosticSourceLocation::Default{
        .File = L"Undefined",
        .Line = L"0",
        .Column = L"0"
    };

    LispDiagnostic::LispDiagnostic(std::wstring&& message, const Severity severity) noexcept:
        _message(std::move(message)),
        _severity(severity) {}

    LispDiagnostic::LispDiagnostic(LispDiagnostic&& diagnostic) noexcept :
        _message(std::move(diagnostic._message)),
        _severity(diagnostic._severity) {}

    DiagnosticSourceLocation LispDiagnostic::GetLocation() const noexcept {
        return DiagnosticParser::GetFileAndLocationView(_message);
    }

    std::wstring_view LispDiagnostic::GetFullMessage() const noexcept {
        return _message;
    }

    std::wstring_view LispDiagnostic::GetMessage() const noexcept {
        return DiagnosticParser::GetMessageView(_message);
    }

    std::wstring_view LispDiagnostic::GetErrorCode() const noexcept {
        return DiagnosticParser::GetErrorCodeView(_message);
    }

    Severity LispDiagnostic::GetSeverity() const noexcept {
        return _severity;
    }

    const wchar_t * DiagnosticFactory::SeverityToString(const Severity severity) {
        switch (severity) {
            case Severity::Error:   return L"error";
            case Severity::Warning: return L"warning";
            case Severity::Info:    return L"info";
            case Severity::Note:    return L"note";
            default: return L"unknown";
        }
    }

    const wchar_t * DiagnosticFactory::ErrorCodeToString(const ParsingErrorCode errorCode) {
        switch (errorCode) {
            case ParsingErrorCode::SyntaxError: return L"LISP1000";
            case ParsingErrorCode::UnrecognizedToken: return L"LISP1001";
            case ParsingErrorCode::EmptySExpr: return L"LISP1002";
            case ParsingErrorCode::NonTerminatingStringLiteral: return L"LISP1003";
            case ParsingErrorCode::UnexpectedToken: return L"LISP1004";
            case ParsingErrorCode::MalformedFloatingPointLiteral: return L"LISP1005";
            case ParsingErrorCode::ProgramMustStartWithSExpression: return L"LISP1006";
            case ParsingErrorCode::NoMatchingOpenParenthesis: return L"LISP1007";
            case ParsingErrorCode::NoMatchingCloseParenthesis: return L"LISP1008";
            case ParsingErrorCode::FetchingAuxiliaryOfLazyToken: return L"LISP1009";
            case ParsingErrorCode::UnexpectedTopLevelToken: return L"LISP1010";
            default: return L"unknown";
        }
    }

    std::wstring DiagnosticFactory::Create(std::wstring_view filePath,
        const std::uint32_t line,
        const std::uint32_t column,
        const Severity severity,
        const std::wstring& errorCode,
        const std::wstring& message) {

        return std::move(std::format(L"{}({},{}): {} {}: {}",
            filePath, line, column,
            SeverityToString(severity), errorCode, message));
    }

    LispDiagnostic DiagnosticFactory::SyntaxError(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col,
        const std::string& expected) {
        return LispDiagnostic {
            std::move(Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::SyntaxError),
                std::format(L"Syntax error, '{}' expected", WidenString(expected)))),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::UnexpectedToken(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col,
        const LispToken& unexpectedToken) {
        return LispDiagnostic {
            std::move(Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::SyntaxError),
                std::format(L"Syntax error, unexpected token '{}' ", WidenString(unexpectedToken.GetText())))),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::EmptySExpression(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col) {
        return LispDiagnostic{
            Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::EmptySExpr),
                L"Empty s-expression"),
                Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::UnterminatedStringLiteral(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col) {
        return LispDiagnostic{
        Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::NonTerminatingStringLiteral),
            L"Unterminated string literal"),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::UnrecognizedToken(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col,
        const LispToken& unrecognizedToken) {
        return LispDiagnostic{
            Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::UnrecognizedToken),
                     std::format(L"Unrecognized token, '{}' ", WidenString(unrecognizedToken.GetText()))),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::MalformedFloatingPointLiteral(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t column,
        const std::string_view malformedFloat) {
        return LispDiagnostic{
            Create(file, line, column, Severity::Error, ErrorCodeToString(ParsingErrorCode::MalformedFloatingPointLiteral),
                std::format(L"Malformed floating point literal, '{}'", WidenString(malformedFloat) )),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::ProgramMustStartWithSExpression(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t column) {
        return LispDiagnostic{
            Create(file, line, column, Severity::Error, ErrorCodeToString(ParsingErrorCode::ProgramMustStartWithSExpression),
                L"Lisp program must start with SExpression"),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::NoMatchingOpenParenthesis(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t column,
        const LispToken &closingParenthesis) {
        return LispDiagnostic{
            Create(file, line, column, Severity::Error, ErrorCodeToString(ParsingErrorCode::NoMatchingOpenParenthesis),
               std::format(L"closing parenthesis at ({},{}) does not have an opening parenthesis",
                   closingParenthesis.Line,closingParenthesis.Column)),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::NoMatchingCloseParenthesis(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t column,
        const LispToken &openingParenthesis) {
        return LispDiagnostic{
            Create(file, line, column, Severity::Error, ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis),
               std::format(L"open parenthesis at ({},{}) does not have a closing parenthesis",
                   openingParenthesis.Line,openingParenthesis.Column)),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::FetchingAuxiliaryOfLazyToken(const std::wstring_view file,
        std::uint32_t line,
        std::uint32_t column,
        const LispToken &lazyToken) {
        return LispDiagnostic{
            Create(file, line, column, Severity::Error, ErrorCodeToString(ParsingErrorCode::FetchingAuxiliaryOfLazyToken),
               std::format(L"getting auxiliary of lazy token '{}' at ({},{}) is prohibited",
                   WidenString(lazyToken.GetText()),line,column)),
            Severity::Error
        };
    }

    LispDiagnostic DiagnosticFactory::UnexpectedTopLevelToken(const std::wstring_view file,
        const std::uint32_t line,
        const std::uint32_t col) {
        return LispDiagnostic {
            std::move(Create(file, line, col, Severity::Error, ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken),
                std::format(L"unexpected token at program top level, only s-expressions are allowed at program top level "))),
            Severity::Error
        };
    }

    std::wstring_view DiagnosticParser::GetFilePathView(std::wstring_view diagnosticString) {
        if (const size_t openParen = diagnosticString.find('('); openParen != std::string_view::npos && openParen > 0) {
            return diagnosticString.substr(0, openParen);
        }
        return L"";
    }

    std::wstring_view DiagnosticParser::GetLineView(std::wstring_view diagnosticString) {
        const size_t openParen = diagnosticString.find('(');
        const size_t comma = diagnosticString.find(',', openParen);
        if (openParen != std::string_view::npos && comma != std::string_view::npos && comma > openParen + 1) {
            return diagnosticString.substr(openParen + 1, comma - openParen - 1);
        }
        return L"";
    }

    std::wstring_view DiagnosticParser::GetColumnView(std::wstring_view diagnosticString) {
        const size_t comma = diagnosticString.find(',');
        const size_t closeParen = diagnosticString.find(')', comma);
        if (comma != std::string_view::npos && closeParen != std::string_view::npos && closeParen > comma + 1) {
            return diagnosticString.substr(comma + 1, closeParen - comma - 1);
        }
        return L"";
    }

    std::wstring_view DiagnosticParser::GetErrorCodeView(std::wstring_view diagnosticString) {
        const size_t closeParen = diagnosticString.find(')');
        const size_t firstColon = diagnosticString.find(L": ", closeParen);
        const size_t lastColon = diagnosticString.find(L": ", firstColon + 2);

        if (closeParen != std::string_view::npos && firstColon != std::string_view::npos && lastColon != std::string_view::npos) {
            const size_t severityStart = closeParen + 3;
            const size_t spaceBeforeCode = diagnosticString.find(' ', severityStart);
            if (spaceBeforeCode != std::string_view::npos && spaceBeforeCode < lastColon) {
                return diagnosticString.substr(spaceBeforeCode + 1, lastColon - spaceBeforeCode - 1);
            }
        }
        return L"";
    }

    std::wstring_view DiagnosticParser::GetMessageView(const std::wstring_view diagnosticString) {
        const size_t lastColon = diagnosticString.rfind(L": ");
        if (lastColon != std::string_view::npos && lastColon + 2 < diagnosticString.length()) {
            return diagnosticString.substr(lastColon + 2);
        }
        return L"";
    }

    std::wstring_view DiagnosticParser::GetLocationView(std::wstring_view diagnosticString) {
        const size_t openParen = diagnosticString.find('(');
        const size_t closeParen = diagnosticString.find(')', openParen);
        if (openParen != std::string_view::npos && closeParen != std::string_view::npos && closeParen > openParen + 1) {
            return diagnosticString.substr(openParen + 1, closeParen - openParen - 1);
        }
        return L"";
    }

    DiagnosticSourceLocation DiagnosticParser::GetFileAndLocationView(std::wstring_view diagnosticString) {
        const size_t openParen = diagnosticString.find('(');
        const size_t comma = diagnosticString.find(',', openParen);
        const size_t closeParen = diagnosticString.find(')', comma);

        if (openParen != std::string_view::npos &&
            comma != std::string_view::npos &&
            closeParen != std::string_view::npos &&
            openParen > 0 && comma > openParen + 1 && closeParen > comma + 1) {
            return DiagnosticSourceLocation{
                .File = diagnosticString.substr(0, openParen),
                .Line = diagnosticString.substr(openParen + 1, comma - openParen - 1),
                .Column = diagnosticString.substr(comma + 1, closeParen - comma - 1)
            };
        }
        return DiagnosticSourceLocation::Default;
    }
}