/*
    _____ _____ _____ __
   |   __|     |  |  |  |      The SOUL language
   |__   |  |  |  |  |  |__    Copyright (c) 2019 - ROLI Ltd.
   |_____|_____|_____|_____|

   The code in this file is provided under the terms of the ISC license:

   Permission to use, copy, modify, and/or distribute this software for any purpose
   with or without fee is hereby granted, provided that the above copyright notice and
   this permission notice appear in all copies.

   THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD
   TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN
   NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
   DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
   IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

namespace soul
{

//==============================================================================
/**
    High-level compiler AST classes
*/
struct AST
{
    #define SOUL_AST_MODULES(X) \
        X(Graph) \
        X(Processor) \
        X(Namespace) \

    #define SOUL_AST_EXPRESSIONS(X) \
        X(ConcreteType) \
        X(SubscriptWithBrackets) \
        X(SubscriptWithChevrons) \
        X(TypeMetaFunction) \
        X(Assignment) \
        X(BinaryOperator) \
        X(Constant) \
        X(DotOperator) \
        X(CallOrCast) \
        X(FunctionCall) \
        X(TypeCast) \
        X(PreOrPostIncOrDec) \
        X(ArrayElementRef) \
        X(StructMemberRef) \
        X(StructDeclaration) \
        X(UsingDeclaration) \
        X(TernaryOp) \
        X(UnaryOperator) \
        X(QualifiedIdentifier) \
        X(VariableRef) \
        X(InputEndpointRef) \
        X(OutputEndpointRef) \
        X(ProcessorRef) \
        X(CommaSeparatedList) \
        X(ProcessorProperty) \
        X(WriteToEndpoint) \
        X(AdvanceClock) \
        X(StaticAssertion) \

    #define SOUL_AST_STATEMENTS(X) \
        X(Block) \
        X(BreakStatement) \
        X(ContinueStatement) \
        X(IfStatement) \
        X(LoopStatement) \
        X(NoopStatement) \
        X(ReturnStatement) \
        X(VariableDeclaration) \
        SOUL_AST_EXPRESSIONS(X) \

    #define SOUL_AST_OBJECTS(X) \
        X(Function) \
        X(ProcessorAliasDeclaration) \
        X(Connection) \
        X(ProcessorInstance) \
        X(EndpointDeclaration) \
        SOUL_AST_STATEMENTS(X) \

    #define SOUL_AST_ALL_TYPES(X) \
        SOUL_AST_MODULES (X) \
        SOUL_AST_OBJECTS (X)

    #define SOUL_PREDECLARE_TYPE(Type)   struct Type;
    SOUL_AST_ALL_TYPES (SOUL_PREDECLARE_TYPE)
    #undef SOUL_PREDECLARE_TYPE

    enum class ObjectType
    {
        #define SOUL_DECLARE_ENUM(Type)    Type,
        SOUL_AST_ALL_TYPES (SOUL_DECLARE_ENUM)
        #undef SOUL_DECLARE_ENUM
    };

    static constexpr size_t maxIdentifierLength = 128;
    static constexpr size_t maxInitialiserListLength = 1024 * 64;
    static constexpr size_t maxEndpointArraySize = 256;
    static constexpr size_t maxProcessorArraySize = 256;
    static constexpr size_t maxDelayLineLength = 1024 * 256;


    struct ASTObject;
    struct Scope;
    struct ModuleBase;
    struct ProcessorBase;
    struct FunctionSignature;
    struct Expression;
    struct Statement;

    using TypeArray = ArrayWithPreallocation<Type, 8>;

    enum class ExpressionKind  { value, type, endpoint, processor, unknown };

    static bool isPossiblyType (Expression& e)                   { return e.kind == ExpressionKind::type     || e.kind == ExpressionKind::unknown; }
    static bool isPossiblyValue (Expression& e)                  { return e.kind == ExpressionKind::value    || e.kind == ExpressionKind::unknown; }
    static bool isPossiblyEndpoint (Expression& e)               { return e.kind == ExpressionKind::endpoint || e.kind == ExpressionKind::unknown; }

    static bool isPossiblyType (pool_ptr<Expression> e)          { return e != nullptr && isPossiblyType (*e); }
    static bool isPossiblyValue (pool_ptr<Expression> e)         { return e != nullptr && isPossiblyValue (*e); }
    static bool isPossiblyEndpoint (pool_ptr<Expression> e)      { return e != nullptr && isPossiblyEndpoint (*e); }

    static bool isResolvedAsType (Expression& e)                 { return e.isResolved() && e.kind == ExpressionKind::type; }
    static bool isResolvedAsValue (Expression& e)                { return e.isResolved() && e.kind == ExpressionKind::value; }
    static bool isResolvedAsConstant (Expression& e)             { return isResolvedAsValue (e) && e.getAsConstant() != nullptr; }
    static bool isResolvedAsEndpoint (Expression& e)             { return e.isResolved() && e.isOutputEndpoint(); }
    static bool isResolvedAsProcessor (Expression& e)            { return e.isResolved() && e.kind == ExpressionKind::processor; }

    static bool isResolvedAsType (pool_ptr<Expression> e)        { return e != nullptr && isResolvedAsType (*e); }
    static bool isResolvedAsValue (pool_ptr<Expression> e)       { return e != nullptr && isResolvedAsValue (*e); }
    static bool isResolvedAsConstant (pool_ptr<Expression> e)    { return e != nullptr && isResolvedAsConstant (*e); }
    static bool isResolvedAsEndpoint (pool_ptr<Expression> e)    { return e != nullptr && isResolvedAsEndpoint (*e); }
    static bool isResolvedAsProcessor (pool_ptr<Expression> e)   { return e != nullptr && isResolvedAsProcessor (*e); }

    enum class Constness  { definitelyConst, notConst, unknown };

    //==============================================================================
    struct Allocator
    {
        Allocator() = default;
        Allocator (const Allocator&) = delete;
        Allocator (Allocator&&) = default;

        template <typename Type, typename... Args>
        Type& allocate (Args&&... args)   { return pool.allocate<Type> (std::forward<Args> (args)...); }

        template <typename Type>
        Identifier get (const Type& newString)  { return identifiers.get (newString); }

        void clear()
        {
            pool.clear();
            identifiers.clear();
        }

        PoolAllocator pool;
        Identifier::Pool identifiers;
        StringDictionary stringDictionary;
    };

    //==============================================================================
    /** Every ASTObject has a context, which consists of its parent scope and its original
        code location.
    */
    struct Context
    {
        CodeLocation location;
        Scope* parentScope;

        [[noreturn]] void throwError (CompileMessage message, bool isStaticAssertion = false) const
        {
            CompileMessageGroup messages;
            messages.messages.push_back (message.withLocation (location));

            for (auto p = parentScope; p != nullptr && messages.messages.size() < 10; p = p->getParentScope())
            {
                if (auto f = p->getAsFunction())
                {
                    if (f->originalCallLeadingToSpecialisation != nullptr)
                    {
                        CompileMessage error { "Failed to instantiate generic function "
                                                  + f->originalCallLeadingToSpecialisation->getDescription (f->originalGenericFunction->name),
                                               f->originalCallLeadingToSpecialisation->context.location,
                                               CompileMessage::Type::error };

                        if (location.sourceCode->isInternal)
                        {
                            messages.messages.clear();

                            if (isStaticAssertion)
                                error.description = message.description;
                            else
                                error.description += ", error: " + message.description;

                            messages.messages.push_back (error);
                        }
                        else
                        {
                            messages.messages.insert (messages.messages.begin(), error);
                        }

                        p = f->originalCallLeadingToSpecialisation->context.parentScope;
                    }
                }
            }

            soul::throwError (messages);
        }
    };

    //==============================================================================
    struct ASTObject
    {
        ASTObject (ObjectType ot, const Context& c) : objectType (ot), context (c) {}
        ASTObject (const ASTObject&) = delete;
        ASTObject (ASTObject&&) = delete;
        virtual ~ASTObject() {}

        Scope* getParentScope() const           { return context.parentScope; }

        const ObjectType objectType;
        Context context;
    };

    //==============================================================================
    struct Annotation
    {
        struct Property
        {
            pool_ref<QualifiedIdentifier> name;
            pool_ref<Expression> value;
        };

        std::vector<Property> properties;

        template <typename StringType>
        const Property* findProperty (const StringType& name) const
        {
            for (auto& p : properties)
                if (p.name->path == name)
                    return std::addressof (p);

            return {};
        }

        void addProperty (const Property& newProperty)
        {
            properties.push_back (newProperty);
        }

        void setProperty (const Property& newProperty)
        {
            for (auto& p : properties)
            {
                if (p.name->path == newProperty.name->path)
                {
                    p.value = newProperty.value;
                    return;
                }
            }

            addProperty (newProperty);
        }

        void setProperties (const Annotation& other)
        {
            for (auto& p : other.properties)
                setProperty (p);
        }

        soul::Annotation toPlainAnnotation (const StringDictionary& dictionary) const
        {
            soul::Annotation a;

            for (auto& p : properties)
            {
                if (auto constValue = p.value->getAsConstant())
                    a.set (p.name->path.toString(), constValue->value, dictionary);
                else
                    p.value->context.throwError (Errors::unresolvedAnnotation());
            }

            return a;
        }
    };

    //==============================================================================
    struct ImportsList
    {
        void addIfNotAlreadyThere (std::string newImport)
        {
            newImport = trim (newImport);

            if (! contains (imports, newImport))
                imports.push_back (newImport);
        }

        void mergeList (const ImportsList& other)
        {
            for (auto& i : other.imports)
                if (! contains (imports, i))
                    imports.push_back (i);
        }

        ArrayWithPreallocation<std::string, 4> imports;
    };

    //==============================================================================
    struct Scope
    {
        Scope() = default;
        virtual ~Scope() = default;

        virtual IdentifierPath getFullyQualifiedPath() const
        {
            SOUL_ASSERT_FALSE;
            return {};
        }

        virtual Scope* getParentScope() const = 0;
        virtual ModuleBase* getAsModule()               { return nullptr; }
        virtual ProcessorBase* getAsProcessor()         { return nullptr; }
        virtual Namespace* getAsNamespace()             { return nullptr; }
        virtual Function* getAsFunction()               { return nullptr; }
        virtual Block* getAsBlock()                     { return nullptr; }

        virtual pool_ptr<Function> getParentFunction() const
        {
            if (auto p = getParentScope())
                return p->getParentFunction();

            return {};
        }

        pool_ptr<ModuleBase> findModule()
        {
            for (auto p = this; p != nullptr; p = p->getParentScope())
                if (auto m = p->getAsModule())
                    return *m;

            return {};
        }

        pool_ptr<ProcessorBase> findProcessor()
        {
            for (auto s = this; s != nullptr; s = s->getParentScope())
                if (auto p = s->getAsProcessor())
                    return *p;

            return {};
        }

        virtual ArrayView<pool_ref<VariableDeclaration>>        getVariables() const            { return {}; }
        virtual ArrayView<pool_ref<Function>>                   getFunctions() const            { return {}; }
        virtual ArrayView<pool_ref<StructDeclaration>>          getStructDeclarations() const   { return {}; }
        virtual ArrayView<pool_ref<UsingDeclaration>>           getUsingDeclarations() const    { return {}; }
        virtual ArrayView<pool_ref<ModuleBase>>                 getSubModules() const           { return {}; }
        virtual ArrayView<pool_ref<ProcessorAliasDeclaration>>  getProcessorAliases() const     { return {}; }

        virtual const Statement* getAsStatement() const      { return nullptr; }

        struct NameSearch
        {
            ArrayWithPreallocation<pool_ref<ASTObject>, 8> itemsFound;
            IdentifierPath partiallyQualifiedPath;
            bool stopAtFirstScopeWithResults = false;
            int requiredNumFunctionArgs = -1;
            bool findVariables = true, findTypes = true, findFunctions = true,
                 findProcessorsAndNamespaces = true, findEndpoints = true,
                 onlyFindLocalVariables = false;

            void addResult (ASTObject& o)
            {
                if (! contains (itemsFound, o))
                    itemsFound.push_back (o);
            }

            template <typename ArrayType>
            void addFirstMatching (const ArrayType& array)
            {
                addFirstWithName (array, partiallyQualifiedPath.getLastPart());
            }

            template <typename ArrayType>
            void addFirstWithName (const ArrayType& array, Identifier targetName)
            {
                for (auto& o : array)
                {
                    if (o->name == targetName)
                    {
                        addResult (o);
                        break;
                    }
                }
            }
        };

        void performFullNameSearch (NameSearch& search, const Statement* statementToSearchUpTo) const
        {
            SOUL_ASSERT (! search.partiallyQualifiedPath.empty());
            auto parentPath = search.partiallyQualifiedPath.getParentPath();

            for (auto s = this; s != nullptr; s = s->getParentScope())
            {
                if (search.onlyFindLocalVariables && const_cast<Scope*>(s)->getAsBlock() == nullptr)
                    break;

                if (auto scopeToSearch = parentPath.empty() ? s : s->findChildScope (parentPath))
                    scopeToSearch->performLocalNameSearch (search, statementToSearchUpTo);

                if (search.stopAtFirstScopeWithResults && ! search.itemsFound.empty())
                    break;

                statementToSearchUpTo = s->getAsStatement();
            }
        }

        virtual void performLocalNameSearch (NameSearch& search, const Statement* statementToSearchUpTo) const = 0;

        pool_ptr<ModuleBase> findSubModuleNamed (Identifier name) const
        {
            for (auto& m : getSubModules())
                if (m->name == name)
                    return m;

            return {};
        }

        const Scope* findChildScope (const IdentifierPath& path) const
        {
            const Scope* s = this;

            for (auto& p : path.pathSections)
            {
                s = s->findSubModuleNamed (p).get();

                if (s == nullptr)
                    break;
            }

            return s;
        }

        std::vector<pool_ref<ModuleBase>> getMatchingSubModules (IdentifierPath partiallyQualifiedName) const
        {
            Scope::NameSearch search;
            search.partiallyQualifiedPath = partiallyQualifiedName;
            search.stopAtFirstScopeWithResults = false;
            search.findVariables = false;
            search.findTypes = false;
            search.findFunctions = false;
            search.findProcessorsAndNamespaces = true;
            search.findEndpoints = false;

            performFullNameSearch (search, nullptr);

            std::vector<pool_ref<ModuleBase>> found;

            for (auto& o : search.itemsFound)
                if (auto m = cast<ModuleBase> (o))
                    found.push_back (*m);

            return found;
        }

        ModuleBase& findSingleMatchingSubModule (const QualifiedIdentifier& name) const
        {
            auto modulesFound = getMatchingSubModules (name.path);

            if (modulesFound.empty())
                name.context.throwError (Errors::unresolvedSymbol (name.path));

            if (modulesFound.size() > 1)
                name.context.throwError (Errors::ambiguousSymbol (name.path));

            return modulesFound.front();
        }

        ProcessorBase& findSingleMatchingProcessor (const QualifiedIdentifier& name) const
        {
            auto p = cast<ProcessorBase> (findSingleMatchingSubModule (name));

            if (p == nullptr)
                name.context.throwError (Errors::notAProcessorOrGraph (name.path));

            return *p;
        }

        ProcessorBase& findSingleMatchingProcessor (const ProcessorInstance& i) const
        {
            SOUL_ASSERT (i.targetProcessor != nullptr);
            auto p = i.targetProcessor->getAsProcessor();

            if (p == nullptr)
                if (auto name = cast<QualifiedIdentifier> (i.targetProcessor))
                    return findSingleMatchingProcessor (*name);

            return *p;
        }

        std::string makeUniqueName (const std::string& root) const
        {
            return addSuffixToMakeUnique (root,
                                          [this] (const std::string& name)
                                          {
                                              for (auto& f : getFunctions())
                                                  if (f->name == name)
                                                      return true;

                                              for (auto& s : getStructDeclarations())
                                                  if (s->name == name)
                                                      return true;

                                              for (auto& u : getUsingDeclarations())
                                                  if (u->name == name)
                                                      return true;

                                              for (auto& s : getSubModules())
                                                  if (s->name == name)
                                                      return true;

                                              for (auto& a : getProcessorAliases())
                                                  if (a->name == name)
                                                      return true;

                                              return false;
                                          });
        }
    };

    //==============================================================================
    struct ModuleBase   : public ASTObject,
                          public Scope
    {
        ModuleBase (ObjectType ot, const Context& c, Identifier moduleName)
            : ASTObject (ot, c), name (moduleName) {}

        Scope* getParentScope() const override      { return ASTObject::getParentScope(); }
        ModuleBase* getAsModule() override          { return this; }

        virtual bool isProcessor() const            { return false; }
        virtual bool isGraph() const                { return false; }
        virtual bool isNamespace() const            { return false; }

        virtual ArrayView<pool_ref<ASTObject>>                 getSpecialisationParameters() const  { return {}; }
        virtual ArrayView<pool_ref<EndpointDeclaration>>       getEndpoints() const                 { return {}; }

        virtual std::vector<pool_ref<StructDeclaration>>*      getStructList() = 0;
        virtual std::vector<pool_ref<UsingDeclaration>>*       getUsingList() = 0;
        virtual std::vector<pool_ref<VariableDeclaration>>*    getStateVariableList() = 0;
        virtual std::vector<pool_ref<Function>>*               getFunctionList() = 0;

        size_t getNumInputs() const                 { return countEndpoints (true); }
        size_t getNumOutputs() const                { return countEndpoints (false); }

        IdentifierPath getFullyQualifiedPath() const override
        {
            if (auto p = getParentScope())
                return IdentifierPath (p->getFullyQualifiedPath(), name);

            return IdentifierPath (name);
        }

        void performLocalNameSearch (NameSearch& search, const Statement*) const override
        {
            auto targetName = search.partiallyQualifiedPath.getLastPart();

            if (search.findVariables)
                search.addFirstWithName (getVariables(), targetName);

            if (search.findTypes)
            {
                search.addFirstWithName (getStructDeclarations(), targetName);
                search.addFirstWithName (getUsingDeclarations(), targetName);
            }

            if (search.findFunctions)
            {
                for (auto& f : getFunctions())
                {
                    if (f->name == targetName
                         && (search.requiredNumFunctionArgs < 0
                              || f->parameters.size() == static_cast<uint32_t> (search.requiredNumFunctionArgs)))
                    {
                        search.addResult (f);
                    }
                }
            }

            if (search.findEndpoints)
                search.addFirstWithName (getEndpoints(), targetName);

            if (search.findProcessorsAndNamespaces)
            {
                search.addFirstWithName (getSubModules(), targetName);
                search.addFirstWithName (getProcessorAliases(), targetName);
            }
        }

        //==============================================================================
        Identifier name;
        bool isFullyResolved = false;

    private:
        size_t countEndpoints (bool countInputs) const
        {
            size_t num = 0;

            for (auto& e : getEndpoints())
                if (e->isInput == countInputs)
                    ++num;

            return num;
        }
    };

    //==============================================================================
    struct ProcessorBase  : public ModuleBase
    {
        ProcessorBase (ObjectType ot, const Context& c, Identifier moduleName) : ModuleBase (ot, c, moduleName)
        {
            SOUL_ASSERT (getParentScope() != nullptr);
        }

        Namespace& getNamespace() const
        {
            auto processorNamespace = getParentScope()->getAsNamespace();
            SOUL_ASSERT (processorNamespace != nullptr);
            return *processorNamespace;
        }

        ProcessorBase* getAsProcessor() override        { return this; }

        ArrayView<pool_ref<EndpointDeclaration>>  getEndpoints() const override                   { return endpoints; }
        ArrayView<pool_ref<ASTObject>>            getSpecialisationParameters() const override    { return specialisationParams; }

        template <typename StringType>
        pool_ptr<EndpointDeclaration> findEndpoint (const StringType& nameToFind, bool isInput) const
        {
            for (auto& e : endpoints)
                if (e->isInput == isInput && e->name == nameToFind)
                    return e;

            return {};
        }

        template <typename StringType>
        pool_ptr<EndpointDeclaration> findEndpoint (const StringType& nameToFind) const
        {
            for (auto& e : endpoints)
                if (e->name == nameToFind)
                    return e;

            return {};
        }

        virtual void addSpecialisationParameter (VariableDeclaration&) = 0;
        virtual void addSpecialisationParameter (UsingDeclaration&) = 0;
        virtual void addSpecialisationParameter (ProcessorAliasDeclaration&) = 0;

        std::vector<pool_ref<EndpointDeclaration>> endpoints;
        std::vector<pool_ref<ASTObject>> specialisationParams;

        Annotation annotation;
    };

    //==============================================================================
    struct Processor   : public ProcessorBase
    {
        Processor (const Context& c, Identifier moduleName) : ProcessorBase (ObjectType::Processor, c, moduleName) {}

        pool_ptr<Function> getRunFunction() const
        {
            for (auto& f : functions)
                if (f->isRunFunction())
                    return f;

            return {};
        }

        bool isProcessor() const override   { return true; }

        ArrayView<pool_ref<VariableDeclaration>>     getVariables() const override           { return stateVariables; }
        ArrayView<pool_ref<Function>>                getFunctions() const override           { return functions; }
        ArrayView<pool_ref<StructDeclaration>>       getStructDeclarations() const override  { return structures; }
        ArrayView<pool_ref<UsingDeclaration>>        getUsingDeclarations() const override   { return usings; }

        std::vector<pool_ref<StructDeclaration>>*    getStructList() override                { return &structures; }
        std::vector<pool_ref<UsingDeclaration>>*     getUsingList() override                 { return &usings; }
        std::vector<pool_ref<VariableDeclaration>>*  getStateVariableList() override         { return &stateVariables; }
        std::vector<pool_ref<Function>>*             getFunctionList() override              { return &functions; }

        void addSpecialisationParameter (VariableDeclaration& v) override
        {
            SOUL_ASSERT (v.isConstant);
            stateVariables.push_back (v);
            specialisationParams.push_back (v);
        }

        void addSpecialisationParameter (UsingDeclaration& u) override
        {
            usings.push_back (u);
            specialisationParams.push_back (u);
        }

        void addSpecialisationParameter (ProcessorAliasDeclaration&) override
        {
            SOUL_ASSERT_FALSE;
        }

        std::vector<pool_ref<StructDeclaration>> structures;
        std::vector<pool_ref<UsingDeclaration>> usings;
        std::vector<pool_ref<Function>> functions;
        std::vector<pool_ref<VariableDeclaration>> stateVariables;
    };

    //==============================================================================
    struct Graph   : public ProcessorBase
    {
        Graph (const Context& c, Identifier moduleName) : ProcessorBase (ObjectType::Graph, c, moduleName) {}

        void addSpecialisationParameter (VariableDeclaration& v) override
        {
            constants.push_back (v);
            specialisationParams.push_back (v);
        }

        void addSpecialisationParameter (ProcessorAliasDeclaration& alias) override
        {
            processorAliases.push_back (alias);
            specialisationParams.push_back (alias);
        }

        void addSpecialisationParameter (UsingDeclaration&) override
        {
            SOUL_ASSERT_FALSE;
        }

        bool isGraph() const override                                                        { return true; }

        ArrayView<pool_ref<ProcessorAliasDeclaration>> getProcessorAliases() const override  { return processorAliases; }
        ArrayView<pool_ref<VariableDeclaration>> getVariables() const override               { return constants; }

        std::vector<pool_ref<StructDeclaration>>* getStructList() override                   { return {}; }
        std::vector<pool_ref<UsingDeclaration>>* getUsingList() override                     { return {}; }
        std::vector<pool_ref<VariableDeclaration>>* getStateVariableList() override          { return {}; }
        std::vector<pool_ref<Function>>* getFunctionList() override                          { return {}; }

        std::vector<pool_ref<ProcessorInstance>> processorInstances;
        std::vector<pool_ref<Connection>> connections;
        std::vector<pool_ref<VariableDeclaration>> constants;
        std::vector<pool_ref<ProcessorAliasDeclaration>> processorAliases;

        template <typename StringType>
        pool_ptr<ProcessorInstance> findChildProcessor (const StringType& processorInstanceName) const
        {
            for (auto& i : processorInstances)
                if (i->instanceName->path == processorInstanceName)
                    return i;

            return {};
        }

        //==============================================================================
        struct RecursiveGraphDetector
        {
            static void check (Graph& g, const RecursiveGraphDetector* stack = nullptr)
            {
                for (auto s = stack; s != nullptr; s = s->previous)
                    if (s->graph == std::addressof (g))
                        g.context.throwError (Errors::recursiveTypes (g.getFullyQualifiedPath()));

                const RecursiveGraphDetector newStack { stack, std::addressof (g) };

                for (auto& p : g.processorInstances)
                {
                    // avoid using findSingleMatchingSubModule() as we don't want an error thrown if
                    // a processor specialisation alias has not yet been resolved

                    pool_ptr<Graph> sub;

                    if (auto pr = cast<ProcessorRef> (p->targetProcessor))
                    {
                        sub = cast<Graph> (pr->processor);
                    }
                    else if (auto name = cast<QualifiedIdentifier> (p->targetProcessor))
                    {
                        auto modulesFound = g.getMatchingSubModules (name->path);

                        if (modulesFound.size() == 1)
                            sub = cast<Graph> (modulesFound.front());
                    }

                    if (sub != nullptr)
                        return check (*sub, std::addressof (newStack));
                }
            }

            const RecursiveGraphDetector* previous = nullptr;
            const Graph* graph = nullptr;
        };

        //==============================================================================
        struct CycleDetector
        {
            CycleDetector (Graph& g)
            {
                for (auto& n : g.processorInstances)
                    nodes.push_back ({ n });

                for (auto& c : g.connections)
                    if (c->delayLength == nullptr)
                        if (auto src = findNode (*c->source.processorName))
                            if (auto dst = findNode (*c->dest.processorName))
                                dst->sources.push_back ({ src, c });
            }

            void check()
            {
                for (auto& n : nodes)
                    check (n, nullptr, {});
            }

        private:
            struct Node
            {
                struct Source
                {
                    Node* node;
                    pool_ref<Connection> connection;
                };

                pool_ref<ProcessorInstance> processor;
                ArrayWithPreallocation<Source, 4> sources;
            };

            std::vector<Node> nodes;

            Node* findNode (const QualifiedIdentifier& nodeName)
            {
                if (nodeName.path.empty())
                    return {};

                for (auto& n : nodes)
                    if (nodeName == *n.processor->instanceName)
                        return std::addressof (n);

                nodeName.context.throwError (Errors::cannotFindProcessor (nodeName.path));
                return {};
            }

            struct VisitedStack
            {
                const VisitedStack* previous = nullptr;
                const Node* node = nullptr;
            };

            void check (Node& node, const VisitedStack* stack, const Context& errorContext)
            {
                for (auto s = stack; s != nullptr; s = s->previous)
                    if (s->node == std::addressof (node))
                        throwCycleError (stack, errorContext);

                const VisitedStack newStack { stack, std::addressof (node) };

                for (auto& source : node.sources)
                    check (*source.node, std::addressof (newStack), source.connection->context);
            }

            void throwCycleError (const VisitedStack* stack, const Context& errorContext)
            {
                std::vector<std::string> nodesInCycle;

                for (auto node = stack; node != nullptr; node = node->previous)
                    nodesInCycle.push_back (node->node->processor->instanceName->path.toString());

                nodesInCycle.push_back (nodesInCycle.front());

                errorContext.throwError (Errors::feedbackInGraph (joinStrings (nodesInCycle, " -> ")));
            }
        };
    };

    //==============================================================================
    struct Namespace  : public ModuleBase
    {
        Namespace (const Context& c, Identifier moduleName) : ModuleBase (ObjectType::Namespace, c, moduleName) {}

        bool isNamespace() const override                                              { return true; }
        Namespace* getAsNamespace() override                                           { return this; }

        ArrayView<pool_ref<VariableDeclaration>> getVariables() const override         { return constants; }
        ArrayView<pool_ref<Function>> getFunctions() const override                    { return functions; }
        ArrayView<pool_ref<StructDeclaration>> getStructDeclarations() const override  { return structures; }
        ArrayView<pool_ref<UsingDeclaration>> getUsingDeclarations() const override    { return usings; }
        ArrayView<pool_ref<ModuleBase>> getSubModules() const override                 { return subModules; }

        std::vector<pool_ref<Function>>* getFunctionList() override                    { return &functions; }
        std::vector<pool_ref<StructDeclaration>>* getStructList() override             { return &structures; }
        std::vector<pool_ref<UsingDeclaration>>* getUsingList() override               { return &usings; }
        std::vector<pool_ref<VariableDeclaration>>* getStateVariableList() override    { return &constants; }

        ImportsList importsList;
        std::vector<pool_ref<Function>> functions;
        std::vector<pool_ref<StructDeclaration>> structures;
        std::vector<pool_ref<UsingDeclaration>> usings;
        std::vector<pool_ref<ModuleBase>> subModules;
        std::vector<pool_ref<VariableDeclaration>> constants;
    };

    //==============================================================================
    struct Statement  : public ASTObject
    {
        Statement (ObjectType ot, const Context& c) : ASTObject (ot, c) {}

        virtual const Statement* getAsStatement() const  { return this; }

        pool_ptr<Function> getParentFunction() const
        {
            if (auto pn = getParentScope())
                return pn->getParentFunction();

            SOUL_ASSERT_FALSE;
            return {};
        }
    };

    //==============================================================================
    struct Expression  : public Statement
    {
        Expression (ObjectType ot, const Context& c, ExpressionKind k)  : Statement (ot, c), kind (k) {}

        virtual bool isResolved() const = 0;

        virtual Type getResultType() const                     { SOUL_ASSERT_FALSE; return {}; }
        virtual Type resolveAsType() const                     { SOUL_ASSERT_FALSE; return {}; }
        virtual pool_ptr<ProcessorBase> getAsProcessor() const { return {}; }
        virtual bool isOutputEndpoint() const                  { return false; }
        virtual Constness getConstness() const                 { return Constness::unknown; }
        virtual const Type* getConcreteType() const            { return {}; }
        virtual pool_ptr<StructDeclaration> getAsStruct()      { return {}; }
        virtual pool_ptr<Constant> getAsConstant()             { return {}; }
        virtual bool isCompileTimeConstant() const             { return false; }
        virtual bool isAssignable() const                      { return false; }

        virtual bool canSilentlyCastTo (const Type& targetType) const
        {
            return ! isOutputEndpoint() && TypeRules::canSilentlyCastTo (targetType, getResultType());
        }

        ExpressionKind kind;
    };

    //==============================================================================
    struct EndpointDetails
    {
        EndpointDetails (EndpointKind k) : kind (k) {}
        EndpointDetails (const EndpointDetails&) = default;

        const EndpointKind kind;
        std::vector<pool_ref<Expression>> dataTypes;
        pool_ptr<Expression> arraySize;

        void checkDataTypesValid (const Context& context)
        {
            if (isStream (kind))
            {
                SOUL_ASSERT (dataTypes.size() == 1);
                auto dataType = getResolvedDataTypes().front();

                if (! (dataType.isPrimitive() || dataType.isVector()))
                    context.throwError (Errors::illegalTypeForEndpoint());
            }

            // Ensure all of the types are unique
            {
                std::vector<Type> processedTypes;

                for (auto& dataType : getResolvedDataTypes())
                {
                    for (auto& processedType : processedTypes)
                        if (processedType.isEqual (dataType, Type::ignoreVectorSize1))
                            context.throwError (Errors::duplicateTypesInList (processedType.getDescription(),
                                                                              dataType.getDescription()));

                    processedTypes.push_back (dataType);
                }
            }
        }

        bool isResolved() const
        {
            for (auto& t : dataTypes)
                if (! isResolvedAsType (t.get()))
                    return false;

            return arraySize == nullptr || isResolvedAsConstant (arraySize);
        }

        std::vector<Type> getResolvedDataTypes() const
        {
            std::vector<Type> types;
            types.reserve (dataTypes.size());

            for (auto& t : dataTypes)
            {
                SOUL_ASSERT (isResolvedAsType (t.get()));
                types.push_back (t->resolveAsType());
            }

            return types;
        }

        int getArraySize() const
        {
            SOUL_ASSERT (isResolvedAsConstant (arraySize));
            return arraySize->getAsConstant()->value.getAsInt32();
        }

        std::vector<Type> getSampleArrayTypes() const
        {
            std::vector<Type> types;
            types.reserve (dataTypes.size());

            auto size = static_cast<uint32_t> (arraySize != nullptr ? getArraySize() : 0);

            for (auto& t : getResolvedDataTypes())
                types.push_back (size == 0 ? t : t.createArray (size));

            return types;
        }

        std::string getTypesDescription() const
        {
            return heart::Utilities::getDescriptionOfTypeList (getResolvedDataTypes(), false);
        }

        bool supportsDataType (Expression& e) const
        {
            for (auto& type : getSampleArrayTypes())
                if (e.canSilentlyCastTo (type))
                    return true;

            return false;
        }

        Type getDataType (Expression& e) const
        {
            for (auto& type : getSampleArrayTypes())
                if (e.getResultType().isEqual (type, Type::ignoreVectorSize1))
                    return type;

            for (auto& type : getSampleArrayTypes())
                if (e.canSilentlyCastTo (type))
                    return type;

            SOUL_ASSERT_FALSE;
            return {};
        }

        Type getElementDataType (Expression& e) const
        {
            for (auto& type : getResolvedDataTypes())
                if (e.canSilentlyCastTo (type))
                    return type;

            SOUL_ASSERT_FALSE;
            return {};
        }
    };

    struct ChildEndpointPath
    {
        struct PathSection
        {
            pool_ptr<QualifiedIdentifier> name;
            pool_ptr<Expression> index;
            bool isWildcard = false;
        };

        ArrayWithPreallocation<PathSection, 4> sections;
    };

    struct EndpointDeclaration  : public ASTObject
    {
        EndpointDeclaration (const Context& c, bool isInputEndpoint)
            : ASTObject (ObjectType::EndpointDeclaration, c), isInput (isInputEndpoint) {}

        bool isResolved() const         { return details != nullptr && details->isResolved(); }

        const bool isInput;
        Identifier name;
        std::unique_ptr<EndpointDetails> details;
        std::unique_ptr<ChildEndpointPath> childPath;
        Annotation annotation;
        bool needsToBeExposedInParent = false;

        pool_ptr<heart::InputDeclaration> generatedInput;
        pool_ptr<heart::OutputDeclaration> generatedOutput;
    };

    //==============================================================================
    struct InputEndpointRef  : public Expression
    {
        InputEndpointRef (const Context& c, EndpointDeclaration& i)
            : Expression (ObjectType::InputEndpointRef, c, ExpressionKind::value), input (i)
        {
        }

        bool isResolved() const override            { return input->isResolved(); }

        Type getResultType() const override
        {
            auto& details = *input->details;

            if (isEvent (details.kind))
                return (details.arraySize == nullptr) ? Type() : Type().createArray (static_cast<uint32_t> (details.getArraySize()));

            SOUL_ASSERT (details.dataTypes.size() == 1);
            return details.getSampleArrayTypes().front();
        }

        pool_ref<EndpointDeclaration> input;
    };

    struct OutputEndpointRef  : public Expression
    {
        OutputEndpointRef (const Context& c, EndpointDeclaration& o)
            : Expression (ObjectType::OutputEndpointRef, c, ExpressionKind::endpoint), output (o)
        {
        }

        bool isOutputEndpoint() const override      { return true; }
        bool isResolved() const override            { return output->isResolved(); }

        pool_ref<EndpointDeclaration> output;
    };

    //==============================================================================
    struct Connection  : public ASTObject
    {
        struct NameAndEndpoint
        {
            pool_ptr<QualifiedIdentifier>  processorName;
            pool_ptr<Expression>           processorIndex;
            Identifier                     endpoint;
            pool_ptr<Expression>           endpointIndex;
        };

        Connection (const Context& c, InterpolationType interpolation,
                    NameAndEndpoint src, NameAndEndpoint dst, pool_ptr<Expression> delay)
            : ASTObject (ObjectType::Connection, c), interpolationType (interpolation),
              source (src), dest (dst), delayLength (delay) {}

        InterpolationType interpolationType;
        NameAndEndpoint source, dest;
        pool_ptr<Expression> delayLength;
    };

    struct ProcessorInstance  : public ASTObject
    {
        ProcessorInstance (const Context& c) : ASTObject (ObjectType::ProcessorInstance, c) {}

        pool_ptr<QualifiedIdentifier> instanceName;
        pool_ptr<Expression> targetProcessor;
        std::vector<pool_ref<Expression>> specialisationArgs;
        pool_ptr<Expression> clockMultiplierRatio, clockDividerRatio, arraySize;
        bool wasCreatedImplicitly = false;
    };

    //==============================================================================
    struct Function  : public ASTObject,
                       public Scope

    {
        Function (const Context& c) : ASTObject (ObjectType::Function, c) {}

        pool_ptr<Expression> returnType;
        Identifier name;
        Context nameLocation;
        std::vector<pool_ref<VariableDeclaration>> parameters;
        std::vector<pool_ref<QualifiedIdentifier>> genericWildcards;
        std::vector<pool_ref<UsingDeclaration>> genericSpecialisations;
        pool_ptr<Function> originalGenericFunction;
        pool_ptr<FunctionCall> originalCallLeadingToSpecialisation;
        Annotation annotation;
        IntrinsicType intrinsic = IntrinsicType::none;
        bool eventFunction = false;

        pool_ptr<Block> block;
        pool_ptr<heart::Function> generatedFunction;

        Function* getAsFunction() override  { return this; }

        bool isEventFunction() const        { return eventFunction; }
        bool isRunFunction() const          { return name == heart::getRunFunctionName(); }
        bool isUserInitFunction() const     { return name == heart::getUserInitFunctionName(); }
        bool isSystemInitFunction() const   { return name == heart::getSystemInitFunctionName(); }
        bool isGeneric() const              { return ! genericWildcards.empty(); }
        bool isIntrinsic() const            { return intrinsic != IntrinsicType::none; }

        heart::Function& getGeneratedFunction() const
        {
            SOUL_ASSERT (generatedFunction != nullptr);
            return *generatedFunction;
        }

        std::string getDescription() const
        {
            return name.toString() + heart::Utilities::getDescriptionOfTypeList (getParameterTypes(), true);
        }

        std::string getSignatureID() const
        {
            auto args = "_" + std::to_string (parameters.size());

            for (auto& p : parameters)
                args += "_" + p->getType().withConstAndRefFlags (false, false).getShortIdentifierDescription();

            return name.toString() + args;
        }

        TypeArray getParameterTypes() const
        {
            TypeArray types;
            types.reserve (parameters.size());

            for (auto& param : parameters)
                types.push_back (param->getType());

            return types;
        }

        Scope* getParentScope() const override                                       { return ASTObject::getParentScope(); }
        ArrayView<pool_ref<UsingDeclaration>> getUsingDeclarations() const override  { return genericSpecialisations; }

        void performLocalNameSearch (NameSearch& search, const Statement*) const override
        {
            if (search.findVariables)
                search.addFirstMatching (parameters);

            if (search.findTypes)
                search.addFirstMatching (genericSpecialisations);
        }
    };

    //==============================================================================
    struct ConcreteType  : public Expression
    {
        ConcreteType (const Context& c, Type t)
           : Expression (ObjectType::ConcreteType, c, ExpressionKind::type), type (std::move (t))
        {
        }

        bool isResolved() const override                { return true; }
        Type resolveAsType() const override             { return type; }
        const Type* getConcreteType() const override    { return &type; }
        Constness getConstness() const override         { return type.isConst() ? Constness::definitelyConst : Constness::notConst; }
        bool isCompileTimeConstant() const override     { return true; }

        Type type;
    };

    struct TypeDeclarationBase  : public Expression
    {
        TypeDeclarationBase (ObjectType ot, const Context& c, Identifier typeName)
            : Expression (ot, c, ExpressionKind::type), name (typeName)
        {}

        Identifier name;
    };

    struct StructDeclaration  : public TypeDeclarationBase
    {
        StructDeclaration (const Context& c, Identifier structName)
            : TypeDeclarationBase (ObjectType::StructDeclaration, c, structName)
        {
        }

        ~StructDeclaration() override
        {
            if (structure != nullptr)
                structure->backlinkToASTObject = nullptr;
        }

        struct Member
        {
            pool_ref<Expression> type;
            Identifier name;
        };

        ArrayView<Member> getMembers() const     { return members; }

        void addMember (Expression& type, Identifier memberName)
        {
            SOUL_ASSERT (structure == nullptr);
            members.push_back ({ type, memberName });
        }

        bool isResolved() const override
        {
            for (auto& m : members)
                if (! isResolvedAsType (m.type.get()))
                    return false;

            return true;
        }

        pool_ptr<StructDeclaration> getAsStruct() override  { return *this; }
        Constness getConstness() const override             { return Constness::notConst; }
        Type resolveAsType() const override                 { return Type::createStruct (getStruct()); }

        Structure& getStruct() const
        {
            if (structure == nullptr)
            {
                structure.reset (new Structure (name.toString(), const_cast<StructDeclaration*> (this)));

                for (auto& m : members)
                    structure->addMember(m.type->resolveAsType(), m.name.toString());
            }

            return *structure;
        }

    private:
        mutable StructurePtr structure;
        ArrayWithPreallocation<Member, 16> members;
    };

    struct UsingDeclaration  : public TypeDeclarationBase
    {
        UsingDeclaration (const Context& c, Identifier usingName, pool_ptr<Expression> target)
            : TypeDeclarationBase (ObjectType::UsingDeclaration, c, usingName), targetType (target)
        {
            SOUL_ASSERT (targetType == nullptr || isPossiblyType (targetType));
        }

        pool_ptr<StructDeclaration> getAsStruct() override  { return targetType->getAsStruct(); }
        bool isResolved() const override                    { return targetType != nullptr && targetType->isResolved(); }
        Type resolveAsType() const override                 { return targetType->resolveAsType(); }
        Constness getConstness() const override             { return targetType == nullptr ? Constness::unknown : targetType->getConstness(); }

        pool_ptr<Expression> targetType;
    };

    //==============================================================================
    struct ProcessorAliasDeclaration  : public ASTObject
    {
        ProcessorAliasDeclaration (const Context& c, Identifier nm)
            : ASTObject (ObjectType::ProcessorAliasDeclaration, c), name (nm)
        {
        }

        Identifier name;
        pool_ptr<ProcessorBase> targetProcessor;
    };

    struct ProcessorRef   : public Expression
    {
        ProcessorRef (const Context& c, ProcessorBase& p)
           : Expression (ObjectType::ProcessorRef, c, ExpressionKind::processor), processor (p)
        {
        }

        bool isResolved() const override                           { return true; }
        bool isCompileTimeConstant() const override                { return true; }
        pool_ptr<ProcessorBase> getAsProcessor() const override    { return processor; }

        pool_ref<ProcessorBase> processor;
    };

    //==============================================================================
    struct Block  : public Statement,
                    public Scope
    {
        Block (const Context& c, pool_ptr<Function> f)
            : Statement (ObjectType::Block, c), functionForWhichThisIsMain (f)
        {
        }

        void performLocalNameSearch (NameSearch& search, const Statement* statementToSearchUpTo) const override
        {
            if (search.findVariables)
            {
                auto name = search.partiallyQualifiedPath.getLastPart();
                pool_ptr<VariableDeclaration> lastMatch;

                for (auto& s : statements)
                {
                    if (s.getPointer() == statementToSearchUpTo)
                        break;

                    if (auto v = cast<VariableDeclaration> (s))
                        if (v->name == name)
                            lastMatch = v;
                }

                if (lastMatch != nullptr)
                    search.addResult (*lastMatch);
            }
        }

        pool_ptr<Function> getParentFunction() const override
        {
            if (isFunctionMainBlock())
                return functionForWhichThisIsMain;

            return Scope::getParentFunction();
        }

        Block* getAsBlock() override              { return this; }
        Scope* getParentScope() const override    { return ASTObject::getParentScope(); }
        bool isFunctionMainBlock() const          { return functionForWhichThisIsMain != nullptr; }
        void addStatement (Statement& s)          { statements.push_back (s); }

        pool_ptr<Function> functionForWhichThisIsMain;
        std::vector<pool_ref<Statement>> statements;
    };

    //==============================================================================
    struct NoopStatement  : public Statement
    {
        NoopStatement (const Context& c)  : Statement (ObjectType::NoopStatement, c) {}
    };

    //==============================================================================
    struct LoopStatement  : public Statement
    {
        LoopStatement (const Context& c, bool isDo)  : Statement (ObjectType::LoopStatement, c), isDoLoop (isDo) {}

        pool_ptr<Statement> iterator, body;
        pool_ptr<Expression> condition, numIterations;
        const bool isDoLoop;
    };

    //==============================================================================
    struct ReturnStatement  : public Statement
    {
        ReturnStatement (const Context& c)  : Statement (ObjectType::ReturnStatement, c) {}

        pool_ptr<Expression> returnValue;
    };

    //==============================================================================
    struct BreakStatement  : public Statement
    {
        BreakStatement (const Context& c)  : Statement (ObjectType::BreakStatement, c) {}
    };

    //==============================================================================
    struct ContinueStatement  : public Statement
    {
        ContinueStatement (const Context& c)  : Statement (ObjectType::ContinueStatement, c) {}
    };

    //==============================================================================
    struct IfStatement  : public Statement
    {
        IfStatement (const Context& c, bool isConst, Expression& cond, Statement& t, pool_ptr<Statement> f)
            : Statement (ObjectType::IfStatement, c),
              condition (cond), trueBranch (t), falseBranch (f), isConstIf (isConst) {}

        pool_ref<Expression> condition;
        pool_ref<Statement> trueBranch;
        pool_ptr<Statement> falseBranch;
        const bool isConstIf;
    };

    //==============================================================================
    struct TernaryOp  : public Expression
    {
        TernaryOp (const Context& c, Expression& cond, Expression& trueValue, Expression& falseValue)
            : Expression (ObjectType::TernaryOp, c, ExpressionKind::value),
              condition (cond), trueBranch (trueValue), falseBranch (falseValue) {}

        bool isResolved() const override
        {
            return condition->isResolved()
                    && trueBranch->isResolved()
                    && falseBranch->isResolved();
        }

        bool isCompileTimeConstant() const override
        {
            return condition->isCompileTimeConstant()
                      && trueBranch->isCompileTimeConstant()
                      && falseBranch->isCompileTimeConstant();
        }

        Type getResultType() const override         { return trueBranch->getResultType(); }

        pool_ref<Expression> condition, trueBranch, falseBranch;
    };

    //==============================================================================
    struct Constant  : public Expression
    {
        Constant (const Context& c, Value v)
            : Expression (ObjectType::Constant, c, ExpressionKind::value), value (std::move (v))
        {
            SOUL_ASSERT (value.isValid());
        }

        bool isResolved() const override                    { return true; }
        Type getResultType() const override                 { return value.getType(); }
        pool_ptr<Constant> getAsConstant() override         { return *this; }
        bool isCompileTimeConstant() const override         { return true; }

        bool canSilentlyCastTo (const Type& targetType) const override
        {
            return TypeRules::canSilentlyCastTo (targetType, value);
        }

        Value value;
    };

    //==============================================================================
    struct QualifiedIdentifier  : public Expression
    {
        QualifiedIdentifier (const Context& c, IdentifierPath p)
            : Expression (ObjectType::QualifiedIdentifier, c, ExpressionKind::unknown), path (std::move (p)) {}

        bool isResolved() const override            { return false; }
        std::string toString() const                { return path.toString(); }

        bool operator== (const QualifiedIdentifier& other) const     { return path == other.path; }
        bool operator!= (const QualifiedIdentifier& other) const     { return path != other.path; }

        IdentifierPath path;
    };

    struct SubscriptWithBrackets  : public Expression
    {
        SubscriptWithBrackets (const Context& c, Expression& objectOrType, pool_ptr<Expression> optionalSize)
            : Expression (ObjectType::SubscriptWithBrackets, c, ExpressionKind::unknown), lhs (objectOrType), rhs (optionalSize) {}

        bool isResolved() const override         { return false; }
        Constness getConstness() const override  { return lhs->getConstness(); }

        pool_ref<Expression> lhs;
        pool_ptr<Expression> rhs;
    };

    struct SubscriptWithChevrons  : public Expression
    {
        SubscriptWithChevrons (const Context& c, Expression& type, Expression& size)
            : Expression (ObjectType::SubscriptWithChevrons, c, ExpressionKind::unknown), lhs (type), rhs (size) {}

        bool isResolved() const override            { return false; }

        pool_ref<Expression> lhs;
        pool_ptr<Expression> rhs;
    };

    struct TypeMetaFunction  : public Expression
    {
        enum class Op
        {
            none,
            makeConst,
            makeConstSilent,
            makeReference,
            removeReference,
            elementType,
            primitiveType,
            size,
            isStruct,
            isArray,
            isDynamicArray,
            isFixedSizeArray,
            isVector,
            isPrimitive,
            isFloat,
            isFloat32,
            isFloat64,
            isInt,
            isInt32,
            isInt64,
            isScalar,
            isString,
            isBool,
            isReference,
            isConst
        };

        TypeMetaFunction (const Context& c, Expression& type, Op op)
            : Expression (ObjectType::TypeMetaFunction, c,
                          operationReturnsAType (op) ? ExpressionKind::type : ExpressionKind::value),
                          source (type), operation (op)
        {
            SOUL_ASSERT (isPossiblyType (type));
        }

        static constexpr bool operationReturnsAType (Op op)
        {
            return op == Op::makeConst || op == Op::makeConstSilent || op == Op::makeReference
                    || op == Op::removeReference || op == Op::elementType || op == Op::primitiveType;
        }

        static Op getOperationForName (Identifier name)
        {
            if (name == "elementType")      return Op::elementType;
            if (name == "primitiveType")    return Op::primitiveType;
            if (name == "size")             return Op::size;
            if (name == "removeReference")  return Op::removeReference;
            if (name == "isStruct")         return Op::isStruct;
            if (name == "isArray")          return Op::isArray;
            if (name == "isDynamicArray")   return Op::isDynamicArray;
            if (name == "isFixedSizeArray") return Op::isFixedSizeArray;
            if (name == "isVector")         return Op::isVector;
            if (name == "isPrimitive")      return Op::isPrimitive;
            if (name == "isFloat")          return Op::isFloat;
            if (name == "isFloat32")        return Op::isFloat32;
            if (name == "isFloat64")        return Op::isFloat64;
            if (name == "isInt")            return Op::isInt;
            if (name == "isInt32")          return Op::isInt32;
            if (name == "isInt64")          return Op::isInt64;
            if (name == "isScalar")         return Op::isScalar;
            if (name == "isString")         return Op::isString;
            if (name == "isBool")           return Op::isBool;
            if (name == "isReference")      return Op::isReference;
            if (name == "isConst")          return Op::isConst;

            return Op::none;
        }

        static Value performOp (Op op, const Type& sourceType)
        {
            if (op == Op::size)
                return Value::createInt64 (sourceType.isBoundedInt() ? (uint64_t) sourceType.getBoundedIntLimit()
                                                                     : (uint64_t) sourceType.getArrayOrVectorSize());

            return Value (performBoolOp (op, sourceType));
        }

        static bool performBoolOp (Op op, const Type& inputType)
        {
            switch (op)
            {
                case Op::isStruct:          return inputType.isStruct();
                case Op::isArray:           return inputType.isArray();
                case Op::isDynamicArray:    return inputType.isUnsizedArray();
                case Op::isFixedSizeArray:  return inputType.isFixedSizeArray();
                case Op::isVector:          return inputType.isVector();
                case Op::isPrimitive:       return inputType.isPrimitive();
                case Op::isFloat:           return inputType.isFloatingPoint();
                case Op::isFloat32:         return inputType.isFloat32();
                case Op::isFloat64:         return inputType.isFloat64();
                case Op::isInt:             return inputType.isInteger();
                case Op::isInt32:           return inputType.isInteger32();
                case Op::isInt64:           return inputType.isInteger64();
                case Op::isScalar:          return inputType.isScalar();
                case Op::isString:          return inputType.isStringLiteral();
                case Op::isBool:            return inputType.isBool();
                case Op::isReference:       return inputType.isReference();
                case Op::isConst:           return inputType.isConst();

                case Op::none:
                case Op::makeConst:
                case Op::makeConstSilent:
                case Op::makeReference:
                case Op::removeReference:
                case Op::elementType:
                case Op::primitiveType:
                case Op::size:
                default:                    SOUL_ASSERT_FALSE; return false;
            }
        }

        static bool canTakeSizeOf (const Type& type)
        {
            return type.isFixedSizeArray() || type.isVector() || type.isBoundedInt();
        }

        bool isMakingConst() const               { return operation == Op::makeConst || operation == Op::makeConstSilent; }
        bool isMakingReference() const           { return operation == Op::makeReference; }
        bool isRemovingReference() const         { return operation == Op::removeReference; }
        bool isChangingType() const              { return operation == Op::elementType || operation == Op::primitiveType; }

        bool isResolved() const override
        {
            if (isResolvedAsValue (source.get()))
                return checkSourceType (source->getResultType());

            if (isResolvedAsType (source.get()))
                return checkSourceType (source->resolveAsType());

            return false;
        }

        Constness getConstness() const override  { return isMakingConst() ? Constness::definitelyConst : source->getConstness(); }

        pool_ptr<StructDeclaration> getAsStruct() override
        {
            if (operation == Op::makeConst || operation == Op::makeConstSilent
                 || operation == Op::makeReference || operation == Op::removeReference)
                return source->getAsStruct();

            return {};
        }

        Type resolveAsType() const override
        {
            SOUL_ASSERT (isResolved() && operationReturnsAType (operation));
            throwErrorIfUnresolved();

            auto type = getSourceType();

            if (operation == Op::makeConst)        return type.createConst();
            if (operation == Op::makeConstSilent)  return type.createConstIfNotPresent();
            if (operation == Op::makeReference)    return type.isReference() ? type : type.createReference();
            if (operation == Op::removeReference)  return type.removeReferenceIfPresent();
            if (operation == Op::elementType)      return type.getElementType();
            if (operation == Op::primitiveType)    return type.getPrimitiveType();

            SOUL_ASSERT_FALSE; return {};
        }

        bool checkSourceType (const Type& sourceType) const
        {
            if (operation == Op::size)           return canTakeSizeOf (sourceType);
            if (operation == Op::makeConst)      return ! sourceType.isConst();
            if (operation == Op::elementType)    return sourceType.isArrayOrVector();
            if (operation == Op::primitiveType)  return ! (sourceType.isArray() || sourceType.isStruct());

            return true;
        }

        void throwErrorIfUnresolved() const
        {
            if (isResolvedAsValue (source.get()))
                throwErrorIfUnresolved (source->getResultType());
            else if (isResolvedAsType (source.get()))
                throwErrorIfUnresolved (source->resolveAsType());
        }

        void throwErrorIfUnresolved (const Type& sourceType) const
        {
            if (! checkSourceType (sourceType))
            {
                if (operation == Op::size)           source->context.throwError (Errors::cannotTakeSizeOfType());
                if (operation == Op::makeConst)      context.throwError (Errors::tooManyConsts());
                if (operation == Op::elementType)    context.throwError (Errors::badTypeForElementType());
                if (operation == Op::primitiveType)  context.throwError (Errors::badTypeForPrimitiveType());
            }
        }

        Type getSourceType() const
        {
            return isResolvedAsType (source.get()) ? source->resolveAsType()
                                                   : source->getResultType();
        }

        Value getResultValue() const
        {
            SOUL_ASSERT (isResolved() && ! operationReturnsAType (operation));
            return performOp (operation, getSourceType());
        }

        Type getResultType() const override
        {
            switch (operation)
            {
                case Op::size:
                    return PrimitiveType::int64;

                case Op::isStruct:
                case Op::isArray:
                case Op::isDynamicArray:
                case Op::isFixedSizeArray:
                case Op::isVector:
                case Op::isPrimitive:
                case Op::isFloat:
                case Op::isFloat32:
                case Op::isFloat64:
                case Op::isInt:
                case Op::isInt32:
                case Op::isInt64:
                case Op::isScalar:
                case Op::isString:
                case Op::isBool:
                case Op::isReference:
                case Op::isConst:
                    return PrimitiveType::bool_;

                case Op::none:
                case Op::makeConst:
                case Op::makeConstSilent:
                case Op::makeReference:
                case Op::removeReference:
                case Op::elementType:
                case Op::primitiveType:
                default:
                    SOUL_ASSERT_FALSE;
                    return {};
            }
        }

        bool isSizeOfUnsizedType() const
        {
            return operation == Op::size && source->isResolved() && getSourceType().isUnsizedArray();
        }

        pool_ref<Expression> source;
        const Op operation;
    };

    struct DotOperator  : public Expression
    {
        DotOperator (const Context& c, Expression& a, QualifiedIdentifier& b)
           : Expression (ObjectType::DotOperator, c, ExpressionKind::unknown), lhs (a), rhs (b) {}

        bool isResolved() const override            { return false; }

        pool_ref<Expression> lhs;
        QualifiedIdentifier& rhs;
    };

    //==============================================================================
    struct VariableDeclaration  : public Statement
    {
        VariableDeclaration (const Context& c, pool_ptr<Expression> type, pool_ptr<Expression> initialiser, bool isConst)
            : Statement (ObjectType::VariableDeclaration, c), declaredType (type), initialValue (initialiser),
              isConstant (isConst)
        {
            SOUL_ASSERT (initialValue != nullptr || declaredType != nullptr);
            SOUL_ASSERT (declaredType == nullptr || isPossiblyType (declaredType));
            SOUL_ASSERT (initialValue == nullptr || isPossiblyValue (initialValue));
        }

        bool isResolved() const
        {
            if (declaredType != nullptr)
                return initialValue == nullptr && isResolvedAsType (declaredType);

            return isResolvedAsValue (initialValue);
        }

        bool isAssignable() const
        {
            if (isConstant || declaredType == nullptr)
                return ! isConstant;

            return ! (isResolved() && declaredType->resolveAsType().isConst());
        }

        Type getType() const
        {
            if (declaredType != nullptr)
                return declaredType->resolveAsType();

            auto t = initialValue->getResultType();

            if (t.isValid() && isConstant != t.isConst())
                return isConstant ? t.createConst()
                                  : t.removeConst();

            return t;
        }

        bool isCompileTimeConstant() const
        {
            return isConstant && (initialValue == nullptr || initialValue->isCompileTimeConstant());
        }

        Identifier name;
        pool_ptr<Expression> declaredType, initialValue;
        Annotation annotation;
        bool isFunctionParameter = false;
        bool isConstant = false;
        bool isExternal = false;
        size_t numReads = 0, numWrites = 0;

        heart::Variable& getGeneratedVariable() const
        {
            SOUL_ASSERT (generatedVariable != nullptr);
            return *generatedVariable;
        }

        pool_ptr<heart::Variable> generatedVariable;
    };

    //==============================================================================
    struct VariableRef  : public Expression
    {
        VariableRef (const Context& c, VariableDeclaration& v)
           : Expression (ObjectType::VariableRef, c, ExpressionKind::value), variable (v)
        {}

        bool isResolved() const override             { return variable->isResolved(); }
        Type getResultType() const override          { return variable->getType(); }
        bool isAssignable() const override           { return variable->isAssignable(); }
        bool isCompileTimeConstant() const override  { return variable->isCompileTimeConstant(); }

        pool_ptr<Constant> getAsConstant() override
        {
            if (isCompileTimeConstant() && variable->initialValue != nullptr)
                return variable->initialValue->getAsConstant();

            return Expression::getAsConstant();
        }

        pool_ref<VariableDeclaration> variable;
    };

    //==============================================================================
    struct CallOrCastBase  : public Expression
    {
        CallOrCastBase (ObjectType ot, const Context& c, pool_ptr<CommaSeparatedList> args, bool isMethod)
            : Expression (ot, c, ExpressionKind::value), arguments (args), isMethodCall (isMethod)
        {}

        bool isResolved() const override        { return false; }
        bool areAllArgumentsResolved() const    { return arguments == nullptr || arguments->isResolved(); }
        size_t getNumArguments() const          { return arguments == nullptr ? 0 : arguments->items.size(); }

        TypeArray getArgumentTypes() const
        {
            if (arguments != nullptr)
                return arguments->getListOfResultTypes();

            return {};
        }

        std::string getIDStringForArgumentTypes() const
        {
            auto types = getArgumentTypes();
            auto args = std::to_string (types.size());

            for (auto& argType : types)
                args += "_" + argType.getShortIdentifierDescription();

            return args;
        }

        std::string getDescription (std::string name) const
        {
            auto argTypes = getArgumentTypes();

            if (isMethodCall)
            {
                SOUL_ASSERT (! argTypes.empty());

                return TokenisedPathString::join (argTypes.front().getDescription(), name)
                        + heart::Utilities::getDescriptionOfTypeList (ArrayView<Type> (argTypes).tail(), true);
            }

            return name + heart::Utilities::getDescriptionOfTypeList (argTypes, true);
        }

        pool_ptr<CommaSeparatedList> arguments;
        bool isMethodCall;
    };

    struct CallOrCast  : public CallOrCastBase
    {
        CallOrCast (Expression& nameOrTargetType, pool_ptr<CommaSeparatedList> args, bool isMethod)
            : CallOrCastBase (ObjectType::CallOrCast, nameOrTargetType.context, args, isMethod), nameOrType (nameOrTargetType)
        {
            SOUL_ASSERT (nameOrType != nullptr);
        }

        pool_ptr<Expression> nameOrType;
    };

    struct FunctionCall  : public CallOrCastBase
    {
        FunctionCall (const Context& c, Function& function, pool_ptr<CommaSeparatedList> args, bool isMethod)
            : CallOrCastBase (ObjectType::FunctionCall, c, args, isMethod), targetFunction (function)
        {}

        bool isResolved() const override        { return areAllArgumentsResolved() && (targetFunction.returnType == nullptr || targetFunction.returnType->isResolved()); }
        Type getResultType() const override     { return targetFunction.returnType->resolveAsType(); }

        Function& targetFunction;
    };

    struct TypeCast  : public Expression
    {
        TypeCast (const Context& c, Type destType, Expression& sourceValue)
            : Expression (ObjectType::TypeCast, c, ExpressionKind::value),
              targetType (std::move (destType)), source (sourceValue)
        {
        }

        bool isResolved() const override             { return source->isResolved(); }
        Type getResultType() const override          { return targetType; }
        bool isCompileTimeConstant() const override  { return source->isCompileTimeConstant(); }
        Constness getConstness() const override      { return targetType.isConst() ? Constness::definitelyConst : source->getConstness(); }

        size_t getNumArguments() const
        {
            if (auto list = cast<CommaSeparatedList> (source))
                return list->items.size();

            return 1;
        }

        Type targetType;
        pool_ref<Expression> source;
    };

    //==============================================================================
    struct CommaSeparatedList  : public Expression
    {
        CommaSeparatedList (const Context& c)
            : Expression (ObjectType::CommaSeparatedList, c, ExpressionKind::unknown)
        {
        }

        bool isResolved() const override
        {
            for (auto& i : items)
                if (! i->isResolved())
                    return false;

            return true;
        }

        bool isCompileTimeConstant() const override
        {
            for (auto& i : items)
                if (! i->isCompileTimeConstant())
                    return false;

            return true;
        }

        TypeArray getListOfResultTypes() const
        {
            TypeArray types;
            types.reserve (items.size());

            for (auto& i : items)
                types.push_back (i->getResultType());

            return types;
        }

        ArrayWithPreallocation<pool_ref<Expression>, 4> items;
    };

    //==============================================================================
    struct UnaryOperator  : public Expression
    {
        UnaryOperator (const Context& c, Expression& s, UnaryOp::Op op)
           : Expression (ObjectType::UnaryOperator, c, ExpressionKind::value), source (s), operation (op) {}

        bool isResolved() const override                { return source->isResolved(); }
        bool isCompileTimeConstant() const override     { return source->isCompileTimeConstant(); }
        Constness getConstness() const override         { return source->getConstness(); }

        Type getResultType() const override
        {
            if (operation == UnaryOp::Op::logicalNot)
                return PrimitiveType::bool_;

            if (operation == UnaryOp::Op::bitwiseNot)
                return PrimitiveType::int32;

            return source->getResultType();
        }

        pool_ref<Expression> source;
        UnaryOp::Op operation;
    };

    //==============================================================================
    struct BinaryOperator  : public Expression
    {
        BinaryOperator (const Context& c, Expression& a, Expression& b, BinaryOp::Op op)
           : Expression (ObjectType::BinaryOperator, c, ExpressionKind::value), lhs (a), rhs (b), operation (op)
        {
            SOUL_ASSERT (isPossiblyValue (lhs.get()) && isPossiblyValue (rhs.get()));
        }

        bool isOutputEndpoint() const override      { return operation == BinaryOp::Op::leftShift && lhs->isOutputEndpoint(); }
        bool isResolved() const override            { return isResolvedAsValue (lhs.get()) && isResolvedAsValue (rhs.get()); }
        bool isCompileTimeConstant() const override { return lhs->isCompileTimeConstant() && rhs->isCompileTimeConstant(); }
        Type getOperandType() const                 { resolveOpTypes(); return resolvedOpTypes.operandType; }
        Type getResultType() const override         { resolveOpTypes(); return resolvedOpTypes.resultType; }

        Constness getConstness() const override
        {
            auto const1 = lhs->getConstness();
            auto const2 = rhs->getConstness();
            return const1 == const2 ? const1 : Constness::unknown;
        }

        pool_ref<Expression> lhs, rhs;
        BinaryOp::Op operation;

    private:
        void resolveOpTypes() const
        {
            if (! resolvedOpTypes.resultType.isValid())
            {
                SOUL_ASSERT (isResolved());
                resolvedOpTypes = BinaryOp::getTypes (operation, lhs->getResultType(), rhs->getResultType());
            }
        }

        // this gets cached because doing so provides a 1000x speed-up in some
        // pathological nested parentheses code examples
        mutable TypeRules::BinaryOperatorTypes resolvedOpTypes;
    };

    //==============================================================================
    struct Assignment  : public Expression
    {
        Assignment (const Context& c, Expression& dest, Expression& source)
            : Expression (ObjectType::Assignment, c, ExpressionKind::value), target (dest), newValue (source)
        {
            SOUL_ASSERT (isPossiblyValue (target.get()) && isPossiblyValue (newValue.get()));
        }

        bool isResolved() const override            { return target->isResolved() && newValue->isResolved(); }
        Type getResultType() const override         { return target->getResultType(); }

        pool_ref<Expression> target, newValue;
    };

    //==============================================================================
    struct PreOrPostIncOrDec  : public Expression
    {
        PreOrPostIncOrDec (const Context& c, Expression& input, bool inc, bool post)
            : Expression (ObjectType::PreOrPostIncOrDec, c, ExpressionKind::value), target (input), isIncrement (inc), isPost (post)
        {
            SOUL_ASSERT (isPossiblyValue (input));
        }

        bool isResolved() const override            { return target->isResolved(); }
        Type getResultType() const override         { return target->getResultType(); }

        pool_ref<Expression> target;
        bool isIncrement, isPost;
    };

    //==============================================================================
    struct ArrayElementRef  : public Expression
    {
        ArrayElementRef (const Context& c, Expression& o, pool_ptr<Expression> start, pool_ptr<Expression> end, bool slice)
            : Expression (ObjectType::ArrayElementRef, c, ExpressionKind::value),
              object (o), startIndex (start), endIndex (end), isSlice (slice)
        {
            SOUL_ASSERT (isPossiblyValue (object) || isPossiblyEndpoint (object));
        }

        bool isAssignable() const override          { return object->isAssignable(); }
        bool isOutputEndpoint() const override      { return object->isOutputEndpoint(); }

        bool isResolved() const override
        {
            return isSlice ? isSliceRangeValid()
                           : (isResolvedAsValue (object) && isResolvedAsValue (startIndex));
        }

        Type getResultType() const override
        {
            auto arrayOrVectorType = object->getResultType();

            if (! arrayOrVectorType.isArrayOrVector())
                return {};

            auto elementType = arrayOrVectorType.getElementType();

            if (isSlice)
            {
                if (! isSliceRangeValid())
                    return {};

                auto range = getResolvedSliceRange();
                auto sliceSize = (Type::ArraySize) (range.end - range.start);

                if (sliceSize > 1)
                    return arrayOrVectorType.createCopyWithNewArraySize (sliceSize);
            }

            return elementType;
        }

        struct SliceRange
        {
            Type::ArraySize start, end;
        };

        SliceRange getResolvedSliceRange() const
        {
            SOUL_ASSERT (isSliceRangeValid());
            int64_t start = 0, end = 0;

            if (auto c = startIndex->getAsConstant())
                start = c->value.getAsInt64();

            auto type = object->getResultType();

            if (endIndex == nullptr)
                end = (int64_t) type.getArrayOrVectorSize();
            else if (auto c = endIndex->getAsConstant())
                end = c->value.getAsInt64();

            return { type.convertArrayOrVectorIndexToValidRange (start),
                     type.convertArrayOrVectorIndexToValidRange (end) };
        }

        bool isSliceRangeValid() const
        {
            if (isSlice && isResolvedAsValue (object) && isResolvedAsValue (startIndex))
            {
                int64_t start = 0, end = 0;

                if (auto c = startIndex->getAsConstant())
                    start = c->value.getAsInt64();
                else
                    return false;

                if (endIndex == nullptr)
                {
                    end = (int64_t) object->getResultType().getArrayOrVectorSize();
                }
                else
                {
                    if (! endIndex->isResolved())
                        return false;

                    SOUL_ASSERT (isResolvedAsValue (endIndex));

                    if (auto c = endIndex->getAsConstant())
                        end = c->value.getAsInt64();
                    else
                        return false;
                }

                auto type = object->getResultType();
                return type.isArrayOrVector() && type.isValidArrayOrVectorRange (start, end);
            }

            return false;
        }

        pool_ptr<Expression> object, startIndex, endIndex;
        bool isSlice = false;
        bool suppressWrapWarning = false;
    };

    //==============================================================================
    struct StructMemberRef  : public Expression
    {
        StructMemberRef (const Context& c, Expression& o, StructurePtr s, std::string member)
            : Expression (ObjectType::StructMemberRef, c, ExpressionKind::value),
              object (o), structure (s), memberName (std::move (member))
        {
            SOUL_ASSERT (isPossiblyValue (object.get()) && structure != nullptr);
        }

        bool isResolved() const override        { return object->isResolved(); }
        bool isAssignable() const override      { return object->isAssignable(); }
        Type getResultType() const override     { return structure->getMemberWithName (memberName).type; }

        pool_ref<Expression> object;
        StructurePtr structure;
        std::string memberName;
    };

    //==============================================================================
    struct AdvanceClock  : public Expression
    {
        AdvanceClock (const Context& c)  : Expression (ObjectType::AdvanceClock, c, ExpressionKind::value) {}

        bool isResolved() const override            { return true; }
        Type getResultType() const override         { return PrimitiveType::void_; }
    };

    //==============================================================================
    struct WriteToEndpoint  : public Expression
    {
        WriteToEndpoint (const Context& c, Expression& endpoint, Expression& v)
            : Expression (ObjectType::WriteToEndpoint, c, ExpressionKind::endpoint), target (endpoint), value (v) {}

        bool isOutputEndpoint() const override      { return true; }
        bool isResolved() const override            { return value->isResolved(); }
        Type getResultType() const override         { return target->getResultType(); }

        pool_ref<Expression> target, value;
    };

    //==============================================================================
    struct ProcessorProperty  : public Expression
    {
        ProcessorProperty (const Context& c, heart::ProcessorProperty::Property prop)
            : Expression (ObjectType::ProcessorProperty, c, ExpressionKind::value), property (prop)
        {
        }

        bool isResolved() const override                { return true; }
        Type getResultType() const override             { return heart::ProcessorProperty::getPropertyType (property); }
        bool isCompileTimeConstant() const override     { return true; }
        Constness getConstness() const override         { return Constness::definitelyConst; }

        heart::ProcessorProperty::Property property;
    };

    //==============================================================================
    struct StaticAssertion  : public Expression
    {
        StaticAssertion (const Context& c, Expression& failureCondition, std::string error)
            : Expression (ObjectType::StaticAssertion, c, ExpressionKind::unknown),
              condition (failureCondition), errorMessage (std::move (error))
        {
            SOUL_ASSERT (isPossiblyValue (condition.get()));
        }

        bool isResolved() const override            { return condition->isResolved(); }
        Type getResultType() const override         { return PrimitiveType::void_; }

        void testAndThrowErrorOnFailure()
        {
            if (isResolvedAsValue (condition.get()))
                if (auto c = condition->getAsConstant())
                    if (! c->value.getAsBool())
                        context.throwError (errorMessage.empty() ? Errors::staticAssertionFailure()
                                                                 : Errors::staticAssertionFailureWithMessage (errorMessage), true);
        }

        pool_ref<Expression> condition;
        std::string errorMessage;
    };


    AST() = delete;
};

} // namespace soul
