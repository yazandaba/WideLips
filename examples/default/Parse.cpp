#include "LispParseTree.h"
#include "../DummyPrinter.h"

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

    auto paddedCore = WideLips::LispParseTree::MakeParserFriendlyString(schemeProgram);
    auto [result,parseTree]= WideLips::LispParseTree::Parse(std::move(paddedCore), false);
    auto printer = WideLips::Examples::DummyPrinter{};
    if (result) {
        parseTree->Accept(&printer);
    }
}

