#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LispParseTree.h" // This will be found via ../../include
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <fstream>

using namespace WideLips;
using namespace testing;

namespace WideLips::Tests {
    // ============================================================================
    // Test Fixture
    // ============================================================================
    class CommonLispParseTest : public Test {
    protected:
        std::filesystem::path tempFilePath;

        static LispParseResult ParseProgram(const std::string& program, const bool conservative = false) {
            auto paddedString = LispParseTree::MakeParserFriendlyString(program);
            return LispParseTree::Parse(std::move(paddedString),conservative);
        }
    };

    // ============================================================================
    // Common Lisp-Specific Syntax Tests
    // ============================================================================

    TEST_F(CommonLispParseTest, CommonLispKeywordDefun) {
        // This test verifies that FuncKeyword="defun" is working.
        const auto result = ParseProgram("(defun my-func (x) (+ x 1))");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* defun = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(defun, nullptr);

        EXPECT_EQ(defun->GetParseNodeText(), "defun");
        EXPECT_EQ(defun->Kind, LispParseNodeKind::Defun);
    }

    TEST_F(CommonLispParseTest, CommonLispBooleanLiterals) {
        // This test verifies TrueLiteral="t" and FalseLiteral="nil".
        // The lexer maps FalseLiteral to BooleanLiteral.
        const auto result = ParseProgram("(if t nil)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* iff = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(iff, nullptr);
        EXPECT_EQ(iff->Kind, LispParseNodeKind::If);

        auto* t = reinterpret_cast<LispAtom*>(iff->NextNode());
        ASSERT_NE(t, nullptr);
        EXPECT_EQ(t->GetParseNodeText(), "t");
        EXPECT_EQ(t->Kind, LispParseNodeKind::BooleanLiteral);

        auto* f = reinterpret_cast<LispAtom*>(t->NextNode());
        ASSERT_NE(f, nullptr);
        EXPECT_EQ(f->GetParseNodeText(), "nil");
        // This should be tokenized as FalseLiteral -> BooleanLiteral
        EXPECT_EQ(f->Kind, LispParseNodeKind::BooleanLiteral);
    }

    TEST_F(CommonLispParseTest, CommonLispBracketsAreInvalid) {
        // This test verifies that `EnableBrackets` is OFF for Common Lisp.
        // The '[' token should be `LispTokenKind::Invalid`.
        using namespace Diagnostic;
        const auto result = ParseProgram("([1 2])");
        ASSERT_FALSE(result.Success); // Parse should fail

        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_GE(diagnostics.Size(), 1u);

        // We expect an "UnrecognizedToken" error for '['
        bool foundError = false;
        for (size_t i = 0; i < diagnostics.Size(); ++i) {
            if (diagnostics[i].GetErrorCode() == DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken)) {
                foundError = true;
                break;
            }
        }
        EXPECT_TRUE(foundError) << "Did not find UnrecognizedToken diagnostic for '['";
    }

    TEST_F(CommonLispParseTest, CommonLispQuote) {
        // Expectation: The parser will treat it as a standard `LispParseNodeKind::Operator`.
        const auto result = ParseProgram("('foo)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* quote = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(quote, nullptr);
        EXPECT_EQ(quote->GetParseNodeText(), "'");
        EXPECT_EQ(quote->Kind, LispParseNodeKind::Operator);

        auto* symbol = reinterpret_cast<LispAtom*>(quote->NextNode());
        ASSERT_NE(symbol, nullptr);
        EXPECT_EQ(symbol->GetParseNodeText(), "foo");
    }

    TEST_F(CommonLispParseTest, CommonLispQuasiQuote) {
        // Expectation: `LispParser::ParseDialectSpecial` *explicitly* handles
        // `QuasiColumn` (`) and `Comma` (,) as `LispParseNodeKind::Operator`.
        const auto result = ParseProgram("(`(foo ,bar))");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* quasi = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(quasi, nullptr);
        EXPECT_EQ(quasi->GetParseNodeText(), "`");
        EXPECT_EQ(quasi->Kind, LispParseNodeKind::Operator);

        auto* list = reinterpret_cast<LispList*>(quasi->NextNode());
        ASSERT_NE(list, nullptr);
        EXPECT_EQ(list->Kind, LispParseNodeKind::SExpr);

        auto* foo = reinterpret_cast<LispAtom*>(list->GetSubExpressions());
        auto* comma = reinterpret_cast<LispAtom*>(foo->NextNode());
        auto* bar = reinterpret_cast<LispAtom*>(comma->NextNode());

        ASSERT_NE(foo, nullptr);
        ASSERT_NE(comma, nullptr);
        ASSERT_NE(bar, nullptr);

        EXPECT_EQ(comma->GetParseNodeText(), ",");
        EXPECT_EQ(comma->Kind, LispParseNodeKind::Operator);
    }

    TEST_F(CommonLispParseTest, CommonLispUnquoteSplicing) {
        // Expectation: `ParseDialectSpecial` handles `,` and `@` as *separate* operators.
        const auto result = ParseProgram("(,@foo)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* comma = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(comma, nullptr);
        EXPECT_EQ(comma->GetParseNodeText(), ",");
        EXPECT_EQ(comma->Kind, LispParseNodeKind::Operator);

        auto* at = reinterpret_cast<LispAtom*>(comma->NextNode());
        ASSERT_NE(at, nullptr);
        EXPECT_EQ(at->GetParseNodeText(), "@");
        EXPECT_EQ(at->Kind, LispParseNodeKind::Operator);

        auto* foo = reinterpret_cast<LispAtom*>(at->NextNode());
        ASSERT_NE(foo, nullptr);
        EXPECT_EQ(foo->GetParseNodeText(), "foo");
    }

    TEST_F(CommonLispParseTest, CommonLispCharacterLiteral) {
        // Expectation: The lexer sees `#` (EnableHash) and `\` (Operator) as separate tokens.
        const auto result = ParseProgram("(#\\a)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* hash = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(hash, nullptr);
        EXPECT_EQ(hash->GetParseNodeText(), "#");

        auto* slash = reinterpret_cast<LispAtom*>(hash->NextNode());
        ASSERT_NE(slash, nullptr);
        EXPECT_EQ(slash->GetParseNodeText(), "\\");
        EXPECT_EQ(slash->Kind, LispParseNodeKind::Operator);

        auto* symbol = reinterpret_cast<LispAtom*>(slash->NextNode());
        ASSERT_NE(symbol, nullptr);
        EXPECT_EQ(symbol->GetParseNodeText(), "a");
    }

    TEST_F(CommonLispParseTest, CommonLispVectorLiteral) {
        // Expectation: The parser sees the atom `#` followed by the S-expression `(1 2 3)`.
        const auto result = ParseProgram("(#(1 2 3))");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* hash = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(hash, nullptr);
        EXPECT_EQ(hash->GetParseNodeText(), "#");

        auto* list = reinterpret_cast<LispList*>(hash->NextNode());
        ASSERT_NE(list, nullptr);
        EXPECT_EQ(list->Kind, LispParseNodeKind::SExpr);

        auto* one = reinterpret_cast<LispAtom*>(list->GetSubExpressions());
        ASSERT_NE(one, nullptr);
        EXPECT_EQ(one->GetParseNodeText(), "1");
    }
}