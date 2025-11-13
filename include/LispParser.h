#ifndef PARSER_H
#define PARSER_H
#include <memory_resource>
#include "LispLexer.h"

namespace WideLips {
    struct LispParseError;
    struct LispAuxiliary;
    struct LispAtom;

    class LispParser {
        friend struct LispParseNodeBase;
        template<typename T>
        friend struct LispParseNode;
        friend struct LispList;
        friend class LispParseTree;
    private:
        AlignedFileReadResult _optionalAlignedFile;
    protected:
        std::unique_ptr<LispLexer> Lexer;
        std::pmr::monotonic_buffer_resource ParseNodesPool;
        std::pmr::polymorphic_allocator<> ParseNodesAllocator;
        LispAtom* EndOfProgram;
    public:
        WL_API LispParser(const std::filesystem::path &filePath,bool conservative);
        WL_API LispParser(std::string_view program,bool conservative);
        LispParser(const LispParser&) = delete;
        LispParser(LispParser&& ) = delete;
        LispParser& operator=(const LispParser&) = delete;
        LispParser& operator=(LispParser&& ) = delete;
        WL_API virtual ~LispParser() = default;
    public:
        NODISCARD WL_API LispParseNodeBase* Parse();
        NODISCARD WL_API virtual LispParseNodeBase* Parse(const LispToken* sexprBegin,const LispToken* sexprEnd);
        NODISCARD WL_API const BumpVector<Diagnostic::LispDiagnostic>& GetDiagnostics() const;
        NODISCARD WL_API std::wstring_view OriginFile() const;
        WL_API void Reuse() const;
    protected:
        NODISCARD virtual LispParseNodeBase* ParseDialectSpecial(const LispToken* currentToken);
        NODISCARD LispLexer* GetLexer() const;
        WL_HIDDEN NODISCARD LispParseError* OnUnrecognizedToken(const LispToken* currentToken);
        WL_HIDDEN NODISCARD BumpVector<Diagnostic::LispDiagnostic>& GetDiagnosticsInternal() const;
        NODISCARD LispAuxiliary* MakeAuxiliary(const LispToken* auxBegin,const LispToken* auxEnd);
        NODISCARD LispList* MakeList(const LispToken* sexprBegin,const LispToken* sexprEnd);
        NODISCARD LispAtom * MakeEndOfProgram() const;
    };

    template<typename TParser>
    concept WideLipsParser = std::is_base_of_v<LispParser,TParser>;
}



#endif //PARSER_H
