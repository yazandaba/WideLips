#include "SchemeParser.h"
#include "LispParseTree.h"
#include <iostream>

using namespace WideLips;
using namespace WideLips::Examples;
int main() {
    using namespace std::literals::string_literals;
    std::string_view schemeProgram = R"(
        (define (factorial n)
          (if (<= n 1)
              1
              (* n (factorial (- n 1)))))

        (define lst '(1 2 3 4 5))

        (define template `(a b ,lst ,@lst))
    )";

    auto paddedCore = LispParseTree::MakeParserFriendlyString(schemeProgram);
    auto [result,parseTree]= LispParseTree::Parse<SchemeParser>(std::move(paddedCore), false);
    const auto root = parseTree->GetRoot();
    const auto rootList = reinterpret_cast<LispList*>(parseTree->GetRoot());
    std::cout << "parsing status: " << result << "\n";
    if (root) {
        (void)rootList->GetSubExpressions();//trigger parsing of the first 'define' (you can walk the tree with)
        std::puts("Parse successful!\n");
        for (auto node = root; node != nullptr; node = node->NextNode()) {
            std::cout << "Node: " << node->GetParseNodeText()
                      << " at line " << node->GetSourceLocation().Line
                      << ", column " << node->GetSourceLocation().ColumnChar
                      << "\n";
        }
    } else {
        std::puts("Parse failed!");
    }

    const auto& diagnostics = parseTree->GetDiagnostics();
    if (!diagnostics.Empty()) {
        std::puts("\nDiagnostics:\n");
        for (const auto& diag : diagnostics) {
            std::wcout << "  " << diag.GetFullMessage() << "\n";
        }
    }

    return 0;
}