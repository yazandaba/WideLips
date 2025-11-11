#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H
#include <format>
#include <string>

#include "Config.h"

namespace WideLips {
    struct LispToken;

    namespace Diagnostic{

        class DiagnosticFactory;
        class DiagnosticParser;
        struct DiagnosticSourceLocation;
        struct LispDiagnostic;

        enum class Severity {
            Error,
            Warning,
            Info,
            Note
        };

        enum class ParsingErrorCode : std::uint32_t {
            SyntaxError=1000,
            UnrecognizedToken, //1001
            EmptySExpr, //1002
            NonTerminatingStringLiteral, //1003
            UnexpectedToken, //1004
            MalformedFloatingPointLiteral, //1005,
            ProgramMustStartWithSExpression, //1006
            NoMatchingOpenParenthesis, //1007
            NoMatchingCloseParenthesis, //1008
            FetchingAuxiliaryOfLazyToken,//1009
            UnexpectedTopLevelToken//1010
        };

        struct DiagnosticSourceLocation final {
        private:
            friend class DiagnosticParser;
            static const DiagnosticSourceLocation Default;
        public:
            const std::wstring_view File;
            const std::wstring_view Line;
            const std::wstring_view Column;
        };

        struct LispDiagnostic final {
        private:
            std::wstring _message;
            const Severity _severity;
        public:
            LispDiagnostic(std::wstring&& message, Severity severity) noexcept;
            LispDiagnostic(LispDiagnostic&& diagnostic) noexcept;
            LispDiagnostic(const LispDiagnostic&) = delete;
            LispDiagnostic& operator=(const LispDiagnostic&) = delete;
            LispDiagnostic& operator=(LispDiagnostic&&) noexcept = delete;
            ~LispDiagnostic() = default;
        public:
            NODISCARD DiagnosticSourceLocation GetLocation() const noexcept;
            NODISCARD std::wstring_view GetFullMessage() const noexcept;
            NODISCARD std::wstring_view GetMessage() const noexcept;
            NODISCARD std::wstring_view GetErrorCode() const noexcept;
            NODISCARD Severity GetSeverity() const noexcept;
        };

        class WL_INTERNAL DiagnosticFactory final {
        private:

            static const wchar_t * SeverityToString(Severity severity);

            static std::wstring Create(std::wstring_view filePath,
                std::uint32_t line,
                std::uint32_t column,
                Severity severity,
                const std::wstring& errorCode,
                const std::wstring& message);
        public:
            ~DiagnosticFactory() = delete;
        public:

            static const wchar_t * ErrorCodeToString(ParsingErrorCode errorCode);

            static LispDiagnostic SyntaxError(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t col,
                const std::string& expected);

            static LispDiagnostic UnexpectedToken(std::wstring_view file,
               std::uint32_t line,
               std::uint32_t col,
               const LispToken& unexpectedToken);

            static LispDiagnostic EmptySExpression(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t col);

            static LispDiagnostic UnterminatedStringLiteral(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t col);

            static LispDiagnostic UnrecognizedToken(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t col,
                const LispToken& unrecognizedToken);

            static LispDiagnostic MalformedFloatingPointLiteral(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t column,
                std::string_view malformedFloat);

            static LispDiagnostic ProgramMustStartWithSExpression(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t column);

            static LispDiagnostic NoMatchingOpenParenthesis(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t column,
                const LispToken& closingParenthesis);

            static LispDiagnostic NoMatchingCloseParenthesis(std::wstring_view file,
               std::uint32_t line,
               std::uint32_t column,
               const LispToken& openingParenthesis);

            static LispDiagnostic FetchingAuxiliaryOfLazyToken(std::wstring_view file,
                std::uint32_t line,
                std::uint32_t column,
                const LispToken& lazyToken);

            static LispDiagnostic UnexpectedTopLevelToken(std::wstring_view file,
              std::uint32_t line,
              std::uint32_t col);
        };


        class WL_INTERNAL DiagnosticParser final {
        public:
            static std::wstring_view GetFilePathView(std::wstring_view diagnosticString);

            static std::wstring_view GetLineView(std::wstring_view diagnosticString);

            static std::wstring_view GetColumnView(std::wstring_view diagnosticString);

            static std::wstring_view GetErrorCodeView(std::wstring_view diagnosticString);

            static std::wstring_view GetMessageView(std::wstring_view diagnosticString);

            static std::wstring_view GetLocationView(std::wstring_view diagnosticString);

            static DiagnosticSourceLocation GetFileAndLocationView(std::wstring_view diagnosticString);
        };
    } //namespace Diagnostic
} // namespace WideLips
#endif //DIAGNOSTIC_H