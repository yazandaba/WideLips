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
    class ClojureLispParseTest : public Test {
    protected:
        std::filesystem::path tempFilePath;
        static LispParseResult ParseProgram(const std::string& program, const bool conservative = false) {
            auto paddedString = LispParseTree::MakeParserFriendlyString(program);
            return LispParseTree::Parse(std::move(paddedString),conservative);
        }
    };

    // ============================================================================
    // Clojure-Specific Syntax Tests
    // ============================================================================

    TEST_F(ClojureLispParseTest, ClojureKeywordDefn) {
        // This test verifies that FuncKeyword="defn" is working.
        // The lexer should identify "defn" as LispTokenKind::Defun.
        const auto result = ParseProgram("(defn my-func [x] (+ x 1))");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* defn = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(defn, nullptr);

        EXPECT_EQ(defn->GetParseNodeText(), "defn");
        EXPECT_EQ(defn->Kind, LispParseNodeKind::Defun);

        // We can also check the rest of the structure, which relies on `EnableBrackets`
        auto* symbol = reinterpret_cast<LispAtom*>(defn->NextNode());
        ASSERT_NE(symbol, nullptr);
        EXPECT_EQ(symbol->GetParseNodeText(), "my-func");
        EXPECT_EQ(symbol->Kind, LispParseNodeKind::Symbol);

        // Check for the vector `[x]`
        auto* vector = reinterpret_cast<LispAtom*>(symbol->NextNode());
        ASSERT_NE(vector, nullptr);
        EXPECT_EQ(vector->GetParseNodeText(), "[");

        auto* x = reinterpret_cast<LispAtom*>(vector->NextNode());
        ASSERT_NE(x, nullptr);
        EXPECT_EQ(x->GetParseNodeText(), "x");

        auto* vector_close = reinterpret_cast<LispAtom*>(x->NextNode());
        ASSERT_NE(vector_close, nullptr);
        EXPECT_EQ(vector_close->GetParseNodeText(), "]");
    }

    TEST_F(ClojureLispParseTest, ClojureVectorBrackets) {
        // Test: How does the parser handle vector brackets `[ ... ]`?
        // Expectation: The lexer (due to EnableBrackets) sees `[` and `]` as tokens.
        // The parser will treat them as generic atoms.
        const auto result = ParseProgram("([1 2])");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* node1 = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(node1->GetParseNodeText(), "[");

        auto* node2 = reinterpret_cast<LispAtom*>(node1->NextNode());
        EXPECT_EQ(node2->GetParseNodeText(), "1");

        auto* node3 = reinterpret_cast<LispAtom*>(node2->NextNode());
        EXPECT_EQ(node3->GetParseNodeText(), "2");

        auto* node4 = reinterpret_cast<LispAtom*>(node3->NextNode());
        EXPECT_EQ(node4->GetParseNodeText(), "]");
    }

    TEST_F(ClojureLispParseTest, ClojureKeyword) {
        // Test: How does the parser handle Clojure keywords like `:foo`?
        // Expectation: The lexer (due to EnableColumn) sees `:` as a token.
        const auto result = ParseProgram("(:foo)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* colon = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(colon, nullptr);
        EXPECT_EQ(colon->GetParseNodeText(), ":");

        auto* symbol = reinterpret_cast<LispAtom*>(colon->NextNode());
        ASSERT_NE(symbol, nullptr);
        EXPECT_EQ(symbol->GetParseNodeText(), "foo");
    }

    TEST_F(ClojureLispParseTest, ClojureMapBraces) {
        // Test: How does the parser handle map braces `{ ... }`?
        // Expectation: The lexer does NOT have `{` or `}`.
        // It will tokenize them as `LispTokenKind::Invalid`.
        using namespace Diagnostic;
        const auto result = ParseProgram("({:a 1})");
        ASSERT_FALSE(result.Success);

        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_GE(diagnostics.Size(), 1u);

        bool foundError = false;
        for (size_t i = 0; i < diagnostics.Size(); ++i) {
            if (diagnostics[i].GetErrorCode() == DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken)) {
                foundError = true;
                break;
            }
        }
        EXPECT_TRUE(foundError) << "Did not find UnrecognizedToken diagnostic for '{'";
    }

    TEST_F(ClojureLispParseTest, ClojureSet) {
        // Test: How does the parser handle sets `#{ ... }`?
        // Expectation: The lexer sees `#` (EnableHash) but not `{`.
        // This will parse `#` as an atom and then fail on `{` (Invalid token).
        using namespace Diagnostic;
        const auto result = ParseProgram("(#{1 2})");
        ASSERT_FALSE(result.Success);

        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_GE(diagnostics.Size(), 1u);

        EXPECT_EQ(diagnostics[0].GetErrorCode(), DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnrecognizedToken));
    }

    TEST_F(ClojureLispParseTest, ClojureAnonymousFunction) {
        // Test: How does the parser handle anonymous functions `#( ... )`?
        // Expectation: The lexer (due to EnableHash) sees `#` as a token.
        // The parser will treat it as a generic atom.
        const auto result = ParseProgram("(#(+ 1 %))");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* hash = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(hash, nullptr);
        EXPECT_EQ(hash->GetParseNodeText(), "#");

        auto* list = reinterpret_cast<LispList*>(hash->NextNode());
        ASSERT_NE(list, nullptr);
        EXPECT_EQ(list->Kind, LispParseNodeKind::SExpr);
    }

    TEST_F(ClojureLispParseTest, ClojureBooleanLiterals) {
        // This test verifies that TrueLiteral="true" and FalseLiteral="false" are working.
        const auto result = ParseProgram("(if true false)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* iff = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        ASSERT_NE(iff, nullptr);
        EXPECT_EQ(iff->Kind, LispParseNodeKind::If);

        auto* t = reinterpret_cast<LispAtom*>(iff->NextNode());
        ASSERT_NE(t, nullptr);
        EXPECT_EQ(t->GetParseNodeText(), "true");
        EXPECT_EQ(t->Kind, LispParseNodeKind::BooleanLiteral);

        auto* f = reinterpret_cast<LispAtom*>(t->NextNode());
        ASSERT_NE(f, nullptr);
        EXPECT_EQ(f->GetParseNodeText(), "false");
        EXPECT_EQ(f->Kind, LispParseNodeKind::BooleanLiteral);
    }
}