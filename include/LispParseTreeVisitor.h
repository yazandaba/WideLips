#ifndef LISPPARSETREEVISITOR_H
#define LISPPARSETREEVISITOR_H
#include "LispParseTree.h"

namespace WideLips {
    template<typename Walker>
    struct LispParseTreeWalker : LispParseTreeVisitor<LispParseTreeWalker<Walker>> {
    public:
        ALWAYS_INLINE void Visit( LispAtom * const atom) noexcept{
            if (atom == nullptr) {
                return;
            }
            reinterpret_cast<Walker*>(this)->Visit(atom);
            reinterpret_cast<Walker*>(this)->Dispatch(atom->NextNode());
        }

        ALWAYS_INLINE void Visit( LispList * const list) noexcept{
            reinterpret_cast<Walker*>(this)->Visit(list);
            auto children = list->GetSubExpressions();
            while (children != nullptr) {
                this->template Dispatch<Walker>(children);
                children = children->NextNode();
            }
            this->template Dispatch<Walker>(list->NextNode());
        }

        ALWAYS_INLINE void Visit( LispArguments* const arguments) noexcept{
            reinterpret_cast<Walker*>(this)->Visit(arguments);
            auto args = arguments->GetArguments();
            while (args != nullptr) {
                this->template Dispatch<Walker>(args);
                args = args->NextNode();
            }
        }

        ALWAYS_INLINE void Visit( LispParseError * const error) noexcept{
            reinterpret_cast<Walker*>(this)->Visit(error);
            reinterpret_cast<Walker*>(this)->Dispatch(error->NextNode());
        }
    };

    template<typename Walker>
    struct ImmutableLispParseTreeWalker : ImmutableLispParseTreeVisitor<ImmutableLispParseTreeWalker<Walker>> {
    public:
        void Visit(const LispAtom * const atom) const noexcept{
            if (atom == nullptr) {
                return;
            }
            reinterpret_cast<const Walker*>(this)->Visit(atom);
            this->template Dispatch<Walker>(atom->NextNode());
        }

        void Visit(const LispList * const list) const noexcept{
            reinterpret_cast<const Walker*>(this)->Visit(list);
            auto children = list->GetSubExpressions();
            while (children != nullptr) {
                this->template Dispatch<Walker>(children);
                children = children->NextNode();
            }
            this->template Dispatch<Walker>(list->NextNode());
        }

        void Visit(const LispArguments* const arguments) const noexcept{
            reinterpret_cast<const Walker*>(this)->Visit(arguments);
            auto args = arguments->GetArguments();
            while (args != nullptr) {
                this->template Dispatch<Walker>(args);
                args = args->NextNode();
            }
        }

        void Visit(const LispParseError * const error) const noexcept{
            reinterpret_cast<const Walker*>(this)->Visit(error);
            this->template Dispatch<Walker>(error->NextNode());
        }

    };
}
#endif //LISPPARSETREEVISITOR_H


























