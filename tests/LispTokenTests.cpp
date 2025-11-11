#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LispLexer.h"

using namespace WideLips;
using namespace testing;
using std::string_view_literals::operator ""sv;

namespace WideLips::Tests {
    class LispTokenTest : public Test {
    protected:
        void SetUp() override {
            // Setup common test data if needed
        }

        void TearDown() override {
            // Cleanup if needed
        }
    };

    TEST_F(LispTokenTest, InitializationWithTextPointer) {
        const auto text = "test";
        constexpr std::uint32_t line = 5;
        constexpr std::uint32_t column = 10;
        constexpr std::uint32_t length = 4;
        constexpr auto kind = LispTokenKind::Identifier;

        const LispToken token{text, line, length, 0, column, 0, kind, 0};

        EXPECT_EQ(token.GetText(), "test");
        EXPECT_EQ(token.Line, line);
        EXPECT_EQ(token.Column, column);
        EXPECT_EQ(token.Length, length);
        EXPECT_EQ(token.Kind, kind);
    }

    TEST_F(LispTokenTest, DefaultInitialization) {
        constexpr LispToken token{};

        EXPECT_EQ(token.TextPtr, nullptr);
        EXPECT_EQ(token.Line, 1);
        EXPECT_EQ(token.Column, 1);
        EXPECT_EQ(token.Length, 1);
        EXPECT_EQ(token.Kind, LispTokenKind::Invalid);
    }

    TEST_F(LispTokenTest, GetTextFunction) {
        const auto text = "identifier";
        const LispToken token{text, 1, 10, 0, 1, 0, LispTokenKind::Identifier, 0};

        EXPECT_EQ(token.GetText(), "identifier");
        EXPECT_EQ(token.GetText().length(), 10);
    }

    TEST_F(LispTokenTest, MatchFunction) {
        const auto text = "test";
        const LispToken token{text, 1, 4, 0, 1, 0, LispTokenKind::Identifier, 0};

        EXPECT_TRUE(token.Match(LispTokenKind::Identifier));
        EXPECT_FALSE(token.Match(LispTokenKind::StringLiteral));
        EXPECT_FALSE(token.Match(LispTokenKind::RealLiteral));
    }

    TEST_F(LispTokenTest, IsOperatorFunction) {
        // Test arithmetic operators
        LispToken plusToken{"+", 1, 1, 0, 1, 0, LispTokenKind::Plus, 0};
        EXPECT_TRUE(plusToken.IsOperator());

        LispToken minusToken{"-", 1, 1, 0, 1, 0, LispTokenKind::Minus, 0};
        EXPECT_TRUE(minusToken.IsOperator());

        LispToken asteriskToken{"*", 1, 1, 0, 1, 0, LispTokenKind::Asterisk, 0};
        EXPECT_TRUE(asteriskToken.IsOperator());

        LispToken slashToken{"/", 1, 1, 0, 1, 0, LispTokenKind::ForwardSlash, 0};
        EXPECT_TRUE(slashToken.IsOperator());

        LispToken moduloToken{"%", 1, 1, 0, 1, 0, LispTokenKind::Modulo, 0};
        EXPECT_TRUE(moduloToken.IsOperator());

        // Test comparison operators
        LispToken equalToken{"=", 1, 1, 0, 1, 0, LispTokenKind::Equal, 0};
        EXPECT_TRUE(equalToken.IsOperator());

        LispToken lessThanToken{"<", 1, 1, 0, 1, 0, LispTokenKind::LessThan, 0};
        EXPECT_TRUE(lessThanToken.IsOperator());

        LispToken greaterThanToken{">", 1, 1, 0, 1, 0, LispTokenKind::GreaterThan, 0};
        EXPECT_TRUE(greaterThanToken.IsOperator());

        LispToken lessThanOrEqualToken{"<=", 1, 2, 0, 1, 0, LispTokenKind::LessThanOrEqual, 0};
        EXPECT_TRUE(lessThanOrEqualToken.IsOperator());

        LispToken greaterThanOrEqualToken{">=", 1, 2, 0, 1, 0, LispTokenKind::GreaterThanOrEqual, 0};
        EXPECT_TRUE(greaterThanOrEqualToken.IsOperator());

        // Test logical operators
        LispToken notToken{"!", 1, 1, 0, 1, 0, LispTokenKind::Not, 0};
        EXPECT_TRUE(notToken.IsOperator());

        LispToken ampersandToken{"&", 1, 1, 0, 1, 0, LispTokenKind::Ampersand, 0};
        EXPECT_TRUE(ampersandToken.IsOperator());

        LispToken quoteToken{"'", 1, 1, 0, 1, 0, LispTokenKind::Quote, 0};
        EXPECT_TRUE(quoteToken.IsOperator());

        LispToken dotToken{".", 1, 1, 0, 1, 0, LispTokenKind::Dot, 0};
        EXPECT_TRUE(dotToken.IsOperator());

        // Test bitwise operators
        LispToken xorToken{"^", 1, 1, 0, 1, 0, LispTokenKind::BitwiseXor, 0};
        EXPECT_TRUE(xorToken.IsOperator());

        LispToken orToken{"|", 1, 1, 0, 1, 0, LispTokenKind::BitwiseOr, 0};
        EXPECT_TRUE(orToken.IsOperator());

        LispToken leftShiftToken{"<<", 1, 2, 0, 1, 0, LispTokenKind::LeftBitShift, 0};
        EXPECT_TRUE(leftShiftToken.IsOperator());

        LispToken rightShiftToken{">>", 1, 2, 0, 1, 0, LispTokenKind::RightBitShift, 0};
        EXPECT_TRUE(rightShiftToken.IsOperator());

        // Test non-operators
        LispToken identifierToken{"identifier", 1, 10, 0, 1, 0, LispTokenKind::Identifier, 0};
        EXPECT_FALSE(identifierToken.IsOperator());

        LispToken leftParenToken{"(", 1, 1, 0, 1, 0, LispTokenKind::LeftParenthesis, 0};
        EXPECT_FALSE(leftParenToken.IsOperator());

        LispToken rightParenToken{")", 1, 1, 0, 1, 0, LispTokenKind::RightParenthesis, 0};
        EXPECT_FALSE(rightParenToken.IsOperator());

        LispToken literalToken{"123", 1, 3, 0, 1, 0, LispTokenKind::RealLiteral, 0};
        EXPECT_FALSE(literalToken.IsOperator());
    }

    TEST_F(LispTokenTest, IsDialectSpecialFunction) {
        // Test dialect specials based on which preprocessor defines are enabled

#ifdef EnableHash
        LispToken hashToken{"#", 1, 1, 0, 1, 0, LispTokenKind::Hash, 0};
        bool hashResult = hashToken.IsDialectSpecial();
        EXPECT_TRUE(hashResult);
#else
        LispToken hashToken{"#", 1, 1, 0, 1, 0, LispTokenKind::Hash, 0};
        bool hashResult = hashToken.IsDialectSpecial();
        EXPECT_FALSE(hashResult);
#endif

#ifdef EnableComma
        LispToken commaToken{",", 1, 1, 0, 1, 0, LispTokenKind::Comma, 0};
        bool commaResult = commaToken.IsDialectSpecial();
        EXPECT_TRUE(commaResult);
#else
        LispToken commaToken{",", 1, 1, 0, 1, 0, LispTokenKind::Comma, 0};
        bool commaResult = commaToken.IsDialectSpecial();
        EXPECT_FALSE(commaResult);
#endif

#ifdef EnableQuasiColumn
        LispToken quasiToken{"`", 1, 1, 0, 1, 0, LispTokenKind::QuasiColumn, 0};
        bool quasiResult = quasiToken.IsDialectSpecial();
        EXPECT_TRUE(quasiResult);
#else
        LispToken quasiToken{"`", 1, 1, 0, 1, 0, LispTokenKind::QuasiColumn, 0};
        bool quasiResult = quasiToken.IsDialectSpecial();
        EXPECT_FALSE(quasiResult);
#endif

#ifdef EnableColumn
        LispToken colonToken{":", 1, 1, 0, 1, 0, LispTokenKind::Column, 0};
        bool colonResult = colonToken.IsDialectSpecial();
        EXPECT_TRUE(colonResult);
#else
        LispToken colonToken{":", 1, 1, 0, 1, 0, LispTokenKind::Column, 0};
        bool colonResult = colonToken.IsDialectSpecial();
        EXPECT_FALSE(colonResult);
#endif

#ifdef EnableAtSign
        LispToken atToken{"@", 1, 1, 0, 1, 0, LispTokenKind::At, 0};
        bool atResult = atToken.IsDialectSpecial();
        EXPECT_TRUE(atResult);
#else
        LispToken atToken{"@", 1, 1, 0, 1, 0, LispTokenKind::At, 0};
        bool atResult = atToken.IsDialectSpecial();
        EXPECT_FALSE(atResult);
#endif

#ifdef EnableBenjamin
        LispToken dollarToken{"$", 1, 1, 0, 1, 0, LispTokenKind::Dollar, 0};
        bool dollarResult = dollarToken.IsDialectSpecial();
        EXPECT_TRUE(dollarResult);
#else
        LispToken dollarToken{"$", 1, 1, 0, 1, 0, LispTokenKind::Dollar, 0};
        bool dollarResult = dollarToken.IsDialectSpecial();
        EXPECT_FALSE(dollarResult);
#endif

#ifdef EnableTilda
        LispToken tildaToken{"~", 1, 1, 0, 1, 0, LispTokenKind::Tilda, 0};
        bool tildaResult = tildaToken.IsDialectSpecial();
        EXPECT_TRUE(tildaResult);
#else
        LispToken tildaToken{"~", 1, 1, 0, 1, 0, LispTokenKind::Tilda, 0};
        bool tildaResult = tildaToken.IsDialectSpecial();
        EXPECT_FALSE(tildaResult);
#endif

        // Test non-dialect specials - these should always be false
        LispToken identifierToken{"identifier", 1, 10, 0, 1, 0, LispTokenKind::Identifier, 0};
        bool identifierResult = identifierToken.IsDialectSpecial();
        EXPECT_FALSE(identifierResult);

        LispToken plusToken{"+", 1, 1, 0, 1, 0, LispTokenKind::Plus, 0};
        bool plusResult = plusToken.IsDialectSpecial();
        EXPECT_FALSE(plusResult);

        LispToken leftParenToken{"(", 1, 1, 0, 1, 0, LispTokenKind::LeftParenthesis, 0};
        bool leftParenResult = leftParenToken.IsDialectSpecial();
        EXPECT_FALSE(leftParenResult);

        LispToken minusToken{"-", 1, 1, 0, 1, 0, LispTokenKind::Minus, 0};
        bool minusResult = minusToken.IsDialectSpecial();
        EXPECT_FALSE(minusResult);
    }

    TEST_F(LispTokenTest, IsKeywordOperatorFunction) {
        // Test keyword operators
        LispToken letToken{"let", 1, 3, 0, 1, 0, LispTokenKind::Let, 0};
        EXPECT_TRUE(letToken.IsKeywordOperator());

        LispToken lambdaToken{"lambda", 1, 6, 0, 1, 0, LispTokenKind::Lambda, 0};
        EXPECT_TRUE(lambdaToken.IsKeywordOperator());

        LispToken ifToken{"if", 1, 2, 0, 1, 0, LispTokenKind::If, 0};
        EXPECT_TRUE(ifToken.IsKeywordOperator());

        LispToken defunToken{"defun", 1, 5, 0, 1, 0, LispTokenKind::Defun, 0};
        EXPECT_TRUE(defunToken.IsKeywordOperator());

        LispToken defmacroToken{"defmacro", 1, 8, 0, 1, 0, LispTokenKind::Defmacro, 0};
        EXPECT_TRUE(defmacroToken.IsKeywordOperator());

        LispToken defvarToken{"defvar", 1, 6, 0, 1, 0, LispTokenKind::Defvar, 0};
        EXPECT_TRUE(defvarToken.IsKeywordOperator());

        LispToken logicalAndToken{"&&", 1, 2, 0, 1, 0, LispTokenKind::LogicalAnd, 0};
        EXPECT_TRUE(logicalAndToken.IsKeywordOperator());

        LispToken logicalOrToken{"||", 1, 2, 0, 1, 0, LispTokenKind::LogicalOr, 0};
        EXPECT_TRUE(logicalOrToken.IsKeywordOperator());

        LispToken notToken{"!", 1, 1, 0, 1, 0, LispTokenKind::Not, 0};
        EXPECT_TRUE(notToken.IsKeywordOperator());

        // Test non-keyword operators
        LispToken identifierToken{"identifier", 1, 10, 0, 1, 0, LispTokenKind::Identifier, 0};
        EXPECT_FALSE(identifierToken.IsKeywordOperator());

        LispToken plusToken{"+", 1, 1, 0, 1, 0, LispTokenKind::Plus, 0};
        EXPECT_FALSE(plusToken.IsKeywordOperator());

        LispToken nilToken{"nil", 1, 3, 0, 1, 0, LispTokenKind::Nil, 0};
        EXPECT_FALSE(nilToken.IsKeywordOperator());
    }

    TEST_F(LispTokenTest, IsFragmentOrCommentFunction) {
        constexpr LispToken commentToken{"// comment", 1, 10, 0, 1, 0, LispTokenKind::Comment, 0};
        EXPECT_TRUE(commentToken.IsFragmentOrComment());

        constexpr LispToken fragmentToken{"fragment", 1, 8, 0, 1, 0, LispTokenKind::Fragment, 0};
        EXPECT_TRUE(fragmentToken.IsFragmentOrComment());

        constexpr LispToken identifierToken{"identifier", 1, 10, 0, 1, 0, LispTokenKind::Identifier, 0};
        EXPECT_FALSE(identifierToken.IsFragmentOrComment());

        constexpr LispToken plusToken{"+", 1, 1, 0, 1, 0, LispTokenKind::Plus, 0};
        EXPECT_FALSE(plusToken.IsFragmentOrComment());
    }

    TEST_F(LispTokenTest, TokenKindEnumValues) {
        // Test ASCII character mappings
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::EndOfFile), '\0');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Not), '!');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Hash), '#');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Dollar), '$');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::LeftParenthesis), '(');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::RightParenthesis), ')');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Plus), '+');
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Minus), '-');

        // Test non-ASCII tokens start from 128
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::Identifier), 128);
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::LeftBitShift), 129);
        EXPECT_EQ(static_cast<uint8_t>(LispTokenKind::RightBitShift), 130);
    }

    TEST_F(LispTokenTest, GetByteLocationFunction) {
        const auto text = "hello world";
        const char* tokenPtr = text + 6; // Points to "world"

        const LispToken token{tokenPtr, 1, 5, 0, 7, 0, LispTokenKind::Identifier, 0};

        const std::uint32_t location = token.GetByteLocation(text);
        EXPECT_EQ(location, 6);
    }

    TEST_F(LispTokenTest, MemberAccess) {
        const auto text = "test";
        const LispToken token{text, 5, 4, 2, 10, 3, LispTokenKind::Identifier, 1};

        EXPECT_EQ(token.TextPtr, text);
        EXPECT_EQ(token.Line, 5);
        EXPECT_EQ(token.Length, 4);
        EXPECT_EQ(token.AuxiliaryIndex, 2);
        EXPECT_EQ(token.Column, 10);
        EXPECT_EQ(token.IndexInSpecialStream, 3);
        EXPECT_EQ(token.Kind, LispTokenKind::Identifier);
        EXPECT_EQ(token.AuxiliaryLength, 1);
    }

    TEST_F(LispTokenTest, PredefinedTokens) {
        // Test the predefined EndOfFile token
        const LispToken& eofToken = PredefinedTokens::EndOfFile;

        EXPECT_EQ(eofToken.TextPtr, nullptr);
        EXPECT_EQ(eofToken.Line, 0);
        EXPECT_EQ(eofToken.Length, 0);
        EXPECT_EQ(eofToken.Column, 0);
        EXPECT_EQ(eofToken.Kind, LispTokenKind::EndOfFile);
    }
}