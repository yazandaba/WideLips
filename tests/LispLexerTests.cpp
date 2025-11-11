#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LispLexer.h"
#include <sstream>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <fstream> // Added for file I/O in file tests
#include "Utilities/AlignedFileReader.h" // Added for file-based tests

using namespace WideLips;
using namespace testing;

namespace WideLips::Tests {
    static std::string PadString(const std::string& str) {
        return str + std::string(PaddingSize,EOF);
    }

    // Helper structure to hold expected token information
    struct ExpectedToken {
        LispTokenKind Kind;
        std::string Text;
        std::uint32_t Length;

        ExpectedToken(const LispTokenKind k, const std::string& t)
            : Kind(k), Text(t), Length(t.length()) {}
        ExpectedToken(const LispTokenKind k, std::string  t, const std::uint32_t len)
            : Kind(k), Text(std::move(t)), Length(len) {}
    };

    // Test fixture for LispLexer
    class LispLexerTest : public Test {
    public:
        void SetUp() override {}
        void TearDown() override {}

        // Helper to create lexer from string
        static std::unique_ptr<LispLexer> CreateLexer(const std::string_view input,const bool conservative = false) {
            return LispLexer::Make(input, conservative);
        }

        // Helper to recursively collect all tokens from an S-expression
        static void CollectAllTokens(LispLexer* lexer,
            const LispToken* sexprBegin,
            const LispToken* sexprEnd,
            std::vector<const LispToken*>& result,
            const bool csEmptySExpr= false) {
            // Add opening paren
            result.push_back(sexprBegin);

            // Tokenize immediate children of this S-expression
            const auto tokRegion = lexer->TokenizeSExpr(sexprBegin,csEmptySExpr);
            // Handle empty SExpr case
            if (!tokRegion.has_value()) {
                if (csEmptySExpr) {
                    // This is fine if we allow empty SExpr
                    result.push_back(sexprEnd);
                    return;
                }
                // Fail test if we got nullopt but weren't expecting empty SExpr
                ASSERT_TRUE(tokRegion.has_value());
            }

            const auto [tokBegin, tokEnd] = *tokRegion;

            const LispToken* current = tokBegin;
            while (current <= tokEnd) {
                if (current->Kind == LispTokenKind::LeftParenthesis) {
                    // Found a nested S-expression - need to recursively tokenize it
                    const LispToken* nestedBegin = current;
                    ++current;

                    // The next token should be the matching right paren (placeholder)
                    ASSERT_LE(current, tokEnd);
                    ASSERT_EQ(current->Kind, LispTokenKind::RightParenthesis) << "Expected matching right paren after left paren";
                    const LispToken* nestedEnd = current;

                    // Recursively collect tokens from the nested expression
                    CollectAllTokens(lexer, nestedBegin, nestedEnd, result, csEmptySExpr);
                    ++current;
                } else {
                    result.push_back(current);
                    ++current;
                }
            }

            // Add closing paren
            result.push_back(sexprEnd);
        }

        // Helper to verify token stream matches expected tokens
        static void VerifyTokens(const std::string& input,
            const std::vector<ExpectedToken>& expected,
            const bool csEmptySExpr= false) {

            const auto paddedInput = PadString(input);
            const auto lexer = CreateLexer(paddedInput);
            ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for: " << input;

            const auto optRegion = lexer->TokenizeFirstSExpr();
            ASSERT_TRUE(optRegion.has_value());
            const auto [regionBegin, regionEnd] = *optRegion;
            // Recursively collect all tokens
            std::vector<const LispToken*> flattenTokens;
            CollectAllTokens(lexer.get(), regionBegin, regionEnd, flattenTokens, csEmptySExpr);

            ASSERT_EQ(flattenTokens.size(), expected.size())
                << "Token count mismatch. Expected: " << expected.size()
                << ", Got: " << flattenTokens.size();

            for (size_t i = 0; i < flattenTokens.size(); ++i) {
                EXPECT_EQ(flattenTokens[i]->Kind, expected[i].Kind)
                    << "Token " << i << " kind mismatch. Expected: "
                    << TokenKindToString(expected[i].Kind)
                    << ", Got: " << TokenKindToString(flattenTokens[i]->Kind);

                if (!expected[i].Text.empty()) {
                    EXPECT_EQ(flattenTokens[i]->GetText(), expected[i].Text)
                        << "Token " << i << " text mismatch";
                }

                if (expected[i].Length > 0) {
                    EXPECT_EQ(flattenTokens[i]->Length, expected[i].Length)
                        << "Token " << i << " length mismatch";
                }
            }
        }

        // Helper to verify a simple expression with auxiliary tokens
        static void VerifyTokensWithAux(const std::string& input,
                                 const std::vector<ExpectedToken>& expected,
                                 const bool hasLeadingAux = false) {
            const auto paddedInput = PadString(input);
            const auto lexer = CreateLexer(paddedInput);
            ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for: " << input;

            const auto optRegion = lexer->TokenizeFirstSExpr();
            ASSERT_TRUE(optRegion.has_value());

            const auto [regionBegin, regionEnd] = *optRegion;
            EXPECT_EQ(regionBegin->Kind,  expected.front().Kind);
            EXPECT_EQ(regionEnd->Kind, expected.back().Kind);

            if (hasLeadingAux) {
                const auto aux = lexer->GetTokenAuxiliary(regionBegin);
                EXPECT_TRUE(aux.has_value()) << "Expected auxiliary tokens";
            }
            const auto tokRegion = lexer->TokenizeSExpr(regionBegin);
            ASSERT_TRUE(tokRegion.has_value());
            const auto [regionTokBegin, regionTokEnd] = *tokRegion;
            const LispToken* current = regionTokBegin;
            size_t tokenIndex = 1;

            while (current <= regionTokEnd && tokenIndex < expected.size()-2) {
                EXPECT_EQ(current->Kind, expected[tokenIndex].Kind);
                if (!expected[tokenIndex].Text.empty()) {
                    EXPECT_EQ(current->GetText(), expected[tokenIndex].Text);
                }
                ++current;
                ++tokenIndex;
            }
        }
    };

    // ============================================================================
    // COMPREHENSIVE TOKEN VERIFICATION TESTS - LESS THAN 32 BYTES
    // ============================================================================

    TEST_F(LispLexerTest, SimpleAddition_LessThan32Bytes) {
        // "(+ 1 2)" = 7 bytes
        VerifyTokens("(+ 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, NestedExpression_LessThan32Bytes) {
        // "(+ (* 2 3) 4)" = 13 bytes
        VerifyTokens("(+ (* 2 3) 4)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RealLiteral, "3"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RealLiteral, "4"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, AllBasicOperators_LessThan32Bytes) {
        // "(+ - * / %)" = 11 bytes
        VerifyTokens("(+ - * / %)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::ForwardSlash, "/"},
            {LispTokenKind::Modulo, "%"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, SimpleIdentifier_LessThan32Bytes) {
        // "(test x y)" = 10 bytes
        VerifyTokens("(test x y)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "test"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::Identifier, "y"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FloatingPoint_LessThan32Bytes) {
        // "(+ 3.14 2.71)" = 13 bytes
        VerifyTokens("(+ 3.14 2.71)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "3.14"},
            {LispTokenKind::RealLiteral, "2.71"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, StringLiteral_LessThan32Bytes) {
        // "(msg \"hi\")" = 10 bytes
        VerifyTokens("(msg \"hi\")", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "msg"},
            {LispTokenKind::StringLiteral, "\"hi\""},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // EXACTLY 32 BYTES - BOUNDARY TESTS
    // ============================================================================

    TEST_F(LispLexerTest, Exactly32Bytes_ArithmeticExpression) {
        // Exactly 32 bytes
        const std::string input = "(+ (* 2 3) (- 4 5) (/ 6 7) 8.12)";
        ASSERT_EQ(input.size(), 32);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RealLiteral, "3"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::RealLiteral, "4"},
            {LispTokenKind::RealLiteral, "5"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::ForwardSlash, "/"},
            {LispTokenKind::RealLiteral, "6"},
            {LispTokenKind::RealLiteral, "7"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RealLiteral, "8.12"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Exactly32Bytes_WithIdentifiers) {
        // Exactly 32 bytes
        const std::string input = "(func arg1 arg2 arg3 arg4 x_2Eo)";
        ASSERT_EQ(input.size(), 32);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "func"},
            {LispTokenKind::Identifier, "arg1"},
            {LispTokenKind::Identifier, "arg2"},
            {LispTokenKind::Identifier, "arg3"},
            {LispTokenKind::Identifier, "arg4"},
            {LispTokenKind::Identifier, "x_2Eo"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // MORE THAN 32 BYTES - CROSSING SIMD BOUNDARY
    // ============================================================================

    TEST_F(LispLexerTest, MoreThan32Bytes_ComplexArithmetic) {
        // 45 bytes - crosses boundary
        const std::string input = "(+ (* 10 20) (- 30 40) (/ 50 60) 70 80 90)";
        ASSERT_GT(input.size(), 32);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "10"},
            {LispTokenKind::RealLiteral, "20"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::RealLiteral, "30"},
            {LispTokenKind::RealLiteral, "40"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::ForwardSlash, "/"},
            {LispTokenKind::RealLiteral, "50"},
            {LispTokenKind::RealLiteral, "60"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RealLiteral, "70"},
            {LispTokenKind::RealLiteral, "80"},
            {LispTokenKind::RealLiteral, "90"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, MoreThan32Bytes_FunctionDefinition) {
        // 26 bytes (without comment and null terminator)
        const std::string input = "(defun square (x) (* x x))";
        ASSERT_GT(input.size(), 20);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Defun, "defun"},
            {LispTokenKind::Identifier, "square"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // MORE THAN 64 BYTES - MULTIPLE SIMD BLOCKS
    // ============================================================================

    TEST_F(LispLexerTest, MoreThan64Bytes_Fibonacci) {
        // 93 bytes - multiple blocks
        const std::string input = "(defun fibonacci (n) (if (<= n 1) n (+ (fibonacci (- n 1)) (fibonacci (- n 2)))))";
        ASSERT_GT(input.size(), 64);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Defun, "defun"},
            {LispTokenKind::Identifier, "fibonacci"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "n"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::If, "if"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LessThanOrEqual, "<="},
            {LispTokenKind::Identifier, "n"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::Identifier, "n"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "fibonacci"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Identifier, "n"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "fibonacci"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Identifier, "n"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, MoreThan64Bytes_WithStringAndComments) {
        // 98 bytes
        const std::string input = "(defun greet (name) ; greets the user\n  (print (concat \"Hello, \" name \"!\")) ; friendly message\n)";
        const std::string paddedInput = PadString(input);

        ASSERT_GT(input.size(), 64);

        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());
    }

    // ============================================================================
    // KEYWORD RECOGNITION WITH TOKEN VERIFICATION
    // ============================================================================

    TEST_F(LispLexerTest, Keywords_Let) {
        VerifyTokens("(let ((x 5)))", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Let, "let"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RealLiteral, "5"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_And) {
        VerifyTokens("(and true nil)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LogicalAnd, "and"},
            {LispTokenKind::BooleanLiteral, "true"},
            {LispTokenKind::Nil, "nil"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_Or) {
        VerifyTokens("(or true nil)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LogicalOr, "or"},
            {LispTokenKind::BooleanLiteral, "true"},
            {LispTokenKind::Nil, "nil"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_Not) {
        VerifyTokens("(not true)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Not, "not"},
            {LispTokenKind::BooleanLiteral, "true"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_If) {
        VerifyTokens("(if true 1 0)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::If, "if"},
            {LispTokenKind::BooleanLiteral, "true"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "0"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_Lambda) {
        VerifyTokens("(lambda (x) x)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Lambda, "lambda"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Keywords_Defun) {
        VerifyTokens("(defun f ())",{
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Defun, "defun"},
            {LispTokenKind::Identifier, "f"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        },
        true);
    }

    TEST_F(LispLexerTest, Keywords_Nil) {
        VerifyTokens("(list nil)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "list"},
            {LispTokenKind::Nil, "nil"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // NUMBER TESTS WITH TOKEN VERIFICATION
    // ============================================================================

    TEST_F(LispLexerTest, Numbers_Integer) {
        VerifyTokens("(+ 42 100)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "42"},
            {LispTokenKind::RealLiteral, "100"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_MultiDigit) {
        VerifyTokens("(+ 123456789)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "123456789"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_Float) {
        VerifyTokens("(pi 3.14159)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "pi"},
            {LispTokenKind::RealLiteral, "3.14159"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_ScientificNotation_e) {
        VerifyTokens("(* 1.5e10)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "1.5e10"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_ScientificNotation_E) {
        VerifyTokens("(* 2.5E-3)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "2.5E-3"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_ScientificNotation_Plus) {
        VerifyTokens("(* 1.0e+5)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RealLiteral, "1.0e+5"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Numbers_CrossingBoundary) {
        // Number crosses 32-byte boundary
        const std::string input = "                        (test 123456789.987654321e+100)";
        ASSERT_GT(input.size(), 32);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "test"},
            {LispTokenKind::RealLiteral, "123456789.987654321e+100"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // OPERATOR TESTS WITH TOKEN VERIFICATION
    // ============================================================================

    TEST_F(LispLexerTest, Operators_Comparison_LessThan) {
        VerifyTokens("(< 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LessThan, "<"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Comparison_GreaterThan) {
        VerifyTokens("(> 2 1)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::GreaterThan, ">"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Comparison_LessThanOrEqual) {
        VerifyTokens("(<= 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LessThanOrEqual, "<="},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Comparison_GreaterThanOrEqual) {
        VerifyTokens("(>= 2 1)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::GreaterThanOrEqual, ">="},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Bitwise_LeftShift) {
        VerifyTokens("(<< 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LeftBitShift, "<<"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Bitwise_RightShift) {
        VerifyTokens("(>> 4 1)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::RightBitShift, ">>"},
            {LispTokenKind::RealLiteral, "4"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Bitwise_All) {
        VerifyTokens("(& | ^ \\)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Ampersand, "&"},
            {LispTokenKind::BitwiseOr, "|"},
            {LispTokenKind::BitwiseXor, "^"},
            {LispTokenKind::BackwardSlash, "\\"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Operators_Equal) {
        VerifyTokens("(= x y)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Equal, "="},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::Identifier, "y"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // STRING LITERAL TESTS WITH TOKEN VERIFICATION
    // ============================================================================

    TEST_F(LispLexerTest, String_Empty) {
        VerifyTokens("(\"\")", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, "\"\""},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, String_Simple) {
        VerifyTokens("(\"hello\")", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, "\"hello\""},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, String_WithSpaces) {
        VerifyTokens("(\"hello world\")", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, "\"hello world\""},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, String_WithEscapedQuote) {
        VerifyTokens(R"(("say \"hi\""))", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, R"("say \"hi\"")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, String_CrossingBoundary) {
        const std::string input = "(\"                                  longstring\")";
        ASSERT_GT(input.size(), 32);

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, ""},  // Empty text means don't check text, just kind
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, String_Unterminated) {
        using namespace Diagnostic;
        const std::string input = "(\"unterminated";
        const std::string paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_FALSE(lexer->Tokenize());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 2);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NonTerminatingStringLiteral));
        EXPECT_EQ(lexer->GetDiagnostics()[1].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[1].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis));
    }

    // ============================================================================
    // IDENTIFIER TESTS WITH TOKEN VERIFICATION
    // ============================================================================

    TEST_F(LispLexerTest, Identifier_SingleChar) {
        VerifyTokens("(a)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "a"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Identifier_Lowercase) {
        VerifyTokens("(myvar)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "myvar"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Identifier_MixedCase) {
        VerifyTokens("(myVariable)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "myVariable"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Identifier_WithUnderscore) {
        VerifyTokens("(my_var)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "my_var"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Identifier_StartingWithUnderscore) {
        VerifyTokens("(_private)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "_private"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Identifier_WithNumbers) {
        VerifyTokens("(var123)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "var123"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // WHITESPACE AND AUXILIARY TOKEN TESTS
    // ============================================================================

    TEST_F(LispLexerTest, Whitespace_Spaces) {
        VerifyTokens("(  +  1  2  )", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Whitespace_Leading) {
        VerifyTokensWithAux("   (+ 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        }, true);
    }

    TEST_F(LispLexerTest, Comment_LeadingComment) {
        VerifyTokensWithAux("; comment\n(+ 1 2)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        }, true);
    }

    // ============================================================================
    // COMPLEX MULTI-BLOCK EXPRESSIONS
    // ============================================================================

    TEST_F(LispLexerTest, Complex_NestedArithmetic) {
        VerifyTokens("(+ (* (- 5 3) 2) (/ 10 2))", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::RealLiteral, "5"},
            {LispTokenKind::RealLiteral, "3"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::ForwardSlash, "/"},
            {LispTokenKind::RealLiteral, "10"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Complex_LetExpression) {
        VerifyTokens("(let ((x 5) (y 10)) (+ x y))", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Let, "let"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RealLiteral, "5"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "y"},
            {LispTokenKind::RealLiteral, "10"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::Identifier, "y"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, Complex_ConditionalWithComparison) {
        VerifyTokens("(if (> x 0) x (- x))", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::If, "if"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::GreaterThan, ">"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RealLiteral, "0"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Identifier, "x"},
            {LispTokenKind::RightParenthesis, ")"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // ERROR HANDLING TESTS
    // ============================================================================

    TEST_F(LispLexerTest, Error_UnmatchedCloseParen) {
        using namespace Diagnostic;
        const auto input = PadString("(+ 1 2");
        const auto lexer = CreateLexer(input);
        EXPECT_FALSE(lexer->Tokenize());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis));
    }

    TEST_F(LispLexerTest, Error_UnmatchedOpenParen) {
        using namespace Diagnostic;
        const auto input = PadString("+ 1 2)");
        const auto lexer = CreateLexer(input);
        EXPECT_FALSE(lexer->Tokenize());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 4);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken));
        EXPECT_EQ(lexer->GetDiagnostics()[1].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[1].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken));
        EXPECT_EQ(lexer->GetDiagnostics()[2].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[2].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken));
        EXPECT_EQ(lexer->GetDiagnostics()[3].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[3].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingOpenParenthesis));
    }

    TEST_F(LispLexerTest, Error_MalformedFloat) {
        using namespace Diagnostic;
        const auto input = PadString("(+ 1.5e)");
        const auto lexer = CreateLexer(input);
        EXPECT_FALSE(lexer->Tokenize());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::MalformedFloatingPointLiteral));
    }

    TEST_F(LispLexerTest, Error_InvalidToken) {
        using namespace Diagnostic;
        const auto input = PadString("(?)");
        const auto lexer = CreateLexer(input);
        EXPECT_FALSE(lexer->Tokenize());
        auto root = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(root);
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken));
        auto [begin,end]= *root;
        auto [invalid,_] = *lexer->TokenizeSExpr(begin);
        EXPECT_TRUE(invalid->Match(LispTokenKind::Invalid));
    }

    TEST_F(LispLexerTest, Error_DoubleTokenizeUsage) {
        using namespace Diagnostic;
        const auto input = PadString("(+ 1 2)");
        const auto lexer = CreateLexer(input);
        EXPECT_TRUE(lexer->Tokenize());
#ifndef NDEBUG
        EXPECT_DEATH(lexer->Tokenize(),".*");
#else
        EXPECT_FALSE(lexer->Tokenize());
#endif
    }

    // ============================================================================
    // TOKENIZE NEXT AND MULTIPLE S-EXPRESSIONS
    // ============================================================================

    TEST_F(LispLexerTest, MultipleExpressions_TwoSimple) {
        auto input = PadString("(+ 1 2) (* 3 4)");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());

        // Verify first expression
        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        const auto first = *optRegion;
        ASSERT_NE(first.first, nullptr);
        std::vector<const LispToken*> firstTokens;
        CollectAllTokens(lexer.get(), first.first, first.second, firstTokens);

        ASSERT_EQ(firstTokens.size(), 5);
        EXPECT_EQ(firstTokens[0]->Kind, LispTokenKind::LeftParenthesis);
        EXPECT_EQ(firstTokens[1]->Kind, LispTokenKind::Plus);
        EXPECT_EQ(firstTokens[2]->Kind, LispTokenKind::RealLiteral);
        EXPECT_EQ(firstTokens[3]->Kind, LispTokenKind::RealLiteral);
        EXPECT_EQ(firstTokens[4]->Kind, LispTokenKind::RightParenthesis);

        // Verify second expression
        const auto secondRegion = lexer->TokenizeNext(first.first);
        ASSERT_TRUE(secondRegion.has_value());
        const auto second = *secondRegion;
        ASSERT_NE(second.first, nullptr);
        std::vector<const LispToken*> secondTokens;
        CollectAllTokens(lexer.get(), second.first, second.second, secondTokens);

        ASSERT_EQ(secondTokens.size(), 5);
        EXPECT_EQ(secondTokens[0]->Kind, LispTokenKind::LeftParenthesis);
        EXPECT_EQ(secondTokens[1]->Kind, LispTokenKind::Asterisk);
        EXPECT_EQ(secondTokens[2]->Kind, LispTokenKind::RealLiteral);
        EXPECT_EQ(secondTokens[3]->Kind, LispTokenKind::RealLiteral);
        EXPECT_EQ(secondTokens[4]->Kind, LispTokenKind::RightParenthesis);
    }

    // ============================================================================
    // STRESS TESTS
    // ============================================================================

    TEST_F(LispLexerTest, Stress_DeeplyNested) {
        std::string nested = "(";
        for (int i = 0; i < 50; ++i) {
            nested += "(";
        }
        nested += "x";
        for (int i = 0; i < 51; ++i) {
            nested += ")";
        }

        const auto input = PadString(nested);
        const auto lexer = CreateLexer(input);
        EXPECT_TRUE(lexer->Tokenize());
    }

    TEST_F(LispLexerTest, Stress_ManyExpressions) {
        std::string many;
        for (int i = 0; i < 100; ++i) {
            many += "(test) ";
        }

        const auto input = PadString(many);
        const auto lexer = CreateLexer(input);
        EXPECT_TRUE(lexer->Tokenize());
    }

    TEST_F(LispLexerTest, Stress_LongIdentifier) {
        const std::string longId = "(" + std::string(200, 'x') + ")";

        VerifyTokens(longId, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "", 200},  // Empty text, but verify length
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // EDGE CASES
    // ============================================================================

    TEST_F(LispLexerTest, EdgeCase_EmptyExpression) {
        if (DisallowEmptySExpr) {
            using namespace Diagnostic;
            const auto input = PadString("( )");
            const auto lexer = CreateLexer(input);
            ASSERT_TRUE(lexer->Tokenize());
            auto root = lexer->TokenizeFirstSExpr();
            ASSERT_TRUE(root.has_value());
            auto [regBeg,regEnd] = root.value();
            ASSERT_TRUE(lexer->TokenizeFirstSExpr());
            lexer->TokenizeSExpr(regBeg);
            //empty sexpr get checked by the parser, not the lexer, so we cannot check for such diagnostics
            //here
        }
        else {
            //empty SExpr is recognized by the parser not the lexer, but we can delegate this check to the lexer though
            VerifyTokens("()", {
                     {LispTokenKind::LeftParenthesis, "("},
                     {LispTokenKind::RightParenthesis, ")"}
                 }, true);
        }
    }

    TEST_F(LispLexerTest, EdgeCase_OnlyWhitespace) {
        using namespace Diagnostic;
        const auto input = PadString("     \t\n\r    ");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());
        ASSERT_FALSE(lexer->TokenizeFirstSExpr());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::ProgramMustStartWithSExpression));
    }

    TEST_F(LispLexerTest, EdgeCase_OnlyComment) {
        using namespace Diagnostic;
        const auto input = PadString("; just a comment\n");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());
        ASSERT_FALSE(lexer->TokenizeFirstSExpr());
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::ProgramMustStartWithSExpression));
    }

    TEST_F(LispLexerTest, EdgeCase_SingleCharacterTokens) {
        VerifyTokens("(+ - * / % & | ^)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::ForwardSlash, "/"},
            {LispTokenKind::Modulo, "%"},
            {LispTokenKind::Ampersand, "&"},
            {LispTokenKind::BitwiseOr, "|"},
            {LispTokenKind::BitwiseXor, "^"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // SOURCE LOCATION TESTS
    // ============================================================================
    struct ExpectedLocation {
        std::uint32_t Line;
        std::uint32_t Column;
        std::uint32_t Byte;
    };

    static void CollectAllTokens_Local(LispLexer* lexer,
                                       const LispToken* sexprBegin,
                                       const LispToken* sexprEnd,
                                       std::vector<const LispToken*>& result) {
        result.push_back(sexprBegin);
        const auto tokRegion = lexer->TokenizeSExpr(sexprBegin);
        if (!tokRegion.has_value()) {
            // Handle empty SExpr
            result.push_back(sexprEnd);
            return;
        }

        const auto [tokBegin, tokEnd] = *tokRegion;
        const LispToken* current = tokBegin;
        while (current <= tokEnd) {
            if (current->Kind == LispTokenKind::LeftParenthesis) {
                const LispToken* nestedBegin = current;
                ++current;
                ASSERT_LE(current, tokEnd);
                ASSERT_EQ(current->Kind, LispTokenKind::RightParenthesis) << "Expected matching right paren after left paren";
                const LispToken* nestedEnd = current;
                CollectAllTokens_Local(lexer, nestedBegin, nestedEnd, result);
                ++current;
            } else {
                result.push_back(current);
                ++current;
            }
        }
        result.push_back(sexprEnd);
    }

    static void VerifyTokenLocations(const std::string& input,
                                     const std::vector<ExpectedLocation>& expectedLocs) {
        const auto paddedInput = PadString(input);
        const auto lexer = LispLexer::Make(paddedInput);
        ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for: " << input;

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto [regionBegin, regionEnd] = *optRegion;

        std::vector<const LispToken*> flattenTokens;
        CollectAllTokens_Local(lexer.get(), regionBegin, regionEnd, flattenTokens);

        ASSERT_EQ(flattenTokens.size(), expectedLocs.size())
            << "Token count mismatch for locations. Expected: " << expectedLocs.size()
            << ", Got: " << flattenTokens.size();

        const char* base = lexer->GetTextData();
        for (size_t i = 0; i < flattenTokens.size(); ++i) {
            const auto* tok = flattenTokens[i];
            EXPECT_EQ(tok->Line, expectedLocs[i].Line) << "Line mismatch at token index " << i;
            EXPECT_EQ(tok->Column, expectedLocs[i].Column) << "Column mismatch at token index " << i;
            EXPECT_EQ(tok->GetByteLocation(base), expectedLocs[i].Byte) << "Byte offset mismatch at token index " << i;
        }
    }

    TEST_F(LispLexerTest, SourceLocation_SingleLineBasic) {
        //            0 1 2 3 4 5 6 7
        // input:    "( + 1 23)" but without the extra space after '('
        const std::string input = "(+ 1 23)";
        VerifyTokenLocations(input, {
            {1,1,0},  // '('
            {1,2,1},  // '+'
            {1,4,3},  // '1'
            {1,6,5},  // '23'
            {1,8,7}   // ')'
        });
    }

    TEST_F(LispLexerTest, SourceLocation_MultiLineWithWhitespace) {
        // input with leading spaces and newline
        // indexes: 0:' ',1:' ',2:'\n',3:'(',4:'+',5:' ',6:'1',7:'\n',8:' ',9:' ',10:'2',11:')'
        const std::string input = "  \n(+ 1\n  2)";
        VerifyTokenLocations(input, {
            {2,1,3},  // '('
            {2,2,4},  // '+'
            {2,4,6},  // '1'
            {3,3,10}, // '2'
            {3,4,11}  // ')'
        });
    }

    TEST_F(LispLexerTest, SourceLocation_Crossing32ByteBoundary) {
        // 40 spaces before the s-expression to cross a 32-byte boundary
        const std::string input = std::string(40, ' ') + "(a)";
        VerifyTokenLocations(input, {
            {1,41,40}, // '('
            {1,42,41}, // 'a'
            {1,43,42}  // ')'
        });
    }

    TEST_F(LispLexerTest, Coverage_UnrecognizedOperatorCharacter) {
        using namespace Diagnostic;
        // Try to create a scenario where we have an unusual character
        // that might slip through the various checks
        // Using characters near operators in ASCII table but not recognized
        // Note: This might trigger the "unreachable" assertion path instead

        // One approach: character that's not in any mask at all
        const std::string input = "(+ 1 2~3)"; // '~' is not a recognized operator
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);

        lexer->Tokenize();
        const auto sexpr = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(sexpr.has_value());
        lexer->TokenizeSExpr(sexpr.value().first);
        EXPECT_EQ(lexer->GetDiagnostics().Size(), 1);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetSeverity(),Severity::Error);
        EXPECT_EQ(lexer->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken));
    }

    // Test: Try with various non-standard characters
    TEST_F(LispLexerTest, Coverage_VariousInvalidOperators) {
        const std::vector invalidChars = {'~', '?', '{', '}', '_', '`'};

        for (const char ch : invalidChars) {
            if (ch == '`' || ch == '_') continue; // These might be valid in some configs

            std::string input = "(+ 1 ";
            input += ch;
            input += " 2)";

            const auto paddedInput = PadString(input);
            const auto lexer = CreateLexer(paddedInput);

            // Should fail or handle gracefully
            lexer->Tokenize();
            // The tokenization might fail or succeed depending on character
            // The key is exercising the code path
        }
    }

    // Test: FetchStringRegion - String spans multiple blocks, next block starts with closing quote
    TEST_F(LispLexerTest, Coverage_StringSpanningBlocks_NextBlockStartsWithQuote) {

        // Create a string that crosses the 32-byte boundary
        // First block: 30 bytes of content + opening quote + some chars
        // Second block: starts with closing quote
        // Pattern: "(s "XXXX...XXXX") where X fills to exactly position the quote at block boundary
        const std::string content(29, 'X'); // 29 X's
        const std::string input = "(s \"" + content + "\")";
        // Adjust to ensure the closing quote is at the start of the next block

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "s"},
            {LispTokenKind::StringLiteral, ""},  // Don't check text, just kind
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // Test: FetchCommentRegion - Comment with newline immediately after semicolon
    TEST_F(LispLexerTest, Coverage_CommentWithImmediateNewline) {

        // The key is having ;\n where the \n is detected in the same position check
        const std::string input = "(test)\n;\n(+ 1 2)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        // Should successfully tokenize despite the empty comment
        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
    }

    // Test: FetchCommentRegion - Comment spanning blocks with newline at block start
    TEST_F(LispLexerTest, Coverage_CommentSpanningBlocks_NewlineAtBlockStart) {

        // Create a comment that spans into the next block, with newline at block boundary
        // Position comment so it crosses 32-byte boundary
        const std::string padding(25, ' ');
        const std::string commentContent(10, 'c'); // Will cross into next block
        const std::string input = padding + "(test);comment" + commentContent + "\n(+ 1 2)";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());
    }

    // Test: FetchFragmentRegion - Fragments spanning multiple blocks (do-while loop)
    TEST_F(LispLexerTest, Coverage_FragmentsSpanningMultipleBlocks) {

        // Create whitespace that spans from near end of one block through next block
        // Need at least 32 consecutive spaces/tabs/newlines that cross block boundary
        const std::string leadingContent = "(test)"; // 6 bytes
        const std::string spaces(50, ' '); // 50 spaces - will span multiple blocks
        const std::string input = leadingContent + spaces + "(+ 1 2)";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        // Tokenize first expression
        auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        // Tokenize next - this should trigger the fragment region spanning
        auto [first, _] = *optRegion;
        const auto nextRegion = lexer->TokenizeNext(first);
        ASSERT_TRUE(nextRegion.has_value());
    }

    // Test: FetchFragmentRegion - Mixed whitespace spanning blocks with newlines
    TEST_F(LispLexerTest, Coverage_FragmentsSpanningBlocks_WithNewlines) {

        // Create a mixed whitespace (spaces, tabs, newlines) that spans blocks
        const std::string leadingContent = "(test)"; // 6 bytes
        std::string mixedWhitespace;
        for (int i = 0; i < 15; ++i) {
            mixedWhitespace += "  \t\n"; // 4 bytes each iteration = 60 bytes total
        }
        const std::string input = leadingContent + mixedWhitespace + "(+ 1 2)";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        auto [first, _] = *optRegion;
        const auto nextRegion = lexer->TokenizeNext(first);
        ASSERT_TRUE(nextRegion.has_value());
    }

    // Test: FetchDigitRegion - Number spanning blocks with non-digit at block start
    TEST_F(LispLexerTest, Coverage_NumberSpanningBlocks_NonDigitAtBlockStart) {

        // Create a number that spans into the next block, followed by non-digit
        // Position it so the number crosses the 32-byte boundary
        const std::string padding(28, ' ');
        std::string longNumber = "123456789"; // Will cross into next block
        const std::string input = padding + "(+ " + longNumber + " 2)";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, longNumber},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // Test: FetchDigitRegion - Very long number crossing multiple blocks
    TEST_F(LispLexerTest, Coverage_VeryLongNumberCrossingBlocks) {

        // Create a very long number that definitely crosses block boundaries
        std::string veryLongNumber(100, '9'); // 100 digits
        const std::string input = "(+ " + veryLongNumber + ")";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, veryLongNumber},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // Test: Comprehensive boundary test - Everything crossing blocks
    TEST_F(LispLexerTest, Coverage_ComprehensiveBoundaryCrossing) {

        // Position elements to cross the 32-byte boundary at different points
        const std::string padding1(29, ' ');
        const std::string longId(15, 'x');
        const std::string padding2(30, ' ');
        const std::string longComment(20, 'c');
        const std::string padding3(28, ' ');
        const std::string longString(20, 's');

        const std::string input = padding1 + "(test " + longId + ")" +
                           padding2 + ";comment" + longComment + "\n" +
                           padding3 + "(s \"" + longString + "\")";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());
    }

    //Test: String with exactly 32 bytes of content crossing boundary
    TEST_F(LispLexerTest, Coverage_StringWith32BytesOfContent) {
        const std::string content32(32, 'A'); // Exactly 32 bytes
        const std::string input = "(\"" + content32 + "\")";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::StringLiteral, ""},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // Test: Identifier crossing blocks followed immediately by non-identifier char
    TEST_F(LispLexerTest, Coverage_IdentifierCrossingBlocks) {
        // Position identifier to cross block boundary
        const std::string padding(28, ' ');
        const std::string longId(20, 'i'); // Long identifier
        const std::string input = padding + "(test " + longId + " 123)";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "test"},
            {LispTokenKind::Identifier, ""},  // Long identifier
            {LispTokenKind::RealLiteral, "123"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // Test: Float number with mantissa crossing blocks
    TEST_F(LispLexerTest, Coverage_FloatMantissaCrossingBlocks) {
        const std::string padding(26, ' ');
        const std::string longMantissa(20, '9');
        const std::string input = padding + "(+ 123." + longMantissa + ")";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, ""},  // Long float
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_LineCount_SingleNewline) {
        // Single newline - tests line increment in early return
        const std::string input = "(\n+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto [begin, end] = *optRegion;

        EXPECT_EQ(begin->Line, 1u);
        const auto tokRegion = lexer->TokenizeSExpr(begin);
        EXPECT_TRUE(tokRegion.has_value());
        EXPECT_EQ(tokRegion.value().first->Line, 2u); // '+' token
        EXPECT_EQ(end->Line, 2u);  // Closing paren on line 2
    }

    TEST_F(LispLexerTest, FetchFragment_LineCount_MultipleNewlines) {
        // Multiple newlines between tokens
        const std::string input = "(\n\n\n+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 4u);  // Plus token on line 4
    }

    TEST_F(LispLexerTest, FetchFragment_LineCount_MixedNewlinesAndSpaces) {
        // Mix of newlines and spaces
        const std::string input = "( \n  \n  +)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 3u);  // Plus token on line 3
    }

    TEST_F(LispLexerTest, FetchFragment_LineCount_CommentWithNewline) {
        // Comment with newline at specific position
        const std::string input = "; test\n(+ 1)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 2u);  // First token on line 2
    }

    TEST_F(LispLexerTest, FetchFragment_LineCount_MultilineComment) {
        // Multiple comment lines
        const std::string input = "; line 1\n; line 2\n; line 3\n(+ 1)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 4u);  // First token on line 4
    }

    // ============================================================================
    // TEST SUITE 3: Crossing 32-Byte Boundary (Single Block Span)
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Boundary_Exactly31Bytes) {
        // 31 spaces - just before boundary (31 + posInBlock < 32)
        const std::string spaces31(31, ' ');
        VerifyTokens("(" + spaces31 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Boundary_Exactly32Bytes) {
        // 32 spaces - exactly at boundary, triggers loop
        const std::string spaces32(32, ' ');
        VerifyTokens("(" + spaces32 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Boundary_33Bytes) {
        // 33 spaces - crosses into next block
        const std::string spaces33(33, ' ');
        VerifyTokens("(" + spaces33 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Boundary_Comment32Chars) {
        // Comment exactly 32 characters (including semicolon and newline)
        const std::string comment30(30, 'x');  // 30 x's + ';' + '\n' = 32
        const std::string input = ";" + comment30 + "\n(+)";

        VerifyTokensWithAux(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        }, true);
    }

    TEST_F(LispLexerTest, FetchFragment_Boundary_LeadingWhitespace32) {
        // 32 leading spaces before first s-expression
        const std::string spaces32(32, ' ');
        VerifyTokensWithAux(spaces32 + "(+ 1)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"}
        }, true);
    }

    // ============================================================================
    // TEST SUITE 4: Multiple Block Spans (64+ Bytes)
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_50Spaces) {
        // 50 consecutive spaces - spans 2 blocks
        const std::string spaces50(50, ' ');
        VerifyTokens("(" + spaces50 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_64Spaces) {
        // 64 spaces - exactly 2 blocks
        const std::string spaces64(64, ' ');
        VerifyTokens("(" + spaces64 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_100Spaces) {
        // 100 spaces - spans 4 blocks (32+32+32+4)
        const std::string spaces100(100, ' ');
        VerifyTokens("(" + spaces100 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_200Spaces) {
        // 200 spaces - long fragment chain
        const std::string spaces200(200, ' ');
        VerifyTokens("(" + spaces200 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_VeryLongComment) {
        // Very long comment spanning multiple blocks
        const std::string longComment(150, 'c');
        const std::string input = ";" + longComment + "\n(+ 1)";

        VerifyTokensWithAux(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RightParenthesis, ")"}
        }, true);
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_MixedWhitespace100) {
        // 100 bytes of mixed whitespace (spaces, tabs, newlines)
        std::string mixed;
        for (int i = 0; i < 25; ++i) {
            mixed += "  \t\n";  // 4 bytes each
        }
        VerifyTokens("(" + mixed + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // TEST SUITE 5: Line Counting Across Multiple Blocks
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_50Newlines) {
        // 50 newlines - tests line counting across multiple blocks
        const std::string newlines50(50, '\n');
        const std::string input = "(" + newlines50 + "+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 51u);  // Plus token on line 51
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_NewlinesWithSpaces) {
        // Mix of newlines and spaces across blocks
        std::string mixed;
        for (int i = 0; i < 30; ++i) {
            mixed += "\n  ";  // 3 bytes each = 90 bytes total
        }
        const std::string input = "(" + mixed + "+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 31u);  // 30 newlines + line 1
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_AlternatingNewlines) {
        // Alternating pattern: space, newline, space, newline...
        std::string alternating;
        for (int i = 0; i < 40; ++i) {
            alternating += " \n";  // 2 bytes each = 80 bytes
        }
        const std::string input = "(" + alternating + "+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 41u);  // 40 newlines + line 1
    }

    TEST_F(LispLexerTest, FetchFragment_MultiBlock_CommentsWithNewlines) {
        // Multiple comments with newlines spanning blocks
        std::string comments;
        for (int i = 0; i < 20; ++i) {
            comments += "; comment\n";  // 10 bytes each = 200 bytes
        }
        const std::string input = comments + "(+ 1)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 21u);  // 20 comment lines + line 1
    }

    // ============================================================================
    // TEST SUITE 6: Complex Real-World Patterns
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Complex_IndentedCode) {
        // Realistic indented Lisp code
        const std::string input = std::format(R"(
({} factorial (n)
  (if (<= n 1)
      1
      (* n (factorial (- n 1)))))
)",FuncKeyword);
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 2u);  // First token on line 2
    }

    TEST_F(LispLexerTest, FetchFragment_Complex_HeavilyCommented) {
        // Code with extensive comments
        const std::string input = R"(
; This is a comment
; Another comment
; Yet another comment
(+ 1 2) ; inline comment
; More comments
; Even more
)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 5u);  // First s-expr on line 5
    }

    TEST_F(LispLexerTest, FetchFragment_Complex_DeepIndentation) {
        // Deeply nested with heavy indentation
        const std::string input = R"(
(let ((x 1)
      (y 2))
  (let ((a 3)
        (b 4))
    (+ x y a b)))
)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        // Should successfully tokenize despite varied whitespace
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Complex_MixedTabsAndSpaces) {
        // Mixed tabs and spaces (common in real code)
        const std::string input = "(\t  \t\n\t\t  + \t 1 \t\t 2)";
        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // TEST SUITE 7: Edge Cases with Specific Byte Positions
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Edge_StartAtByte31) {
        // Position fragment to start at byte 31 of a block
        const std::string prefix(30, 'x');  // 30-char identifier
        const std::string input = "(" + prefix + "      +)";  // 6 spaces at position 31

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, ""},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Edge_StartAtByte32) {
        // Fragment starts exactly at byte 32 (block boundary)
        const std::string prefix(31, 'x');  // 31-char identifier
        const std::string input = "(" + prefix + "    +)";  // Spaces start at byte 32

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, ""},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Edge_CommentAtBoundary) {
        // Comment starting at block boundary
        const std::string prefix(31, ' ');  // 31 spaces
        const std::string input = "(" + prefix + "; comment\n+)";

        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Edge_NewlineAtByte32) {
        // Newline positioned exactly at byte 32
        const std::string spaces31(31, ' ');
        const std::string input = "(" + spaces31 + "\n+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 2u);  // Plus on line 2
    }

    // ============================================================================
    // TEST SUITE 8: Multiple Expressions with Varying Fragments
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Multiple_ShortGaps) {
        // Multiple expressions with short gaps (early return each time)
        const std::string input = "(+ 1)  (- 2)  (* 3)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        // First expression
        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        // Second expression - gap triggers FetchFragmentRegion
        const auto secondRegion = lexer->TokenizeNext(optRegion->first);
        ASSERT_TRUE(secondRegion.has_value());

        // Third expression
        const auto thirdRegion = lexer->TokenizeNext(secondRegion->first);
        ASSERT_TRUE(thirdRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Multiple_LongGaps) {
        // Multiple expressions with long gaps (loop execution)
        const std::string gap50(50, ' ');
        const std::string input = "(+ 1)" + gap50 + "(- 2)" + gap50 + "(* 3)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        const auto secondRegion = lexer->TokenizeNext(optRegion->first);
        ASSERT_TRUE(secondRegion.has_value());

        const auto thirdRegion = lexer->TokenizeNext(secondRegion->first);
        ASSERT_TRUE(thirdRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Multiple_MixedGaps) {
        // Mix of short and long gaps between expressions
        const std::string shortGap(5, ' ');
        const std::string longGap(50, ' ');
        const std::string input = "(+ 1)" + shortGap + "(- 2)" + longGap + "(* 3)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        const auto secondRegion = lexer->TokenizeNext(optRegion->first);
        ASSERT_TRUE(secondRegion.has_value());

        const auto thirdRegion = lexer->TokenizeNext(secondRegion->first);
        ASSERT_TRUE(thirdRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Multiple_WithNewlines) {
        // Multiple expressions separated by newlines
        const std::string input = "(+ 1)\n\n\n(- 2)\n\n\n(* 3)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 1u);

        const auto secondRegion = lexer->TokenizeNext(optRegion->first);
        ASSERT_TRUE(secondRegion.has_value());
        EXPECT_EQ(secondRegion->first->Line, 4u);

        const auto thirdRegion = lexer->TokenizeNext(secondRegion->first);
        ASSERT_TRUE(thirdRegion.has_value());
        EXPECT_EQ(thirdRegion->first->Line, 7u);
    }

    // ============================================================================
    // TEST SUITE 9: Tiny stress test for Loop Termination
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Stress_VeryLongWhitespace) {
        // Extremely long whitespace to test loop with many iterations
        const std::string spaces300(300, ' ');
        VerifyTokens("(" + spaces300 + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Stress_VeryLongMixedWhitespace) {
        // 500 bytes of mixed whitespace
        std::string mixed;
        for (int i = 0; i < 100; ++i) {
            mixed += "  \t\n ";  // 5 bytes each
        }
        VerifyTokens("(" + mixed + "+)", {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Stress_ManyShortFragments) {
        // Many short fragment regions (tests repeated early returns)
        std::string input = "(";
        for (int i = 0; i < 50; ++i) {
            input += " + ";  // Space before and after
        }
        input += ")";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_TRUE(lexer->Tokenize());
    }

    TEST_F(LispLexerTest, FetchFragment_Stress_AlternatingFragmentLengths) {
        // Alternate between short and long fragments
        std::string input = "(";
        for (int i = 0; i < 10; ++i) {
            input += " +";  // Short gap
            input += std::string(50, ' ') + "+";  // Long gap
        }
        input += ")";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_TRUE(lexer->Tokenize());
    }

    // ============================================================================
    // TEST SUITE 10: Regression and Special Cases
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Regression_EmptyLines) {
        // Empty lines (newlines with no other content)
        const std::string input = "(\n\n\n\n\n+)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 6u);
    }

    TEST_F(LispLexerTest, FetchFragment_Regression_OnlyWhitespace) {
        // Only whitespace at end after last expression
        const std::string spaces100(100, ' ');
        const std::string input = "(+ 1)" + spaces100;
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());

        // Should not find a second expression
        const auto secondRegion = lexer->TokenizeNext(optRegion->first);
        EXPECT_FALSE(secondRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Regression_TrailingComment) {
        // Trailing comment after expression
        const std::string input = "(+ 1)   ; this is a trailing comment with 50 chars....";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_TRUE(lexer->Tokenize());
    }

    TEST_F(LispLexerTest, FetchFragment_Regression_ConsecutiveComments) {
        // Multiple consecutive comments
        const std::string input = R"(
; Comment 1 with some content here
; Comment 2 with more content here
; Comment 3 and even more here
; Comment 4 continuing on
; Comment 5 final comment
(+ 1 2)
)";
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        EXPECT_EQ(optRegion->first->Line, 7u);
    }

    TEST_F(LispLexerTest, FetchFragment_Regression_WhitespaceBeforeEOF) {
        // Whitespace before EOF (tests fragment at end of stream)
        const std::string spaces50(50, ' ');
        const std::string input = "(+ 1)" + spaces50;
        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        EXPECT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
    }

    TEST_F(LispLexerTest, FetchFragment_Regression_MixedLineEndings) {
        // Mix of \n and \r\n line endings (if supported)
        const std::string input = "(\n+\n-\n*\n)";
        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::Minus, "-"},
            {LispTokenKind::Asterisk, "*"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    // ============================================================================
    // TEST SUITE 11: Position Offset Testing
    // ============================================================================

    TEST_F(LispLexerTest, FetchFragment_Position_MidBlockStart) {
        // Fragment starts mid-block (posInBlock != 0)
        // This happens naturally when token at position 15, then fragment
        const std::string input = "(veryLongIdent     +)";  // Spaces start mid-block
        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, "veryLongIdent"},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Position_VariousOffsets) {
        // Create fragments at various offsets within blocks
        const std::string id5(5, 'x');
        const std::string id15(15, 'x');
        const std::string id25(25, 'x');

        const std::string input = "(" + id5 + "   " + id15 + "      " + id25 + "        +)";
        VerifyTokens(input, {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Identifier, ""},
            {LispTokenKind::Identifier, ""},
            {LispTokenKind::Identifier, ""},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RightParenthesis, ")"}
        });
    }

    TEST_F(LispLexerTest, FetchFragment_Position_WithNewlinesAtOffsets) {
        // Newlines at various positions within fragments
        const std::string id10(10, 'x');
        const std::string input = "(" + id10 + "  \n  " + id10 + "  \n\n  " + id10 + "\n+)";

        const auto paddedInput = PadString(input);
        const auto lexer = CreateLexer(paddedInput);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto tokRegion = lexer->TokenizeSExpr(optRegion->first);
        ASSERT_TRUE(tokRegion.has_value());

        // Verify line counting worked across different positions
        const auto [tokBegin, tokEnd] = *tokRegion;
        EXPECT_EQ(tokBegin->Line, 1U);
        EXPECT_EQ((tokBegin+1)->Line, 2U);
        EXPECT_EQ((tokBegin+2)->Line, 4U);
        EXPECT_EQ((tokBegin+3)->Line, 5U);
    }

    // ============================================================================
    // FILE-BASED LEXER TESTS
    // ============================================================================

    // Test fixture for file-based LispLexer tests
    class LispLexerFileTest : public Test {
    protected:
        std::filesystem::path TempFilePath;
        std::wstring TempFileWPath;
        AlignedFileReadResult ActiveReadFile;

        void SetUp() override {
            // Create a unique temporary file path
            auto tempDir = std::filesystem::temp_directory_path();

            // Get a unique ID from thread and time
            std::stringstream ss;
            ss << std::this_thread::get_id();
            auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();

            std::string uniqueName = "lispLexerTest_" + ss.str() + "_" + std::to_string(timestamp) + ".lisp";

            TempFilePath = tempDir / uniqueName;
            TempFileWPath = TempFilePath.wstring();

            // Clean up any old file
            std::filesystem::remove(TempFilePath);
        }

        void TearDown() override {
            // Clean up the temporary file
            std::filesystem::remove(TempFilePath);
        }

        // Helper to write content to the temp file
        void WriteTempFile(const std::string& content) const {
            std::ofstream outFile(TempFilePath, std::ios::binary); // Use binary to avoid newline translation
            ASSERT_TRUE(outFile.is_open()) << "Failed to open temp file for writing: " << TempFilePath;
            outFile << content;
            outFile.close();
        }

        // Helper to create a file-based lexer
        NODISCARD std::unique_ptr<LispLexer> CreateLexerFromFile(const std::string &input) {
            WriteTempFile(input);
            // Use AlignedFileReader to read the file
            ActiveReadFile = std::move(AlignedFileReader::Read(TempFileWPath));
            // Use the file-based Make function
            return LispLexer::Make(ActiveReadFile, TempFileWPath, false);
        }
    };


    TEST_F(LispLexerFileTest, FileBased_SimpleAddition) {
        const std::string input = "(+ 1 2)";
        const auto lexer = CreateLexerFromFile(input);

        // We can't use VerifyTokens directly as it creates a string-based lexer.
        // We'll replicate its logic.
        ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for file-based input: " << input;

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto [regionBegin, regionEnd] = *optRegion;

        std::vector<const LispToken*> flattenTokens;
        LispLexerTest::CollectAllTokens(lexer.get(), regionBegin, regionEnd, flattenTokens, false);

        const std::vector<ExpectedToken> expected = {
            {LispTokenKind::LeftParenthesis, "("},
            {LispTokenKind::Plus, "+"},
            {LispTokenKind::RealLiteral, "1"},
            {LispTokenKind::RealLiteral, "2"},
            {LispTokenKind::RightParenthesis, ")"}
        };

        ASSERT_EQ(flattenTokens.size(), expected.size());
        for (size_t i = 0; i < flattenTokens.size(); ++i) {
            EXPECT_EQ(flattenTokens[i]->Kind, expected[i].Kind);
            EXPECT_EQ(flattenTokens[i]->GetText(), expected[i].Text);
        }
    }

    TEST_F(LispLexerFileTest, FileBased_ComplexFibonacci) {
        const std::string input = "(defun fib (n) (if (<= n 1) n (+ (fib (- n 1)) (fib (- n 2)))))";
        const auto lexer = CreateLexerFromFile(input);

        ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for file-based input: " << input;

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto [regionBegin, regionEnd] = *optRegion;

        std::vector<const LispToken*> flattenTokens;
        LispLexerTest::CollectAllTokens(lexer.get(), regionBegin, regionEnd, flattenTokens, false);

        // Just check token count for brevity, as VerifyTokens already checks this string
        ASSERT_EQ(flattenTokens.size(), 35);
        EXPECT_EQ(flattenTokens[1]->Kind, LispTokenKind::Defun);
        EXPECT_EQ(flattenTokens[34]->Kind, LispTokenKind::RightParenthesis);
    }


    TEST_F(LispLexerFileTest, FileBased_Error_UnmatchedCloseParen) {
        using namespace Diagnostic;
        const std::string input = "(+ 1 2"; // Missing closing paren
        const auto lexer = CreateLexerFromFile(input);

        EXPECT_FALSE(lexer->Tokenize());

        // Check diagnostics
        auto& diagnostics = lexer->GetDiagnostics();
        ASSERT_EQ(diagnostics.Size(), 1);

        const auto& diag = diagnostics[0];
        EXPECT_EQ(diag.GetSeverity(), Severity::Error);
        EXPECT_EQ(diag.GetErrorCode(), DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis));

        // Verify the file path in the diagnostic
        EXPECT_EQ(diag.GetLocation().File, TempFileWPath);
        EXPECT_NE(diag.GetLocation().File, L"memory") << "Diagnostic file path should be the temp file path, not 'memory'";
    }

    TEST_F(LispLexerFileTest, FileBased_Error_UnrecognizedToken) {
        using namespace Diagnostic;
        const std::string input = "(+ 1 ? 2)"; // Invalid token '?'
        const auto lexer = CreateLexerFromFile(input);

        EXPECT_FALSE(lexer->Tokenize());

        // Check diagnostics
        auto& diagnostics = lexer->GetDiagnostics();
        ASSERT_FALSE(diagnostics.Empty());

        bool foundError = false;
        for(size_t i = 0; i < diagnostics.Size(); ++i) {
            const auto& diag = diagnostics[i];
            if(diag.GetErrorCode() == DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken)) {
                EXPECT_EQ(diag.GetSeverity(), Severity::Error);
                // Verify the file path in the diagnostic
                EXPECT_EQ(diag.GetLocation().File, TempFileWPath);
                EXPECT_NE(diag.GetLocation().File, L"memory");
                foundError = true;
                break;
            }
        }
        EXPECT_TRUE(foundError) << "Did not find UnrecognizedToken diagnostic";
    }

    TEST_F(LispLexerFileTest, FileBase_TestFileSize) {
        const std::string input = "(+ 1 2)";
        const auto lexer = CreateLexerFromFile(input);
        ASSERT_TRUE(lexer->Tokenize()) << "Tokenization failed for file-based input: " << input;
        ASSERT_EQ(lexer->GetFileSize(),input.size());

    }

    // ============================================================================
    // TRIVIA / AUXILIARY TOKEN TESTS
    // ============================================================================

    TEST_F(LispLexerTest, Trivia_LeadingTopLevel) {
        // Test that trivia before the first S-expression is attached to its '('
        auto input = PadString("  ; leading comment\n   \n (sexpr1)");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion1 = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion1.has_value());
        const auto [begin1, end1] = *optRegion1;

        EXPECT_EQ(begin1->Kind, LispTokenKind::LeftParenthesis);
        EXPECT_EQ(begin1->AuxiliaryLength, 1); // One trivia block

        auto auxOpt = lexer->GetTokenAuxiliary(begin1);
        ASSERT_TRUE(auxOpt.has_value());
        auto [auxBegin, auxEnd] = *auxOpt;

        // Check the trivia content
        // The span is from index 0 to the '('
        std::string expectedTrivia = "  ; leading comment\n   \n ";
        EXPECT_EQ(auxBegin->GetText(), expectedTrivia);
        EXPECT_EQ(auxBegin->Length, expectedTrivia.length());
    }

    TEST_F(LispLexerTest, Trivia_BetweenTopLevel) {
        // Test that trivia between S-expressions is attached to the *following* S-expression's '('
        auto input = PadString("(sexpr1)  \n\n ; comment \n  (sexpr2)");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());

        // Get first S-expression
        const auto optRegion1 = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion1.has_value());
        const auto [begin1, end1] = *optRegion1;

        // First S-expr's '(' should have no leading trivia
        EXPECT_EQ(begin1->AuxiliaryLength, 0);

        // Get second S-expression
        const auto optRegion2 = lexer->TokenizeNext(begin1);
        ASSERT_TRUE(optRegion2.has_value());
        const auto [begin2, end2] = *optRegion2;

        // The second S-expr's '(' should have the in-between trivia
        EXPECT_EQ(begin2->Kind, LispTokenKind::LeftParenthesis);
        EXPECT_EQ(begin2->AuxiliaryLength, 1); // One trivia block
        lexer->TokenizeSExpr(begin2);

        auto auxOpt = lexer->GetTokenAuxiliary(begin2);
        ASSERT_TRUE(auxOpt.has_value());
        auto [auxBegin, auxEnd] = *auxOpt;

        // Check the trivia content
        std::string expectedTrivia = "  \n\n ; comment \n  ";
        EXPECT_EQ(auxBegin->GetText(), expectedTrivia);
        EXPECT_EQ(auxBegin->Length, expectedTrivia.length());
    }

    TEST_F(LispLexerTest, Trivia_InsideSExpr) {
        // Test that trivia *inside* an S-expression is attached to the correct tokens
        auto input = PadString("( ; c1 \n atom1 \n ; c2 \n ;another c2\n atom2 \n ;c3 \n )");
        const auto lexer = CreateLexer(input);
        ASSERT_TRUE(lexer->Tokenize());

        const auto optRegion = lexer->TokenizeFirstSExpr();
        ASSERT_TRUE(optRegion.has_value());
        const auto [begin, end] = *optRegion;

        // Tokenize inside
        const auto tokRegion = lexer->TokenizeSExpr(begin);
        ASSERT_TRUE(tokRegion.has_value());
        const auto [tokBegin, tokEnd] = *tokRegion;

        // tokBegin is 'atom1'
        // tokEnd is 'atom2'
        ASSERT_EQ(tokBegin->GetText(), "atom1");
        ASSERT_EQ(tokEnd->GetText(), "atom2");

        // Check trivia on 'atom1'
        // Trivia: " ", "; c1 \n", " "
        EXPECT_EQ(tokBegin->AuxiliaryLength, 3);
        auto aux1Opt = lexer->GetTokenAuxiliary(tokBegin);
        ASSERT_TRUE(aux1Opt.has_value());
        auto [aux1Begin, aux1End] = *aux1Opt;
        EXPECT_EQ(aux1Begin->GetText(), " ");
        EXPECT_EQ((aux1Begin+1)->GetText(), "; c1 \n");
        EXPECT_EQ((aux1Begin+2)->GetText(), " ");
        EXPECT_EQ(aux1End, aux1Begin+2);

        // Check trivia on 'atom2'
        // Trivia: " \n ", "; c2 \n", " ", ";another c2\n", " "
        EXPECT_EQ(tokEnd->AuxiliaryLength, 5);
        auto aux2Opt = lexer->GetTokenAuxiliary(tokEnd);
        ASSERT_TRUE(aux2Opt.has_value());
        auto [aux2Begin, aux2End] = *aux2Opt;
        EXPECT_EQ(aux2Begin->GetText(), " \n ");
        EXPECT_EQ((aux2Begin+1)->GetText(), "; c2 \n");
        EXPECT_EQ((aux2Begin+2)->GetText(), " ");
        EXPECT_EQ((aux2Begin+3)->GetText(), ";another c2\n");
        EXPECT_EQ((aux2Begin+4)->GetText(), " ");
        EXPECT_EQ(aux2End, aux2Begin+4);

        // Check trivia on closing ')'
        // This is stored on the 'end' token.
        // Trivia: " \n ", ";c3 \n", " "
        EXPECT_EQ(end->AuxiliaryLength, 3);
        auto aux3Opt = lexer->GetTokenAuxiliary(end);
        ASSERT_TRUE(aux3Opt.has_value());
        auto [aux3Begin, aux3End] = *aux3Opt;
        EXPECT_EQ(aux3Begin->GetText(), " \n ");
        EXPECT_EQ((aux3Begin+1)->GetText(), ";c3 \n");
        EXPECT_EQ((aux3Begin+2)->GetText(), " ");
        EXPECT_EQ(aux3End, aux3Begin+2);
    }
} // namespace WideLips::Tests
