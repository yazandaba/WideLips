#ifndef LISPPARSETREEVISITOR_H
#define LISPPARSETREEVISITOR_H
#include "LispParseTree.h"

namespace WideLips {
    template<typename Walker>
    struct LispParseTreeWalker : LispParseTreeVisitor<LispParseTreeWalker<Walker>> {
    public:
        void Visit(LispAtom * const atom) noexcept {
            if (atom == nullptr) {
                return;
            }
            reinterpret_cast<Walker*>(this)->Visit(atom->NextNode());
        }

        void Visit(LispList * const list) noexcept {
            const auto children = list->GetSubExpressions();
            while (children != nullptr) {
                reinterpret_cast<Walker*>(this)->Visit(children);
            }
            reinterpret_cast<Walker*>(this)->Visit(list->NextNode());
        }

        void Visit(LispArguments* const arguments) noexcept {
            reinterpret_cast<Walker*>(this)->Visit(arguments);
        }

        void Visit(LispParseError * const error) noexcept{
            reinterpret_cast<Walker*>(this)->Visit(error);
        }
    };

    template<typename Walker>
    struct ImmutableLispParseTreeWalker : ImmutableLispParseTreeVisitor<ImmutableLispParseTreeWalker<Walker>> {
    public:
        void Visit(const LispAtom * const atom) const noexcept{
            if (atom == nullptr) {
                return;
            }
            reinterpret_cast<const Walker*>(this)->Visit(atom->NextNode());
        }

        void Visit(const LispList * const list) const noexcept{
            const auto children = list->GetSubExpressions();
            while (children != nullptr) {
                 reinterpret_cast<const Walker*>(this)->Visit(children);
            }
            reinterpret_cast<const Walker*>(this)->Visit(list->NextNode());
        }

        void Visit(const LispArguments* const arguments) const noexcept{
            reinterpret_cast<const Walker*>(this)->Visit(arguments);
        }

        void Visit(const LispParseError * const error) const noexcept{
            reinterpret_cast<const Walker*>(this)->Visit(error);
        }

    };
}
#endif //LISPPARSETREEVISITOR_H
