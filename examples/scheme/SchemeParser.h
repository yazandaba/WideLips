#ifndef WIDELIPS_SCHEMEPARSER_H
#define WIDELIPS_SCHEMEPARSER_H
#include "LispParser.h"
#include "LispParseTree.h"

namespace WideLips::Examples{

    class SchemeParser final : public LispParser {
    public:
        WL_API explicit SchemeParser(const std::filesystem::path& filePath, bool conservative = false);
        WL_API explicit SchemeParser(std::string_view program, bool conservative = false);

        NODISCARD WL_API LispParseNodeBase* Parse(const LispToken* sexprBegin, const LispToken* sexprEnd) override;

    protected:
        NODISCARD LispParseNodeBase* ParseDialectSpecial(const LispToken* currentToken) override;

    private:
        NODISCARD LispParseNodeBase* HandleQuote(const LispToken* quoteToken);
        NODISCARD LispParseNodeBase* HandleQuasiquote(const LispToken* quasiquoteToken);
        NODISCARD LispParseNodeBase* HandleUnquote(const LispToken* unquoteToken);
        NODISCARD LispParseNodeBase* HandleHash(const LispToken* hashToken);
    };

}

#endif //WIDELIPS_SCHEMEPARSER_H