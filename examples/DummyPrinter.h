#ifndef WIDELIPS_DUMMYPRINTER_H
#define WIDELIPS_DUMMYPRINTER_H
#include "LispParseTreeVisitor.h"

namespace WideLips::Examples {

    class DummyPrinter: public ImmutableLispParseTreeWalker<DummyPrinter> {
    public:
        void Visit(const LispAtom * const atom) const noexcept{
            if (atom->Kind == LispParseNodeKind::EndOfProgram) {
                return;
            }
            std::puts("Atom: ");
            std::puts(atom->GetParseNodeText().data());
            std::puts("\n");
        }

        void Visit(const LispList * const list) const noexcept{
            std::puts("List: ");
            std::puts(list->GetParseNodeText().data());
            std::puts("\n");
        }

        void Visit(const LispArguments* const arguments) const noexcept{
            std::puts("ArgsOrVec: ");
            std::puts(arguments->GetParseNodeText().data());
            std::puts("\n");
        }

        void Visit(const LispParseError * const error) const noexcept{
            std::puts("Error: ");
            std::puts(error->GetParseNodeText().data());
            std::puts("\n");
        }
    };
}
#endif //WIDELIPS_DUMMYPRINTER_H
