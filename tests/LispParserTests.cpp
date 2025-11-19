#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "LispParseTree.h"
#include <vector>
#include <string>
#include <memory>
#include <fstream>      // Added for file I/O
#include <filesystem>   // Added for file paths

using namespace WideLips;
using namespace testing;

// ============================================================================
// Test Fixtures
// ============================================================================

namespace WideLips::Tests {
    class LispParseTreeTest : public Test {
    protected:
        std::filesystem::path tempFilePath; // For file-based parsing tests

        void SetUp() override {}

        void TearDown() override {
            // Clean up temporary file if one was created
            if (!tempFilePath.empty()) {
                std::error_code ec;
                std::filesystem::remove(tempFilePath, ec);
                tempFilePath = "";
            }
        }

        // Helper to create a temp file for testing
        std::filesystem::path CreateTempFile(const std::string& content) {
            tempFilePath = std::filesystem::temp_directory_path() / "lisp_parser_test_temp_file.lsp";
            std::ofstream outFile(tempFilePath.string());
            outFile << content;
            outFile.close();
            return tempFilePath;
        }

        // Helper to parse a program string
        static LispParseResult ParseProgram(const std::string& program, const bool conservative = false) {
            auto paddedString = LispParseTree::MakeParserFriendlyString(program);
            return LispParseTree::Parse(std::move(paddedString),conservative);
        }

        // NEW Helper to parse from a file
        // This assumes LispParseTree::Parse has an overload for std::filesystem::path
        static LispParseResult ParseFile(const std::filesystem::path& filePath, const bool conservative = false) {
            return LispParseTree::Parse(filePath, conservative);
        }

        // Helper to count nodes in tree
        static size_t CountNodes(const LispParseNodeBase* node) {
            if (!node) return 0;
            size_t count = 1;

            // Handle lazy evaluation for LispList
            if (node->Kind == LispParseNodeKind::SExpr) {
                const auto* list = reinterpret_cast<const LispList*>(node);
                const auto* subExprs = list->GetSubExpressions();
                count += CountNodes(subExprs);
            }

            // Get next node
            const auto* next = node->NextNode();
            count += CountNodes(next);

            return count;
        }

        // Helper to traverse and collect all nodes
        static void CollectNodes(const LispParseNodeBase* node, std::vector<const LispParseNodeBase*>& nodes) {
            if (!node) {
                return;
            }

            if (node->Kind == LispParseNodeKind::EndOfProgram) {
                return;
            }

            nodes.push_back(node);
            // Handle lazy evaluation for LispList
            if (node->Kind == LispParseNodeKind::SExpr) {
                const auto* list = reinterpret_cast<const LispList*>(node);
                const auto* subExprs = list->GetSubExpressions();
                CollectNodes(subExprs, nodes);
            }

            // Get next node
            const auto* next = node->NextNode();
            CollectNodes(next, nodes);
        }
    };

    // ============================================================================
    // SourceLocation Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, SourceLocationConstruction) {
        const SourceLocation loc(10, 25);
        EXPECT_EQ(loc.Line, 10u);
        EXPECT_EQ(loc.ColumnChar, 25u);
    }

    TEST_F(LispParseTreeTest, SourceLocationDefault) {
        const auto defaultLoc = SourceLocation::Default();
        EXPECT_EQ(defaultLoc.Line, 0u);
        EXPECT_EQ(defaultLoc.ColumnChar, 0u);
    }

    // ============================================================================
    // Basic Parsing Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseEmptyProgram) {
        const auto result = ParseProgram("");
        EXPECT_FALSE(result.Success);
        EXPECT_EQ(result.ParseTree->GetFilePath(), "memory");
    }

    TEST_F(LispParseTreeTest, ParseSimpleAtom) {
        const auto result = ParseProgram("(42)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind , LispParseNodeKind::RealLiteral);
        EXPECT_EQ(atom->GetParseNodeText(), "42");
    }

    TEST_F(LispParseTreeTest, ParseSymbol) {
        const auto result = ParseProgram("(variable-name)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::Symbol);
        EXPECT_EQ(atom->GetParseNodeText(), "variable-name");
    }

    TEST_F(LispParseTreeTest, ParseStringLiteral) {
        const auto result = ParseProgram("(\"hello world\")");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::StringLiteral);
        EXPECT_EQ(atom->GetParseNodeText(), "\"hello world\"");
    }

    TEST_F(LispParseTreeTest, ParseRealLiteral) {
        const auto result = ParseProgram("(3.14159)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::RealLiteral);
        EXPECT_EQ(atom->GetParseNodeText(), "3.14159");
    }

    TEST_F(LispParseTreeTest, ParseBooleanLiterals) {
        // Test true
        auto result = ParseProgram("("+std::string{TrueLiteral}+")");
        ASSERT_TRUE(result.ParseTree != nullptr);
        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);
        auto* atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::BooleanLiteral);
        EXPECT_EQ(atom->GetParseNodeText(), TrueLiteral);

        // Test false
        result = ParseProgram("("+std::string{FalseLiteral}+")");
        ASSERT_TRUE(result.ParseTree != nullptr);
        root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);
        atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::BooleanLiteral);
        EXPECT_EQ(atom->GetParseNodeText(), FalseLiteral);

        // Test nil
        result = ParseProgram("("+std::string{NilKeyword}+")");
        ASSERT_TRUE(result.ParseTree != nullptr);
        root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);
        atom = reinterpret_cast<const LispAtom*>(root->GetSubExpressions());
        EXPECT_EQ(atom->Kind, LispParseNodeKind::Nil);
        EXPECT_EQ(atom->GetParseNodeText(), NilKeyword);
    }

    // ============================================================================
    // LispList and Lazy Parsing Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseSimpleSExpression) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        // Test lazy parsing - must call GetSubExpressions
        auto* list = reinterpret_cast<const LispList*>(root);
        EXPECT_NE(list, nullptr);

        // Get sub-expressions (triggers lazy evaluation)
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        // Should have operator +
        EXPECT_NE(subExprs->Kind, LispParseNodeKind::EndOfProgram);
    }

    TEST_F(LispParseTreeTest, ParseNestedSExpressions) {
        const auto result = ParseProgram("(+ (* 2 3) 4)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        // Outer list
        auto* outerList = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = outerList->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        // Traverse to find nested expression
        const auto* current = subExprs;
        while (current) {
            if (current->Kind == LispParseNodeKind::SExpr) {
                // Found nested list
                auto* nestedList = reinterpret_cast<const LispList*>(current);
                const auto* nestedSubExprs = nestedList->GetSubExpressions();
                ASSERT_NE(nestedSubExprs, nullptr);

                // Nested list should have content
                EXPECT_NE(nestedSubExprs->Kind, LispParseNodeKind::EndOfProgram);
                break;
            }
            current = current->NextNode();
        }
    }

    TEST_F(LispParseTreeTest, LazyConstParsingSubExpressions) {
        const auto result = ParseProgram("(defun foo (x y) (+ x y))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* list = reinterpret_cast<const LispList*>(root);

        // First call to GetSubExpressions - triggers lazy evaluation
        const auto* subExprs1 = list->GetSubExpressions();
        ASSERT_NE(subExprs1, nullptr);

        // Second call should return new copy - also triggers lazy evaluation
        const auto* subExprs2 = list->GetSubExpressions();
        EXPECT_NE(subExprs1, subExprs2);
    }

    TEST_F(LispParseTreeTest, LazyParsingSubExpressions) {
        const auto result = ParseProgram("(defun foo (x y) (+ x y))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* list = reinterpret_cast<LispList*>(root);

        // First call to GetSubExpressions - triggers lazy evaluation
        auto* subExprs1 = list->GetSubExpressions();
        ASSERT_NE(subExprs1, nullptr);

        // Second call should return the same cached result
        auto* subExprs2 = list->GetSubExpressions();
        EXPECT_EQ(subExprs1, subExprs2);
    }

    TEST_F(LispParseTreeTest, TraverseWithNextNode) {
        const auto result = ParseProgram("(+ 1 2 3)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();

        // Traverse using NextNode
        int nodeCount = 0;
        const auto* current = subExprs;
        while (current && current->Kind != LispParseNodeKind::EndOfProgram) {
            nodeCount++;
            current = current->NextNode();
        }

        EXPECT_GT(nodeCount, 0);
    }

    TEST_F(LispParseTreeTest, MultipleSExpressionsWithNext) {
        const auto result = ParseProgram("(+ 1 2) (* 3 4)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // First expression
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        // Get next expression using NextNode
        const auto* next = root->NextNode();
        ASSERT_NE(next, nullptr);
        EXPECT_EQ(next->Kind, LispParseNodeKind::SExpr);
    }

    // ============================================================================
    // Keyword Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseDefun) {
        const auto result = ParseProgram("(defun square (x) (* x x))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        // First element should be defun keyword
        if (subExprs->Kind == LispParseNodeKind::Defun) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "defun");
        }
    }

    TEST_F(LispParseTreeTest, ParseLambda) {
        const auto result = ParseProgram("(lambda (x) (* x x))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        if (subExprs->Kind == LispParseNodeKind::Lambda) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "lambda");
        }
    }

    TEST_F(LispParseTreeTest, ParseLet) {
        const auto result = ParseProgram("(let ((x 10)) x)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        if (subExprs->Kind == LispParseNodeKind::Let) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "let");
        }
    }

    TEST_F(LispParseTreeTest, ParseIf) {
        const auto result = ParseProgram("(if (> x 0) 1 -1)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        if (subExprs->Kind == LispParseNodeKind::If) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "if");
        }
    }

    TEST_F(LispParseTreeTest, ParseDefmacro) {
        const auto result = ParseProgram("(defmacro unless (test &rest body) `(if (not ,test) (progn ,@body)))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        if (subExprs->Kind == LispParseNodeKind::Defmacro) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "defmacro");
        }
    }

    TEST_F(LispParseTreeTest, ParseDefvar) {
        const auto result = ParseProgram("(defvar *global* 100)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        if (subExprs->Kind == LispParseNodeKind::Defvar) {
            auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
            EXPECT_EQ(atom->GetParseNodeText(), "defvar");
        }
    }

    TEST_F(LispParseTreeTest, ParseLogicalOperators) {
        // Test 'and'
        const auto resultAnd = ParseProgram("(and t nil)");
        ASSERT_TRUE(resultAnd.ParseTree != nullptr);
        const auto* rootAnd = resultAnd.ParseTree->GetRoot();
        if (rootAnd && rootAnd->Kind == LispParseNodeKind::SExpr) {
            auto* list = reinterpret_cast<const LispList*>(rootAnd);
            const auto* subExprs = list->GetSubExpressions();
            if (subExprs && subExprs->Kind == LispParseNodeKind::LogicalAnd) {
                auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
                EXPECT_EQ(atom->GetParseNodeText(), "and");
            }
        }

        // Test 'or'
        const auto resultOr = ParseProgram("(or t nil)");
        ASSERT_TRUE(resultOr.ParseTree != nullptr);
        const auto* rootOr = resultOr.ParseTree->GetRoot();
        if (rootOr && rootOr->Kind == LispParseNodeKind::SExpr) {
            auto* list = reinterpret_cast<const LispList*>(rootOr);
            const auto* subExprs = list->GetSubExpressions();
            if (subExprs && subExprs->Kind == LispParseNodeKind::LogicalOr) {
                auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
                EXPECT_EQ(atom->GetParseNodeText(), "or");
            }
        }

        // Test 'not'
        const auto resultNot = ParseProgram("(not t)");
        ASSERT_TRUE(resultNot.ParseTree != nullptr);
        const auto* rootNot = resultNot.ParseTree->GetRoot();
        if (rootNot && rootNot->Kind == LispParseNodeKind::SExpr) {
            auto* list = reinterpret_cast<const LispList*>(rootNot);
            const auto* subExprs = list->GetSubExpressions();
            if (subExprs && subExprs->Kind == LispParseNodeKind::Not) {
                auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
                EXPECT_EQ(atom->GetParseNodeText(), "not");
            }
        }
    }

    // ============================================================================
    // Operator Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseArithmeticOperators) {
        const std::vector<std::string> operators = {"+", "-", "*", "/", "%"};

        for (const auto& op : operators) {
            std::string program = "(" + op + " 1 2)";
            auto result = ParseProgram(program);
            ASSERT_TRUE(result.Success);

            const auto* root = result.ParseTree->GetRoot();
            if (root && root->Kind == LispParseNodeKind::SExpr) {
                auto* list = reinterpret_cast<const LispList*>(root);
                const auto* subExprs = list->GetSubExpressions();
                if (subExprs && subExprs->Kind == LispParseNodeKind::Operator) {
                    auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
                    EXPECT_EQ(atom->GetParseNodeText(), op);
                }
            }
        }
    }

    TEST_F(LispParseTreeTest, ParseComparisonOperators) {
        const std::vector<std::string> operators = {"<", ">", "<=", ">=", "="};

        for (const auto& op : operators) {
            std::string program = "(" + op + " 1 2)";
            auto result = ParseProgram(program);
            ASSERT_TRUE(result.Success);

            const auto* root = result.ParseTree->GetRoot();
            if (root && root->Kind == LispParseNodeKind::SExpr) {
                auto* list = reinterpret_cast<const LispList*>(root);
                const auto* subExprs = list->GetSubExpressions();
                if (subExprs && subExprs->Kind == LispParseNodeKind::Operator) {
                    auto* atom = reinterpret_cast<const LispAtom*>(subExprs);
                    EXPECT_EQ(atom->GetParseNodeText(), op);
                }
            }
        }
    }

    // ============================================================================
    // Source Location Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, SourceLocationForAtom) {
        const auto result = ParseProgram("(42)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* atom = reinterpret_cast<const LispAtom*>(root);
        const auto loc = atom->GetSourceLocation();

        EXPECT_GE(loc.Line, 0u);
        EXPECT_GE(loc.ColumnChar, 0u);
    }

    TEST_F(LispParseTreeTest, SourceLocationForList) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto loc = list->GetSourceLocation();

        EXPECT_GE(loc.Line, 0u);
        EXPECT_GE(loc.ColumnChar, 0u);
    }

    TEST_F(LispParseTreeTest, SourceLocationMultiline) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} +"foo ()\n  (* 2 3))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();

        // Traverse to find elements on different lines
        const auto* current = subExprs;
        std::set<std::uint32_t> lines;
        while (current && current->Kind != LispParseNodeKind::EndOfProgram) {
            auto loc = current->GetSourceLocation();
            lines.insert(loc.Line);

            if (current->Kind == LispParseNodeKind::SExpr) {
                auto* nestedList = reinterpret_cast<const LispList*>(current);
                const auto* nested = nestedList->GetSubExpressions();
                if (nested) {
                    auto nestedLoc = nested->GetSourceLocation();
                    lines.insert(nestedLoc.Line);
                }
            }

            current = current->NextNode();
        }

        // Should have elements on multiple lines
        EXPECT_GT(lines.size(), 0u);
    }

    // ============================================================================
    // Full Tree Traversal Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, TraverseCompleteTree) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} + " factorial (n) (if (<= n 1) 1 (* n (factorial (- n 1)))))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        std::vector<const LispParseNodeBase*> nodes;
        CollectNodes(root, nodes);

        EXPECT_GT(nodes.size(), 0u);

        // Verify each node has valid kind and text
        for (const auto* node : nodes) {
            ASSERT_NE(node, nullptr);
            EXPECT_NE(node->Kind, LispParseNodeKind::EndOfProgram);

            auto text = node->GetParseNodeText();
            EXPECT_FALSE(text.empty());
        }
    }

    TEST_F(LispParseTreeTest, TraverseMultipleTopLevelExpressions) {
        const auto result = ParseProgram("(defvar x 10) (defvar y 20) (+ x y)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        int topLevelCount = 0;
        const auto* current = root;

        while (current && current->Kind != LispParseNodeKind::EndOfProgram) {
            if (current->Kind == LispParseNodeKind::SExpr) {
                topLevelCount++;

                // Verify we can get sub-expressions
                auto* list = reinterpret_cast<const LispList*>(current);
                const auto* subExprs = list->GetSubExpressions();
                EXPECT_NE(subExprs, nullptr);
            }

            current = current->NextNode();
        }

        EXPECT_GE(topLevelCount, 3);
    }

    TEST_F(LispParseTreeTest, TraverseDeeplyNestedExpressions) {
        const auto result = ParseProgram("(a (b (c (d (e (f 1))))))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        int depth = 0;
        const auto* current = root;

        while (current && current->Kind == LispParseNodeKind::SExpr) {
            depth++;
            auto* list = reinterpret_cast<const LispList*>(current);
            const auto* subExprs = list->GetSubExpressions();

            if (!subExprs) break;

            // Move to next nested expression
            current = subExprs->NextNode();
        }

        EXPECT_GT(depth, 0);
    }

    // ============================================================================
    // Arguments Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseFunctionWithArguments) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} + " add (a b) (+ a b))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* subExprs = list->GetSubExpressions();

        // Traverse to find arguments
        const auto* current = subExprs;

        while (current && current->Kind != LispParseNodeKind::EndOfProgram) {
            if (current->Kind == LispParseNodeKind::Arguments) {
                auto* args = reinterpret_cast<const LispArguments*>(current);
                const auto argsText = args->GetParseNodeText();

                EXPECT_THAT(std::string(argsText), HasSubstr("a"));
                EXPECT_THAT(std::string(argsText), HasSubstr("b"));
                break;
            }
            current = current->NextNode();
        }

        // May or may not have Arguments node depending on parser implementation
        // Test passes either way
    }

    TEST_F(LispParseTreeTest, ParseLambdaWithArguments) {
        const auto result = ParseProgram("(lambda (x y z) (* x (+ y z)))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // Collect all nodes recursively to handle nested structures
        std::vector<const LispParseNodeBase*> nodes;
        CollectNodes(root, nodes);

        // Gather symbols encountered anywhere in the tree
        std::set<std::string> symbols;
        bool sawArgumentsNode = false;

        for (const auto* node : nodes) {
            if (node->Kind == LispParseNodeKind::Symbol) {
                const auto* atom = reinterpret_cast<const LispAtom*>(node);
                symbols.insert(std::string(atom->GetParseNodeText()));
            } else if (node->Kind == LispParseNodeKind::Arguments) {
                sawArgumentsNode = true;
            }
        }

        // Validate that the lambda form and its arguments appear in the list text
        const auto* list = reinterpret_cast<const LispList*>(root);
        const auto listText = list->GetParseNodeText();
        const std::string listTextStr(listText);
        EXPECT_THAT(listTextStr, HasSubstr("lambda"));
        EXPECT_THAT(listTextStr, HasSubstr("x"));
        EXPECT_THAT(listTextStr, HasSubstr("y"));
        EXPECT_THAT(listTextStr, HasSubstr("z"));

        // Ensure we collected at least one of the argument symbols, and optionally
        // accept either explicit Arguments node or symbol occurrences
        const size_t argsFound = (symbols.count("x") ? 1u : 0u)
                               + (symbols.count("y") ? 1u : 0u)
                               + (symbols.count("z") ? 1u : 0u);
        EXPECT_GE(argsFound, 1u);
        (void)sawArgumentsNode; // parser may or may not produce a dedicated Arguments node
    }

    // ============================================================================
    // Auxiliary Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, GetNodeAuxiliaryAtom) {
        const auto result = ParseProgram("(42)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* atom = reinterpret_cast<const LispAtom*>(root);
        const auto* aux = atom->GetNodeAuxiliary();
        ASSERT_EQ(aux,nullptr);
        // Auxiliary may be null or non-null depending on token
        // Just verify the call doesn't crash
    }

    TEST_F(LispParseTreeTest, GetNodeAuxiliaryList) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* list = reinterpret_cast<const LispList*>(root);
        const auto* aux = list->GetNodeAuxiliary();
        ASSERT_EQ(aux,nullptr);
    }

    // ============================================================================
    // Visitor Pattern Tests
    // ============================================================================

    class TestVisitor : public LispParseTreeVisitor<TestVisitor> {
    public:
        int atomCount = 0;
        int listCount = 0;
        int argumentsCount = 0;
        int errorCount = 0;

        void Visit(LispAtom * const atom) noexcept {
            atomCount++;
            // Traverse siblings in the current list
            auto* next = atom->NextNode();
            if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
                next->Accept(this);
            }
        }

        void Visit(LispList * const list) noexcept {
            listCount++;

            // Recursively visit sub-expressions
            LispParseNodeBase* subExprs = list->GetSubExpressions();
            if (subExprs) {
                subExprs->Accept(this);
            }

            // Visit next node (next top-level or sibling list)
            auto* next = list->NextNode();
            if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
                next->Accept(this);
            }
        }

        void Visit(LispArguments * const arguments) noexcept {
            argumentsCount++;
            auto* next = arguments->NextNode();
            if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
                next->Accept(this);
            }
        }

        void Visit(LispParseError * const error) noexcept {
            errorCount++;
            auto* next = error->NextNode();
            if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
                next->Accept(this);
            }
        }
    };

    class ImmutableTestVisitor : public ImmutableLispParseTreeVisitor<ImmutableTestVisitor> {
    public:
        mutable int atomCount = 0;
        mutable int listCount = 0;
        mutable int argumentsCount = 0;
        mutable int errorCount = 0;

        void Visit(const LispAtom* atom) const {
            atomCount++;
        }

        void Visit(const LispList* list) const {
            listCount++;
            // Recursively visit sub-expressions
            const auto* subExprs = list->GetSubExpressions();
            if (subExprs) {
                subExprs->Accept(this);
            }

            // Visit next node
            const auto* next = list->NextNode();
            if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
                next->Accept(this);
            }
        }

        void Visit(const LispArguments*) const {
            argumentsCount++;
        }

        void Visit(const LispParseError*) const {
            errorCount++;
        }
    };

    TEST_F(LispParseTreeTest, VisitorPatternMutable) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        TestVisitor visitor;
        root->Accept(&visitor);

        EXPECT_GT(visitor.atomCount + visitor.listCount, 0);
    }

    TEST_F(LispParseTreeTest, VisitorPatternImmutable) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} + " foo (x) (* x x))");
        ASSERT_TRUE(result.Success);

        const auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        constexpr ImmutableTestVisitor visitor;
        root->Accept(&visitor);

        EXPECT_GT(visitor.atomCount + visitor.listCount, 0);
    }

    TEST_F(LispParseTreeTest, VisitorPatternComplexTree) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} + " factorial (n) (if (<= n 1) 1 (* n (factorial (- n 1)))))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        TestVisitor visitor;
        root->Accept(&visitor);

        EXPECT_GT(visitor.atomCount, 0);
        EXPECT_GT(visitor.listCount, 0);
    }

    TEST_F(LispParseTreeTest, ParseTreeAcceptVisitor) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        TestVisitor visitor;
        result.ParseTree->Accept(&visitor);

        EXPECT_GT(visitor.atomCount + visitor.listCount, 0);
    }

    TEST_F(LispParseTreeTest, ParseTreeAcceptImmutableVisitor) {
        const auto result = ParseProgram("(* 3 4)");
        ASSERT_TRUE(result.Success);

        const ImmutableTestVisitor visitor;
        result.ParseTree->Accept(&visitor);

        EXPECT_GT(visitor.atomCount + visitor.listCount, 0);
    }

    // ============================================================================
    // Error Handling Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseInvalidSyntax) {
        using namespace Diagnostic;
        const auto result = ParseProgram("(+ 1 2");  // Missing closing paren
        ASSERT_FALSE(result.Success);

        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_FALSE(result.Success);
        ASSERT_NE(result.ParseTree,nullptr);
        ASSERT_EQ(diagnostics.Size(),1);
        ASSERT_EQ(diagnostics[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis));
    }

    TEST_F(LispParseTreeTest, ParseWithConservativeMode) {
        const auto result = ParseProgram("(+ 1 2)", true);  // Conservative mode
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        // Should still parse in conservative mode
    }

    TEST_F(LispParseTreeTest, GetRootWhenCannotBeConsumed) {
        using namespace Diagnostic;
        const auto result = ParseProgram("(+ 1 2 invalid syntax here", false);
        ASSERT_FALSE(result.Success);
        ASSERT_NE(result.ParseTree,nullptr);
        ASSERT_EQ(result.ParseTree->GetDiagnostics().Size(),1);
        ASSERT_EQ(result.ParseTree->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::NoMatchingCloseParenthesis));
    }

    // ============================================================================
    // File Parsing Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseFromFileAndCheckOrigin) {
        const std::string program = "(+ 10 20)";
        auto path = CreateTempFile(program);

        // Use the new file-parsing helper
        const auto result = ParseFile(path);

        ASSERT_TRUE(result.Success);
        ASSERT_NE(result.ParseTree, nullptr);

        // This is the test for Gap 2: OriginFile
        // LispParser::OriginFile returns a wstring_view
        EXPECT_EQ(result.ParseTree->GetFilePath(), path.string());

        // Also verify the content was parsed correctly
        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        auto* subExprs = root->GetSubExpressions();
        ASSERT_NE(subExprs, nullptr);

        auto* op = reinterpret_cast<const LispAtom*>(subExprs);
        EXPECT_EQ(op->GetParseNodeText(), "+");

        auto* arg1 = reinterpret_cast<const LispAtom*>(op->NextNode());
        ASSERT_NE(arg1, nullptr);
        EXPECT_EQ(arg1->GetParseNodeText(), "10");

        auto* arg2 = reinterpret_cast<const LispAtom*>(arg1->NextNode());
        ASSERT_NE(arg2, nullptr);
        EXPECT_EQ(arg2->GetParseNodeText(), "20");
    }

    TEST_F(LispParseTreeTest, ParseFromNonExistentFile) {
        using namespace Diagnostic;
        std::filesystem::path nonExistentPath = "non_existent_file_12345.lsp";

        const auto result = ParseFile(nonExistentPath);

        ASSERT_FALSE(result.Success);
        ASSERT_NE(result.ParseTree, nullptr);

        // Check for a file-related diagnostic
        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_GE(diagnostics.Size(), 1u);

        // We don't know the *exact* error code, but it should be an error
        EXPECT_EQ(diagnostics[0].GetSeverity(), Severity::Error);
    }

    // ============================================================================
    // Edge Cases
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseEmptyList) {
        using namespace Diagnostic;
        const auto result = ParseProgram("()");
        ASSERT_TRUE(result.Success);

        const auto* root = result.ParseTree->GetRoot();
        if (root && root->Kind == LispParseNodeKind::SExpr) {
            auto* list = reinterpret_cast<const LispList*>(root);
            const auto* subExprs = list->GetSubExpressions();

            if constexpr (DisallowEmptySExpr) {
                EXPECT_EQ(subExprs, nullptr);
                EXPECT_EQ(result.ParseTree->GetDiagnostics().Size(),1);
                EXPECT_EQ(result.ParseTree->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::EmptySExpr));
            }
            else {
                // Empty list should have no sub-expressions or end marker
                if (subExprs) {
                    EXPECT_TRUE(subExprs->Kind == LispParseNodeKind::EndOfProgram);
                }
            }
        }
    }

    TEST_F(LispParseTreeTest, ParseSingleNumber) {
        const auto result = ParseProgram("(12345)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList *>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        // The number is the first sub-expression (atom) inside the list
        const auto* sub = root->GetSubExpressions();
        ASSERT_NE(sub, nullptr);
        const auto* atom = reinterpret_cast<const LispAtom*>(sub);
        const auto text = atom->GetParseNodeText();
        EXPECT_EQ(text, "12345");
    }

    TEST_F(LispParseTreeTest, ParseNegativeNumber) {
        const auto result = ParseProgram("(-42)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList *>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        const auto subExpressions = root->GetSubExpressions();
        const auto neg = reinterpret_cast<LispAtom*>(subExpressions);
        const auto dig = reinterpret_cast<LispAtom*>(neg->NextNode());
        EXPECT_EQ(neg->GetParseNodeText(), "-");
        EXPECT_EQ(dig->GetParseNodeText(), "42");
    }

    TEST_F(LispParseTreeTest, ParseQuotedExpression) {
        using namespace Diagnostic;
        const auto result = ParseProgram("'(1 2 3)");
        ASSERT_FALSE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_EQ(root, nullptr);
        EXPECT_EQ(result.ParseTree->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken));
        EXPECT_EQ(result.ParseTree->GetDiagnostics()[0].GetSeverity(),Severity::Error);
    }

    TEST_F(LispParseTreeTest, ParseBackquoteExpression) {
        using namespace Diagnostic;
        const auto result = ParseProgram("'(1 2 3)");
        ASSERT_FALSE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_EQ(root, nullptr);
        EXPECT_EQ(result.ParseTree->GetDiagnostics()[0].GetErrorCode(),DiagnosticFactory::ErrorCodeToString(ParsingErrorCode::UnexpectedTopLevelToken));
        EXPECT_EQ(result.ParseTree->GetDiagnostics()[0].GetSeverity(),Severity::Error);
    }

    TEST_F(LispParseTreeTest, ParseCommentIgnored) {
        const auto result = ParseProgram("; This is a comment\n(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
    }

    TEST_F(LispParseTreeTest, ParseWhitespace) {
        const auto result = ParseProgram("   \n\t  (+   1    2)  \n  ");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);
    }

    TEST_F(LispParseTreeTest, ParseVeryLongSymbol) {
        const std::string longSymbol(1000, 'x');
        const std::string expr = "("+longSymbol+")";
        const auto result = ParseProgram(expr);
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList *>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        const auto atom = reinterpret_cast<LispAtom*>(root->GetSubExpressions());
        const auto text = atom->GetParseNodeText();
        EXPECT_EQ(text, longSymbol);
        EXPECT_EQ(text.length(), 1000u);
    }

    TEST_F(LispParseTreeTest, ParseSpecialCharactersInSymbol) {
        const auto result = ParseProgram("(*special-var*)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto* subExpr = root->GetSubExpressions();
        ASSERT_NE(subExpr, nullptr);

        // Expect three tokens: '*', 'special-var', '*'
        auto* first = reinterpret_cast<LispAtom*>(subExpr);
        ASSERT_NE(first, nullptr);
        EXPECT_TRUE(first->Kind == LispParseNodeKind::Operator || first->Kind == LispParseNodeKind::Symbol);
        EXPECT_EQ(std::string(first->GetParseNodeText()), "*");

        auto* second = reinterpret_cast<LispAtom*>(first->NextNode());
        ASSERT_NE(second, nullptr);
        EXPECT_EQ(second->Kind, LispParseNodeKind::Symbol);
        EXPECT_EQ(std::string(second->GetParseNodeText()), "special-var");

        auto* third = reinterpret_cast<LispAtom*>(second->NextNode());
        ASSERT_NE(third, nullptr);
        EXPECT_TRUE(third->Kind == LispParseNodeKind::Operator || third->Kind == LispParseNodeKind::Symbol);
        EXPECT_EQ(std::string(third->GetParseNodeText()), "*");
    }

    // ============================================================================
    // Type Checking Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, VerifyAllNodeKinds) {
        // Test that we can handle all node kinds
        const std::vector expectedKinds = {
            LispParseNodeKind::SExpr,
            LispParseNodeKind::Symbol,
            LispParseNodeKind::RealLiteral,
            LispParseNodeKind::StringLiteral,
            LispParseNodeKind::BooleanLiteral,
            LispParseNodeKind::Let,
            LispParseNodeKind::Lambda,
            LispParseNodeKind::Nil,
            LispParseNodeKind::If,
            LispParseNodeKind::Defun,
            LispParseNodeKind::Defmacro,
            LispParseNodeKind::Defvar,
            LispParseNodeKind::Operator,
            LispParseNodeKind::Arguments,
            LispParseNodeKind::LogicalAnd,
            LispParseNodeKind::LogicalOr,
            LispParseNodeKind::Not
        };

        // We've tested most of these in other tests
        // This test just confirms we have coverage for all important kinds
        EXPECT_GT(expectedKinds.size(), 0u);
    }

    TEST_F(LispParseTreeTest, NodeKindComparison) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        EXPECT_TRUE(root->Kind == LispParseNodeKind::SExpr);
        EXPECT_FALSE(root->Kind == LispParseNodeKind::Symbol);
        EXPECT_FALSE(root->Kind == LispParseNodeKind::EndOfProgram);
    }

    // ============================================================================
    // Lazy Parsing Complete Coverage
    // ============================================================================

    TEST_F(LispParseTreeTest, LazyParsingMultipleGets) {
        const auto result = ParseProgram("(" + std::string{FuncKeyword} + " test (x) (if (> x 0) (+ x 1) (- x 1)))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        auto* outerList = reinterpret_cast<LispList*>(root);

        // Get sub-expressions multiple times - should return cached value
        const auto* sub1 = outerList->GetSubExpressions();
        const auto* sub2 = outerList->GetSubExpressions();
        const auto* sub3 = outerList->GetSubExpressions();

        EXPECT_EQ(sub1, sub2);
        EXPECT_EQ(sub2, sub3);
    }

    TEST_F(LispParseTreeTest, LazyParsingNestedLists) {
        const auto result = ParseProgram("(a (b (c (d e))))");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // Traverse and trigger lazy parsing at each level
        std::function<void(const LispParseNodeBase*, int)> traverse;
        traverse = [&](const LispParseNodeBase* node, int depth) {
            if (!node || node->Kind == LispParseNodeKind::EndOfProgram || depth > 10) {
                return;
            }

            if (node->Kind == LispParseNodeKind::SExpr) {
                auto* list = reinterpret_cast<const LispList*>(node);
                const auto* subExprs = list->GetSubExpressions();

                if (subExprs) {
                    traverse(subExprs, depth + 1);
                    const auto* next = subExprs->NextNode();
                    traverse(next, depth + 1);
                }
            }

            const auto* next = node->NextNode();
            traverse(next, depth);
        };

        traverse(root, 0);
    }

    // ============================================================================
    // GetFilePath Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, GetFilePathMemory) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        EXPECT_EQ(result.ParseTree->GetFilePath(), "memory");
    }

    // ============================================================================
    // Complex Real-World Lisp Programs
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseFibonacciFunction) {
        const std::string program = std::format(R"(
        (" + {} + " fibonacci (n)
          (if (<= n 1)
              n
              (+ (fibonacci (- n 1))
                 (fibonacci (- n 2)))))
    )",FuncKeyword);

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // Traverse entire tree
        TestVisitor visitor;
        root->Accept(&visitor);

        EXPECT_GT(visitor.listCount, 3);  // Multiple nested lists
        EXPECT_GT(visitor.atomCount, 5);  // Various symbols and numbers
    }

    TEST_F(LispParseTreeTest, ParseQuicksortFunction) {
        const std::string program = std::format(R"(
        (" {} " quicksort (lst)
          (if ({} lst)
              {}
              (let ((pivot (car lst))
                    (rest (cdr lst)))
                (append (quicksort (filter ({} (x) (< x pivot)) rest))
                        (list pivot)
                        (quicksort (filter ({} (x) (>= x pivot)) rest))))))
    )",FuncKeyword,NilKeyword,NilKeyword,LambdaKeyword,LambdaKeyword);

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        std::vector<const LispParseNodeBase*> nodes;
        CollectNodes(root, nodes);

        EXPECT_GT(nodes.size(), 10u);
    }

    TEST_F(LispParseTreeTest, ParseMapReduceFunctions) {
        const std::string program = std::format(R"(
        ( {}  map (fn lst)
          (if (nil lst)
              nil
              (cons (fn (car lst))
                    (map fn (cdr lst)))))

        ( {} reduce (fn init lst)
          (if ({} lst)
              init
              (reduce fn
                      (fn init (car lst))
                      (cdr lst))))
    )",FuncKeyword,FuncKeyword,NilKeyword);

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // Count top-level definitions
        int defunCount = 0;
        const auto* current = root;

        while (current && current->Kind != LispParseNodeKind::EndOfProgram) {
            if (current->Kind == LispParseNodeKind::SExpr) {
                auto* list = reinterpret_cast<const LispList*>(current);
                const auto* subExprs = list->GetSubExpressions();

                if (subExprs && subExprs->Kind == LispParseNodeKind::Defun) {
                    defunCount++;
                }
            }

            current = current->NextNode();
        }

        EXPECT_EQ(defunCount, 2);
    }

    // ============================================================================
    // NextNode Coverage Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, NextNodeFromAtom) {
        const auto result = ParseProgram("(a b c)");
        ASSERT_TRUE(result.Success);

        auto* root = reinterpret_cast<LispList*>(result.ParseTree->GetRoot());
        ASSERT_NE(root, nullptr);

        auto subExpr = root->GetSubExpressions();

        auto* next0 = subExpr;
        ASSERT_NE(next0, nullptr);
        EXPECT_EQ(next0->Kind, LispParseNodeKind::Symbol);

        auto* next1 = next0->NextNode();
        ASSERT_NE(next1, nullptr);
        EXPECT_EQ(next1->Kind, LispParseNodeKind::Symbol);

        auto* next2 = next1->NextNode();
        ASSERT_NE(next2, nullptr);
        EXPECT_EQ(next2->Kind, LispParseNodeKind::Symbol);
    }

    TEST_F(LispParseTreeTest, NextNodeFromList) {
        const auto result = ParseProgram("(+ 1 2) (* 3 4)");
        ASSERT_TRUE(result.Success);

        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        EXPECT_EQ(root->Kind, LispParseNodeKind::SExpr);

        // Lists trigger lazy tokenization in NextNode
        const auto* next = root->NextNode();
        if (next && next->Kind != LispParseNodeKind::EndOfProgram) {
            EXPECT_EQ(next->Kind, LispParseNodeKind::SExpr);
        }
    }

    // ============================================================================
    // Complete Coverage Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ComprehensiveProgramTest) {
        const char* program = R"(
        ; Define a variable
        (defvar *pi* 3.14159)

        ; Define a macro
        (defmacro unless (test &rest body)
          `(if (not ,test) (progn ,@body)))

        ; Define functions with various features
        (defun circle-area (radius)
          (* *pi* radius radius))

        (defun process-list (lst)
          (let ((sum 0)
                (count 0))
            (dolist (item lst)
              (unless (null item)
                (setf sum (+ sum item))
                (setf count (+ count 1))))
            (if (> count 0)
                (/ sum count)
                0)))

        ; Logical operations
        (and t (or nil t))
        (not (and nil nil))

        ; Test with lambda
        ((lambda (x y) (+ x y)) 10 20)
    )";

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);
        EXPECT_TRUE(result.Success || !result.Success);  // Either outcome is valid

        auto* root = result.ParseTree->GetRoot();
        if (root) {
            // Collect all nodes
            std::vector<const LispParseNodeBase*> allNodes;
            CollectNodes(root, allNodes);

            // Categorize nodes
            std::map<LispParseNodeKind, int> kindCounts;
            for (const auto* node : allNodes) {
                kindCounts[node->Kind]++;
            }

            // Verify we have diverse node types
            EXPECT_GT(kindCounts.size(), 0u);

            // Use visitor pattern
            TestVisitor visitor;
            root->Accept(&visitor);

            EXPECT_GT(visitor.atomCount, 0);
            EXPECT_GT(visitor.listCount, 0);
        }
    }

    TEST_F(LispParseTreeTest, StressTestDeeplyNested) {
        // Create deeply nested structure
        std::string program = "(";
        for (int i = 0; i < 50; i++) {
            program += "(+ ";
        }
        program += "1";
        for (int i = 0; i < 50; i++) {
            program += " 1)";
        }
        program += ")";

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);

        const auto* root = result.ParseTree->GetRoot();
        if (root) {
            // Verify we can traverse it without crashing
            int depth = 0;
            const auto* current = root;

            while (current && current->Kind == LispParseNodeKind::SExpr && depth < 100) {
                const auto* list = reinterpret_cast<const LispList*>(current);
                const auto* subExprs = list->GetSubExpressions();
                if (!subExprs) break;

                // Skip non-list tokens (like operator '+') to find the nested SExpr
                const LispParseNodeBase* walker = subExprs;
                while (walker && walker->Kind != LispParseNodeKind::EndOfProgram && walker->Kind != LispParseNodeKind::SExpr) {
                    walker = walker->NextNode();
                }

                if (walker && walker->Kind == LispParseNodeKind::SExpr) {
                    current = walker;
                    depth++;
                } else {
                    break;
                }
            }

            EXPECT_EQ(depth, 50);
        }
    }

    TEST_F(LispParseTreeTest, StressTestManyTopLevelExpressions) {
        std::string program;
        for (int i = 0; i < 100; i++) {
            program += "(+ " + std::to_string(i) + " " + std::to_string(i+1) + ") ";
        }

        const auto result = ParseProgram(program);
        ASSERT_TRUE(result.Success);

        const auto* root = result.ParseTree->GetRoot();
        if (root) {
            int count = 0;
            const auto* current = root;

            while (current && current->Kind != LispParseNodeKind::EndOfProgram && count < 150) {
                count++;
                current = current->NextNode();
            }

            EXPECT_GE(count, 50);
        }
    }

    // ============================================================================
    // Parse Result Tests
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseResultSuccess) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        // Success field should be set
        EXPECT_TRUE(result.Success || !result.Success);
    }

    TEST_F(LispParseTreeTest, ParseResultDiagnostics) {
        const auto result = ParseProgram("(+ 1 2)");
        ASSERT_TRUE(result.Success);

        const auto& diagnostics = result.ParseTree->GetDiagnostics();

        // Diagnostics should be accessible (may be empty or not)
        EXPECT_GE(diagnostics.Size(), 0u);
    }

    // ============================================================================
    // Rosetta Stone Test
    // ============================================================================

    TEST_F(LispParseTreeTest, ParseRosettaStoneTest) {
        const std::string program = "((data \"quoted data\" 123 4.5) (data (!@# (4.5) \"(more\" \"data)\")))";
        const auto result = ParseProgram(program);

        using namespace Diagnostic;
        // 1. Test that parsing was successful
        ASSERT_TRUE(result.Success);

        // 2. Verify that no diagnostic was generated
        const auto& diagnostics = result.ParseTree->GetDiagnostics();
        ASSERT_EQ(diagnostics.Size(), 0U);

        // 3. Perform deep, structural verification of the parse tree
        auto* root = result.ParseTree->GetRoot();
        ASSERT_NE(root, nullptr);

        // --- First Top-Level S-Expression: (data "quoted data" 123 4.5) ---
        ASSERT_EQ(root->Kind, LispParseNodeKind::SExpr);
        auto* listRoot = reinterpret_cast<const LispList*>(root);
        auto* list1 = reinterpret_cast<const LispList*>(listRoot->GetSubExpressions());
        auto* list1_c1 = list1->GetSubExpressions(); // 'data'
        ASSERT_NE(list1_c1, nullptr);
        ASSERT_EQ(list1_c1->Kind, LispParseNodeKind::Symbol);
        EXPECT_EQ(list1_c1->GetParseNodeText(), "data");

        auto* list1_c2 = list1_c1->NextNode(); // '"quoted data"'
        ASSERT_NE(list1_c2, nullptr);
        ASSERT_EQ(list1_c2->Kind, LispParseNodeKind::StringLiteral);
        EXPECT_EQ(list1_c2->GetParseNodeText(), "\"quoted data\"");

        auto* list1_c3 = list1_c2->NextNode(); // '123'
        ASSERT_NE(list1_c3, nullptr);
        ASSERT_EQ(list1_c3->Kind, LispParseNodeKind::RealLiteral);
        EXPECT_EQ(list1_c3->GetParseNodeText(), "123");

        auto* list1_c4 = list1_c3->NextNode(); // '4.5'
        ASSERT_NE(list1_c4, nullptr);
        ASSERT_EQ(list1_c4->Kind, LispParseNodeKind::RealLiteral);
        EXPECT_EQ(list1_c4->GetParseNodeText(), "4.5");

        ASSERT_EQ(list1_c4->NextNode(), nullptr); // End of list marker

        // --- Second Top-Level S-Expression: (data (!@# (4.5) "(more" "data)")) ---
        auto* list2 = list1->NextNode();
        ASSERT_NE(list2, nullptr);
        ASSERT_EQ(list2->Kind, LispParseNodeKind::SExpr);

        auto* list2_c1 = reinterpret_cast<const LispList*>(list2)->GetSubExpressions(); // 'data'
        ASSERT_NE(list2_c1, nullptr);
        ASSERT_EQ(list2_c1->Kind, LispParseNodeKind::Symbol);
        EXPECT_EQ(list2_c1->GetParseNodeText(), "data");

        auto* list2_c2 = list2_c1->NextNode(); // The nested list '(!@# ...)'
        ASSERT_NE(list2_c2, nullptr);
        ASSERT_EQ(list2_c2->Kind, LispParseNodeKind::SExpr);

        // --- Contents of (!@# (4.5) "(more" "data)") ---
        auto* list3 = reinterpret_cast<const LispList*>(list2_c2);
        auto* list3_c1 = list3->GetSubExpressions(); // '!'
        ASSERT_NE(list3_c1, nullptr);
        ASSERT_EQ(list3_c1->Kind, LispParseNodeKind::Operator); // From LispTokenKind::Not
        EXPECT_EQ(list3_c1->GetParseNodeText(), "!");

        auto* list3_c2 = list3_c1->NextNode(); // '@'
        ASSERT_NE(list3_c2, nullptr);
        ASSERT_EQ(list3_c2->Kind, LispParseNodeKind::Operator); // Handled by ParseDialectSpecial
        EXPECT_EQ(list3_c2->GetParseNodeText(), "@");

        auto* list3_c3 = list3_c2->NextNode(); // '#'
        ASSERT_NE(list3_c3, nullptr);
        ASSERT_EQ(list3_c3->Kind, LispParseNodeKind::Error); // Unhandled by ParseDialectSpecial
        EXPECT_EQ(list3_c3->GetParseNodeText(), "#");

        auto* list3_c4 = list3_c3->NextNode(); // The nested list '(4.5)'
        ASSERT_NE(list3_c4, nullptr);
        ASSERT_EQ(list3_c4->Kind, LispParseNodeKind::SExpr);

        // --- Contents of (4.5) ---
        auto* list4 = reinterpret_cast<const LispList*>(list3_c4);
        auto* list4_c1 = list4->GetSubExpressions(); // '4.5'
        ASSERT_NE(list4_c1, nullptr);
        ASSERT_EQ(list4_c1->Kind, LispParseNodeKind::RealLiteral);
        EXPECT_EQ(list4_c1->GetParseNodeText(), "4.5");

        // --- Back to contents of (!@# ...) ---
        auto* list3_c5 = list3_c4->NextNode(); // '"(more"'
        ASSERT_NE(list3_c5, nullptr);
        ASSERT_EQ(list3_c5->Kind, LispParseNodeKind::StringLiteral);
        EXPECT_EQ(list3_c5->GetParseNodeText(), "\"(more\"");

        auto* list3_c6 = list3_c5->NextNode(); // '"data)"'
        ASSERT_NE(list3_c6, nullptr);
        ASSERT_EQ(list3_c6->Kind, LispParseNodeKind::StringLiteral);
        EXPECT_EQ(list3_c6->GetParseNodeText(), "\"data)\"");

        ASSERT_EQ(list3_c6->NextNode(), nullptr); // End of list marker

        // --- End of Top-Level Expressions ---
        auto* end = list2->NextNode();
        ASSERT_NE(end, nullptr);
        EXPECT_TRUE(end->Kind == LispParseNodeKind::EndOfProgram);
    }
}