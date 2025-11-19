#ifndef LISPPARSETREE_H
#define LISPPARSETREE_H
#include <cstdint>
#include <ranges>
#include "BumpVector.h"
#include "LispParser.h"
#include "PaddedString.h"

namespace WideLips {
    namespace Examples {
        class SchemeParser;
    }

    class LispParseTreeBuilder;
    struct SourceLocation;
    struct LispParseNodeBase;
    template <typename T>
    struct  LispParseNode;
    template<typename TConcreteVisitor>
    struct LispParseTreeVisitor;
    template<typename TConcreteVisitor>
    struct ImmutableLispParseTreeVisitor;
    struct LispAtom;
    struct LispList;
    struct LispSentinel;
    struct LispAuxiliary;
    class LispParseTree;

    enum class LispParseNodeKind : std::uint8_t {
        SExpr,
        /*
         * even though Not,LogicalAnd,LogicalOr are operators really but since they may be used as keywords
         * 'and','or','not' in some dialects we need to explicitly specify them as parse node kind, since the
         * default parser expect that all keywords tokens have corrosponding parse node
         */
        Not=33,
        Symbol=128,
        LogicalAnd=133,
        LogicalOr,
        RealLiteral,
        StringLiteral,
        BooleanLiteral,
        Let = 138,
        Lambda = 139,
        Nil = 140,
        If = 141,
        Defun = 142,
        Defmacro = 143,
        Defvar = 144,
        Operator,
        Arguments,
        EndOfProgram,
        Error,
    };

    struct SourceLocation {
    public:
        const std::uint32_t Line;
        const std::uint32_t ColumnChar;
    public:
        constexpr SourceLocation(const std::uint32_t line,const std::uint32_t column) : Line(line),ColumnChar(column) {}
        SourceLocation(const SourceLocation& ) = delete;
        SourceLocation(SourceLocation&&) = delete;
        SourceLocation& operator=(const SourceLocation&) = delete;
        SourceLocation& operator=(SourceLocation&&) = delete;
    public:
        NODISCARD ALWAYS_INLINE static consteval SourceLocation Default(){
            return SourceLocation{0,0};
        }
    };

    struct LispParseNodeBase {
        template<typename T>
        friend struct LispParseNode;
        friend struct LispAtom;
        friend struct LispList;
        friend struct LispArguments;
        friend struct LispAuxiliary;
        friend class LispParseTree;
        friend class LispParser;
        friend class Examples::SchemeParser;
    protected:
        using LispParseNodeBasePointer = LispParseNodeBase*;
        using LispParseNodeAuxiliaryPointer = LispAuxiliary*;
    protected:
        mutable LispParseNodeBasePointer Next;
        LispParser* Parser;
    public:
        const LispParseNodeKind Kind;
    public:
        explicit LispParseNodeBase(const LispParseNodeBasePointer next,LispParser* parser,const LispParseNodeKind kind):
        Next(next) ,
        Parser(parser),
        Kind(kind) {}
        LispParseNodeBase(const LispParseNodeBase&) = delete;
        LispParseNodeBase(LispParseNodeBase&&) = delete;
        LispParseNodeBase& operator=(const LispParseNodeBase&) = delete;
        LispParseNodeBase& operator=(LispParseNodeBase&&) = delete;
    public:
        NODISCARD const LispParseNodeBase* NextNode() const noexcept;

        NODISCARD LispParseNodeBase* NextNode() noexcept;

        NODISCARD SourceLocation GetSourceLocation() const;

        NODISCARD std::string_view GetParseNodeText() const;

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor);

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const;

        NODISCARD const LispAuxiliary * GetNodeAuxiliary() const;
    };

    template<typename TConcreteVisitor>
    struct LispParseTreeVisitor {
        ALWAYS_INLINE void Visit(LispAtom * const atom) {
            reinterpret_cast<TConcreteVisitor*>(this)->Visit(atom);
        }
        ALWAYS_INLINE void Visit(LispList * const list) {
            reinterpret_cast<TConcreteVisitor*>(this)->Visit(list);
        }
        ALWAYS_INLINE void Visit(LispArguments * const arguments) {
            reinterpret_cast<TConcreteVisitor*>(this)->Visit(arguments);
        }
        ALWAYS_INLINE void Visit(LispParseError * const error) {
            reinterpret_cast<TConcreteVisitor*>(this)->Visit(error);
        }
    protected:
        template<typename MostDerivedVisitor>
        ALWAYS_INLINE void Dispatch(LispParseNodeBase * const node) requires std::derived_from<MostDerivedVisitor,TConcreteVisitor> {
            switch (node->Kind) {
                case LispParseNodeKind::SExpr:
                    reinterpret_cast<TConcreteVisitor*>(this)->Visit(reinterpret_cast<LispList*>(node));
                    break;
                case LispParseNodeKind::Arguments:
                    reinterpret_cast<TConcreteVisitor*>(this)->Visit(reinterpret_cast<LispArguments*>(node));
                    break;
                case LispParseNodeKind::Error:
                    reinterpret_cast<TConcreteVisitor*>(this)->Visit(reinterpret_cast<LispParseError*>(node));
                    break;
                default:
                    reinterpret_cast<TConcreteVisitor*>(this)->Visit(reinterpret_cast<LispAtom*>(node));
                    break;
            }
        }
    };

    template<typename TConcreteVisitor>
    struct ImmutableLispParseTreeVisitor {
        ALWAYS_INLINE void Visit(const LispAtom * const atom) const {
            reinterpret_cast<const TConcreteVisitor*>(this)->Visit(atom);
        }
        ALWAYS_INLINE void Visit(const LispList * const list) const {
            reinterpret_cast<const TConcreteVisitor*>(this)->Visit(list);
        }
        ALWAYS_INLINE void Visit(const LispArguments * const arguments) const{
            reinterpret_cast<const TConcreteVisitor*>(this)->Visit(arguments);
        }
        ALWAYS_INLINE void Visit(const LispParseError * const error) const{
            reinterpret_cast<const TConcreteVisitor*>(this)->Visit(error);
        }
    protected:
        template<typename MostDerivedVisitor>
        ALWAYS_INLINE void Dispatch(const LispParseNodeBase * const node) const requires std::derived_from<MostDerivedVisitor,TConcreteVisitor>{
            switch (node->Kind) {
                case LispParseNodeKind::SExpr:
                    reinterpret_cast<const MostDerivedVisitor*>(this)->Visit(reinterpret_cast<const LispList*>(node));
                    break;
                case LispParseNodeKind::Arguments:
                    reinterpret_cast<const MostDerivedVisitor*>(this)->Visit(reinterpret_cast<const LispArguments*>(node));
                    break;
                case LispParseNodeKind::Error:
                    reinterpret_cast<const MostDerivedVisitor*>(this)->Visit(reinterpret_cast<const LispParseError*>(node));
                    break;
                default:
                    reinterpret_cast<const MostDerivedVisitor*>(this)->Visit(reinterpret_cast<const LispAtom*>(node));
                    break;
            }
        }
    };

    template<typename TLispNode>
    struct LispParseNode : LispParseNodeBase{
        friend TLispNode;
    private:
        mutable LispParseNodeAuxiliaryPointer _nodeAuxiliary;
    protected:
        LispParseNode(const LispParseNodeBasePointer next,
            LispParser* parser,
            const LispParseNodeKind kind,
            const LispParseNodeAuxiliaryPointer nodeAuxiliary)  :
        LispParseNodeBase(next,parser,kind),
        _nodeAuxiliary{nodeAuxiliary}{}
    public:
        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const{
            return reinterpret_cast<const TLispNode*>(this)->GetSourceLocation();
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return reinterpret_cast<const TLispNode*>(this)->GetParseNodeText();
        }

        NODISCARD ALWAYS_INLINE const LispAuxiliary * GetNodeAuxiliary() const {
            return reinterpret_cast<const TLispNode*>(this)->GetNodeAuxiliary();
        }
    protected:
        NODISCARD ALWAYS_INLINE const LispAuxiliary * GetNodeAuxiliary(const LispToken* token) const {
            if (_nodeAuxiliary != nullptr) {
                return _nodeAuxiliary;
            }
            LispLexer* const lexer = Parser->GetLexer();
            auto optAuxiliary = lexer->GetTokenAuxiliary(token);
            if (!optAuxiliary) {
                if (token->AuxiliaryIndex == std::numeric_limits<std::uint8_t>::max()) {
                    lexer->GetDiagnostics().EmplaceBack(Diagnostic::DiagnosticFactory::FetchingAuxiliaryOfLazyToken(
                        Parser->OriginFile(),
                        token->Line,
                        token->Column,
                        *token));
                }
                return nullptr;
            }
            const auto& [auxBeg,auxEnd] = optAuxiliary.value();
            _nodeAuxiliary = Parser->MakeAuxiliary(auxBeg,auxEnd);
            return _nodeAuxiliary;
        }
    };

    struct LispAtom final : LispParseNode<LispAtom> {
        friend struct LispParseNodeBase;
        friend struct LispParseNode;
        friend struct LispArguments;
    private:
        const LispToken * const _token;
        const LispParseNodeKind _kind;
    public:
        explicit LispAtom(const LispToken * const token,
            const LispParseNodeKind kind,
            const LispParseNodeBasePointer next,
            const LispParseNodeAuxiliaryPointer nodeAuxiliary,
            LispParser* parser):
        LispParseNode (next,parser,kind,nodeAuxiliary),
        _token(token),
        _kind(kind){}
    public:
        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const {
            return SourceLocation{_token->Line,  _token->Column};
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return _token->GetText();
        }

        NODISCARD ALWAYS_INLINE LispTokenKind GetUnderlyingKind() const {
            return _token->Kind;
        }

        NODISCARD ALWAYS_INLINE const LispAuxiliary * GetNodeAuxiliary() const {
            return LispParseNode::GetNodeAuxiliary(_token);
        }

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor) {
            visitor->Visit(this);
        }

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const {
            visitor->Visit(this);
        }
    };

    struct LispList final : LispParseNode<LispList> {
        friend struct LispParseNodeBase;
        friend struct LispParseNode;
        friend class LispParser;
    private:
        const LispToken * const _sexprBegin;
        const LispToken * const _sexprEnd;
        LispParseNodeBasePointer _subExpressions;
    public:
        LispList(const LispToken * const sexprBegin,
            const LispToken * const sexprEnd,
            const LispParseNodeBasePointer subExpressions,
            const LispParseNodeBasePointer next,
            const LispParseNodeAuxiliaryPointer nodeAuxiliary,
            LispParser* parser) :
        LispParseNode (next,parser,LispParseNodeKind::SExpr,nodeAuxiliary),
        _sexprBegin(sexprBegin),
        _sexprEnd(sexprEnd),
        _subExpressions(subExpressions) {}
    public:
        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const {
            return {_sexprBegin->Line, _sexprBegin->Column};
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return std::string_view{_sexprBegin->TextPtr,_sexprEnd->TextPtr};
        }

        /**
         * Retrieves the sub-expressions of the current Lisp parse node.
         * If the sub-expressions have not been parsed yet, this method will
         * lazily tokenize and parse them.
         *
         * @param csEmptySExpr Indicates whether an empty S-expression is context-sensitive, if set to 'true',
         *                     then the parser will not consider this S-expression as a parsing error,
         *                     Defaults to `false`.
         * @return A pointer to the root of the parsed sub-expressions if successful,
         *         or `nullptr` if the sub-expressions could not be parsed or are empty.
         */
        NODISCARD ALWAYS_INLINE const LispParseNodeBase* GetSubExpressions (const bool csEmptySExpr=false) const {
            LispLexer* const lexer = Parser->GetLexer();
            const auto sexprRegion = lexer->TokenizeSExpr(_sexprBegin,csEmptySExpr);
            if (!sexprRegion) {
                if constexpr (DisallowEmptySExpr) {
                    // ReSharper disable once CppDFAUnreachableCode
                    lexer->GetDiagnostics().EmplaceBack(Diagnostic::DiagnosticFactory::EmptySExpression(
                          lexer->GetFilePath(),
                          _sexprBegin->Line,
                          _sexprBegin->Column)
                          );
                }
                return nullptr;
            }
            const auto& [subExprBegin,subExprEnd] = sexprRegion.value();
            return Parser->Parse(subExprBegin,subExprEnd);;
        }

        /**
        * Retrieves the sub-expressions of the current Lisp parse node.
        * If the sub-expressions have not been parsed yet, this method will
        * lazily tokenize and parse them.
        *
        * @param csEmptySExpr Indicates whether an empty S-expression is context-sensitive, if set to 'true',
        *                     then the parser will not consider this S-expression as a malformed S-expression if it's
        *                     empty,
        *                     Defaults to `false`.
        * @note if the parser was built with 'InvalidateEmptySExpr' undefined, then parameter csEmptySExpr
        * can be ignored
        * @return A pointer to the root of the parsed sub-expressions if successful,
        *         or `nullptr` if the sub-expressions could not be parsed or are empty.
        */
        NODISCARD ALWAYS_INLINE LispParseNodeBase* GetSubExpressions (const bool csEmptySExpr=false) {
            if (_subExpressions != nullptr) {
                return _subExpressions;
            }
            LispLexer* const lexer = Parser->GetLexer();
            const auto sexprRegion = lexer->TokenizeSExpr(_sexprBegin,csEmptySExpr);
            if (!sexprRegion) {
                if constexpr (DisallowEmptySExpr) {
                    // ReSharper disable once CppDFAUnreachableCode
                    lexer->GetDiagnostics().EmplaceBack(Diagnostic::DiagnosticFactory::EmptySExpression(
                          lexer->GetFilePath(),
                          _sexprBegin->Line,
                          _sexprBegin->Column)
                          );
                }
                return nullptr;
            }
            const auto& [subExprBegin,subExprEnd] = sexprRegion.value();
            _subExpressions = Parser->Parse(subExprBegin,subExprEnd);
            return _subExpressions;
        }

        NODISCARD ALWAYS_INLINE const LispAuxiliary * GetNodeAuxiliary() const {
            return LispParseNode::GetNodeAuxiliary(_sexprBegin);
        }

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor) {
            visitor->Visit(this);
        }

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const {
            visitor->Visit(this);
        }
    };

    struct LispArguments final : LispParseNode<LispArguments> {
        friend struct LispParseNodeBase;
        friend struct LispParseNode;
    private:
        const LispToken * const _argumentsBegin;
        const LispToken * const _argumentsEnd;
        LispParseNodeBasePointer _arguments;
    public:
        LispArguments(const LispToken * const argumentsBegin,
            const LispToken * const argumentsEnd,
            const LispParseNodeBasePointer arguments,
            const LispParseNodeBasePointer next,
            const LispParseNodeAuxiliaryPointer nodeAuxiliary,
            LispParser* parser):
        LispParseNode(next,parser,LispParseNodeKind::SExpr,nodeAuxiliary),
        _argumentsBegin(argumentsBegin),
        _argumentsEnd(argumentsEnd),
        _arguments(arguments){}
    public:
        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const {
            return {_argumentsBegin->Line, _argumentsBegin->Column};
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return std::string_view{&_argumentsBegin->GetText().front(),&_argumentsEnd->GetText().back()+1};
        }

        NODISCARD ALWAYS_INLINE const LispAuxiliary * GetNodeAuxiliary() const {
            return LispParseNode::GetNodeAuxiliary(_argumentsBegin);
        }

        NODISCARD ALWAYS_INLINE LispParseNodeBase* GetArguments() const {
            return _arguments;
        }

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor) {
            visitor->Visit(this);
        }

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const {
            visitor->Visit(this);
        }
    };

    struct LispParseError final : LispParseNode<LispParseError> {
        friend struct LispParseNodeBase;
    private:
        const LispToken * const _errorToken;
        LispParseNodeAuxiliaryPointer _nodeAuxiliary;
    public:
        LispParseError(const LispToken* errorToken,
            const LispParseNodeBasePointer next,
            const LispParseNodeAuxiliaryPointer nodeAuxiliary,
            LispParser* parser):
        LispParseNode(next,parser,LispParseNodeKind::Error,nodeAuxiliary),
        _errorToken(errorToken),
        _nodeAuxiliary(nodeAuxiliary){}
    public:

        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const {
            return {_errorToken->Line, _errorToken->Column};
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return std::string_view{&_errorToken->GetText().front(),&_errorToken->GetText().back()+1};
        }

        NODISCARD const LispAuxiliary * GetNodeAuxiliary() const {
            return LispParseNode::GetNodeAuxiliary(_errorToken);
        }

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor) {
            visitor->Visit(this);
        }

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const {
            visitor->Visit(this);
        }
    };

    struct LispAuxiliary final {
    private:
        using AuxiliariesRange = const std::pair<const LispToken*,const LispToken*>;
        AuxiliariesRange _auxiliariesRange;
    public:
        explicit LispAuxiliary(AuxiliariesRange auxiliaries_range) :_auxiliariesRange(auxiliaries_range){}
    public:
        NODISCARD ALWAYS_INLINE SourceLocation GetSourceLocation() const {
            return {_auxiliariesRange.first->Line, _auxiliariesRange.first->Column};
        }

        NODISCARD ALWAYS_INLINE std::string_view GetParseNodeText() const {
            return std::string_view{&_auxiliariesRange.first->GetText().front(),&_auxiliariesRange.second->GetText().back()+1};
        }
    };

    struct LispParseResult final {
        bool Success;
        std::unique_ptr<LispParseTree> ParseTree;
    };

    class LispParseTree final {
    private:
        using LispParseNodeBasePointer = LispParseNodeBase*;
    private:
        struct ConstructorEnabler {constexpr ConstructorEnabler()= default;};
    private:
        static constexpr ConstructorEnabler CtorEnabler {};
    private:
        std::string _filePath;
        std::unique_ptr<LispParser> _parser;
        LispParseNodeBasePointer _root;
        const BumpVector<Diagnostic::LispDiagnostic>& _diagnostics;
        const bool _canBeConsumed;
        const PaddedString _program;
    public:
        LispParseTree(UNUSED ConstructorEnabler enabler,
            const std::string_view filePath,
            std::unique_ptr<LispParser> parser,
            const LispParseNodeBasePointer root,
            const BumpVector<Diagnostic::LispDiagnostic>& diagnostics,
            const bool canBeConsumed,
            PaddedString&& program):
        _filePath(filePath),
        _parser(std::move(parser)),
        _root(root),
        _diagnostics(diagnostics),
        _canBeConsumed(canBeConsumed),
        _program(std::move(program)){}

        LispParseTree(UNUSED ConstructorEnabler enabler,
            const std::string_view filePath,
            std::unique_ptr<LispParser> parser,
            const LispParseNodeBasePointer root,
            const BumpVector<Diagnostic::LispDiagnostic>& diagnostics,
            const bool canBeConsumed,
            const PaddedString& program):
        _filePath(filePath),
        _parser(std::move(parser)),
        _root(root),
        _diagnostics(diagnostics),
        _canBeConsumed(canBeConsumed),
        _program(program){}

        LispParseTree(const LispParseTree&) = delete;
        LispParseTree(LispParseTree&&) = delete;
        LispParseTree& operator=(const LispParseTree&) = delete;
        LispParseTree& operator=(LispParseTree&&) = delete;
    public:
        template<WideLipsParser TParser = LispParser>
        NODISCARD static LispParseResult Parse(const std::filesystem::path& filePath,bool conservative) {
            auto parser = std::make_unique<TParser>(filePath,conservative);
            auto parsedProgram = parser->Parse();
            bool success = parsedProgram != nullptr;
            for (auto&& diagnostic : parser->GetDiagnostics()) {
                if (diagnostic.GetSeverity() == Diagnostic::Severity::Error) {
                    success = false;
                    break;
                }
            }
            return {
                .Success = success,
                .ParseTree = std::make_unique<LispParseTree>(CtorEnabler,
                    filePath.string(),
                    std::move(parser),
                    parsedProgram,
                    parser->GetDiagnostics(),
                    success,
                    EmptyPaddedString::GetPaddedString())
            };
        }

        template<WideLipsParser TParser = LispParser>
        NODISCARD static LispParseResult Parse(PaddedString&& paddedString,bool conservative) {
            auto parser = std::make_unique<TParser>(paddedString.GetUnderlyingString(), conservative);
            auto parsedProgram = static_cast<LispParser*>(parser.get())->Parse();
            bool success = true;
            for (auto&& diagnostic : static_cast<LispParser*>(parser.get())->GetDiagnostics()) {
                if (diagnostic.GetSeverity() == Diagnostic::Severity::Error) {
                    success = false;
                    break;
                }
            }
            auto result = LispParseResult {
                .Success = success,
                .ParseTree = std::make_unique<LispParseTree>(CtorEnabler,
                    "memory",
                    std::move(parser),
                    parsedProgram,
                    parser->GetDiagnostics(),
                    success,
                    std::move(paddedString))
            };
            return std::move(result);
        }

        NODISCARD static PaddedString MakeParserFriendlyString(std::string_view program) {
            return PaddedString{program,EOF,PaddingSize};
        }
    public:
        NODISCARD LispParseNodeBase * GetRoot() const noexcept {
            return _canBeConsumed ? _root : nullptr;
        }

        NODISCARD std::string_view GetFilePath() const noexcept {
            return _filePath;
        }

        NODISCARD ALWAYS_INLINE const BumpVector<Diagnostic::LispDiagnostic>& GetDiagnostics() const noexcept {
            return _diagnostics;
        }

        template<typename TConcreteVisitor>
        void Accept(LispParseTreeVisitor<TConcreteVisitor>* visitor) {
            if (!_canBeConsumed) {
                return;
            }
            _root->Accept(visitor);
        }

        template<typename TConcreteVisitor>
        void Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor>* visitor) const {
            if (!_canBeConsumed) {
                return;
            }
            _root->Accept(visitor);
        }
    };

    ALWAYS_INLINE const LispParseNodeBase *LispParseNodeBase::NextNode() const noexcept {
        if (Next) {
            return Next;
        }
        switch (Kind) {
            case LispParseNodeKind::SExpr: {
                const auto parentSExpr = reinterpret_cast<const LispList*>(this);
                LispLexer* const lexer = Parser->GetLexer();
                auto next = lexer->TokenizeNext(parentSExpr->_sexprBegin);
                if (!next) {
                    return Parser->MakeEndOfProgram();
                }
                const auto& [sexprBegin,sexprEnd] = next.value();
                return Parser->MakeList(sexprBegin,sexprEnd);
            }
            default:
                return Next;
        }
    }

    ALWAYS_INLINE LispParseNodeBase *LispParseNodeBase::NextNode() noexcept {
        if (Next) {
            return Next;
        }
        switch (Kind) {
            case LispParseNodeKind::SExpr: {
                const auto parentSExpr = reinterpret_cast<const LispList*>(this);
                LispLexer* const lexer = Parser->GetLexer();
                auto next = lexer->TokenizeNext(parentSExpr->_sexprBegin);
                if (!next) {
                    return Parser->MakeEndOfProgram();
                }
                const auto& [sexprBegin,sexprEnd] = next.value();
                Next = Parser->MakeList(sexprBegin,sexprEnd);
                return Next;
            }
            default:
                return Next;
        }
    }

    ALWAYS_INLINE SourceLocation LispParseNodeBase::GetSourceLocation() const {
        switch (Kind) {
            case LispParseNodeKind::SExpr:
                return reinterpret_cast<const LispList*>(this)->GetSourceLocation();
            case LispParseNodeKind::Arguments:
                return reinterpret_cast<const LispArguments*>(this)->GetSourceLocation();
            case LispParseNodeKind::Error:
                return reinterpret_cast<const LispParseError*>(this)->GetSourceLocation();
            default:
                return reinterpret_cast<const LispAtom*>(this)->GetSourceLocation();
        }
    }


    ALWAYS_INLINE std::string_view LispParseNodeBase::GetParseNodeText() const {
        switch (Kind) {
            case LispParseNodeKind::SExpr:
                return reinterpret_cast<const LispList*>(this)->GetParseNodeText();
            case LispParseNodeKind::Arguments:
                return reinterpret_cast<const LispArguments*>(this)->GetParseNodeText();
            case LispParseNodeKind::Error:
                return reinterpret_cast<const LispParseError*>(this)->GetParseNodeText();
            default:
                return reinterpret_cast<const LispAtom*>(this)->GetParseNodeText();
        }
    }


    template<typename TConcreteVisitor>
    ALWAYS_INLINE void LispParseNodeBase::Accept(LispParseTreeVisitor<TConcreteVisitor> *visitor) {
        switch (Kind) {
            case LispParseNodeKind::SExpr:
                visitor->Visit(reinterpret_cast<LispList*>(this));
                break;
            case LispParseNodeKind::Arguments:
                visitor->Visit(reinterpret_cast<LispArguments*>(this));
                break;
            case LispParseNodeKind::Error:
                visitor->Visit(reinterpret_cast<LispParseError*>(this));
                break;
            default:
                visitor->Visit(reinterpret_cast<LispAtom*>(this));
                break;
        }
    }

    template<typename TConcreteVisitor>
    void LispParseNodeBase::Accept(const ImmutableLispParseTreeVisitor<TConcreteVisitor> *visitor) const {
        switch (Kind) {
            case LispParseNodeKind::SExpr:
                visitor->Visit(reinterpret_cast<const LispList*>(this));
                break;
            case LispParseNodeKind::Arguments:
                visitor->Visit(reinterpret_cast<const LispArguments*>(this));
                break;
            case LispParseNodeKind::Error:
                visitor->Visit(reinterpret_cast<const LispParseError*>(this));
                return;
            default:
                visitor->Visit(reinterpret_cast<const LispAtom*>(this));
                break;
        }
    }

    inline const LispAuxiliary *LispParseNodeBase::GetNodeAuxiliary() const {
        switch (Kind) {
            case LispParseNodeKind::SExpr:
                return reinterpret_cast<const LispList*>(this)->GetNodeAuxiliary();
            case LispParseNodeKind::Arguments:
                return reinterpret_cast<const LispArguments*>(this)->GetNodeAuxiliary();
            case LispParseNodeKind::Error:
                return reinterpret_cast<const LispParseError*>(this)->GetNodeAuxiliary();
            default:
                return reinterpret_cast<const LispAtom*>(this)->GetNodeAuxiliary();
        }
    }

}

#undef is
#endif //LISPPARSETREE_H














