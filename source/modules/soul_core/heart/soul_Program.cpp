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

#if ! SOUL_INSIDE_CORE_CPP
 #error "Don't add this cpp file to your build, it gets included indirectly by soul_core.cpp"
#endif

namespace soul
{

struct Program::ProgramImpl  : public RefCountedObject
{
    ProgramImpl() = default;
    ProgramImpl (ProgramImpl&&) = delete;
    ProgramImpl (const ProgramImpl&) = delete;

    heart::Allocator allocator;
    std::vector<pool_ref<Module>> modules;
    ConstantTable constantTable;
    StringDictionary stringDictionary;

    uint32_t nextModuleID = 1;

    pool_ptr<Module> getModuleWithName (const std::string& name) const
    {
        for (auto& m : modules)
            if (m->fullName == name)
                return m;

        return {};
    }

    pool_ptr<Module> getModuleContainingFunction (const heart::Function& f) const
    {
        for (auto& m : modules)
            if (contains (m->functions, f))
                return m;

        return {};
    }

    void removeModule (Module& module)
    {
        removeIf (modules, [&] (pool_ref<Module> m) { return m == module; });
    }

    Module& getOrCreateNamespace (const std::string& name)
    {
        if (auto m = getModuleWithName (name))
            return *m;

        Program p (*this, false);
        auto& newModule = Module::createNamespace (p);
        newModule.shortName = name;
        newModule.fullName = name;
        newModule.originalFullName = name;
        modules.push_back (newModule);
        return newModule;
    }

    pool_ptr<heart::Variable> getVariableWithName (const std::string& name)
    {
        TokenisedPathString path (name);
        auto parentPath = path.getParentPath();
        auto variableName = path.getLastPart();

        if (auto m = getModuleWithName (TokenisedPathString::join (Program::getRootNamespaceName(), path.getParentPath())))
            return m->findStateVariable (variableName);

        return {};
    }

    pool_ptr<heart::Function> getFunctionWithName (const std::string& name)
    {
        TokenisedPathString path (name);
        auto parentPath = path.getParentPath();
        auto functionName = path.getLastPart();

        if (auto m = getModuleWithName (TokenisedPathString::join (Program::getRootNamespaceName(), path.getParentPath())))
            return m->findFunction (functionName);

        return {};
    }

    pool_ptr<Module> getMainProcessor() const
    {
        for (auto& m : modules)
            if (m->isProcessor() || m->isGraph())
                if (m->annotation.getBool ("main"))
                    return m;

        for (auto& m : modules)
            if (m->isProcessor() || m->isGraph())
                if (! m->annotation.hasValue ("main"))
                    return m;

        return {};
    }

    uint32_t getModuleID (Module& m, uint32_t arraySize)
    {
        if (m.moduleID == 0)
        {
            m.moduleID = nextModuleID;
            nextModuleID += arraySize;
        }

        return m.moduleID;
    }

    std::vector<pool_ref<heart::Variable>> getExternalVariables() const
    {
        std::vector<pool_ref<heart::Variable>> result;

        for (auto& m : modules)
            for (auto& v : m->stateVariables)
                if (v->isExternal())
                    result.push_back (v);

        return result;
    }

    Program clone() const
    {
        Program newProgram;
        newProgram.pimpl->stringDictionary = stringDictionary;

        ModuleCloner::FunctionMappings functionMappings;
        ModuleCloner::StructMappings structMappings;
        ModuleCloner::VariableMappings variableMappings;
        std::vector<ModuleCloner> cloners;

        for (auto& m : modules)
        {
            auto& newModule = newProgram.getAllocator().allocate<Module> (newProgram, m);
            newProgram.pimpl->insert (-1, newModule);
            cloners.emplace_back (m, newModule, functionMappings, structMappings, variableMappings);
        }

        for (auto& c : cloners)
            c.createStructPlaceholders();

        for (auto& c : cloners)
            c.cloneStructAndFunctionPlaceholders();

        for (auto& c : cloners)
            c.clone();

        for (auto& c : constantTable)
            newProgram.pimpl->constantTable.addItem ({ c.handle, std::make_unique<Value> (cloneValue (structMappings, *c.value)) });

        return newProgram;
    }

    std::string getVariableNameWithQualificationIfNeeded (const Module& context, const heart::Variable& v) const
    {
        if (v.isState())
        {
            for (auto& m : modules)
            {
                if (contains (m->stateVariables, v))
                {
                    if (m == context)
                        return v.name.toString();

                    return stripRootNamespaceFromQualifiedPath (TokenisedPathString::join (m->fullName, v.name));
                }
            }
        }

        return v.name;
    }

    std::string getExternalVariableName (const heart::Variable& v) const
    {
        SOUL_ASSERT (v.isState()); // This can only work for state variables

        for (auto& m : modules)
            if (contains (m->stateVariables, v))
                return TokenisedPathString::join (m->originalFullName, v.name);

        return v.name;
    }

    std::string getFunctionNameWithQualificationIfNeeded (const Module& context, const heart::Function& f) const
    {
        if (auto m = getModuleContainingFunction (f))
        {
            if (m == std::addressof (context))
                return f.name.toString();

            return TokenisedPathString::join (m->fullName, f.name);
        }

        SOUL_ASSERT_FALSE;
        return f.name;
    }

    std::string getStructNameWithQualificationIfNeeded (pool_ptr<const Module> context, const Structure& s) const
    {
        for (auto& m : modules)
        {
            if (contains (m->structs, s))
            {
                if (context != nullptr && m == context)
                    return s.getName();

                return stripRootNamespaceFromQualifiedPath (TokenisedPathString::join (m->fullName, s.getName()));
            }
        }

        SOUL_ASSERT_FALSE;
        return s.getName();
    }

    std::string getTypeDescriptionWithQualificationIfNeeded (pool_ptr<const Module> context, const Type& type) const
    {
        if (context == nullptr)
            type.getDescription ([] (const Structure& s) { return s.getName(); });

        return type.getDescription ([this, context] (const Structure& s) { return getStructNameWithQualificationIfNeeded (context, s); });
    }

    std::string getFullyQualifiedTypeDescription (const Type& type) const
    {
        return type.getDescription ([this] (const Structure& s) { return getStructNameWithQualificationIfNeeded ({}, s); });
    }

    Module& insert (int index, Module& newModule)
    {
        if (index < 0)
            modules.emplace_back (newModule);
        else
            modules.insert (modules.begin() + index, newModule);

        return newModule;
    }

    static Value cloneValue (ModuleCloner::StructMappings& structMappings, const Value& v)
    {
        Value newValue (v);
        newValue.getMutableType() = ModuleCloner::cloneType (structMappings, v.getType());
        return newValue;
    }
};

//==============================================================================
Program::Program (ProgramImpl& p, bool holdRef) : pimpl (&p) { if (holdRef) refHolder = p; }
Program::Program() : Program (*new ProgramImpl(), true) {}
Program::~Program() = default;

Program::Program (const Program&) = default;
Program::Program (Program&&) = default;
Program& Program::operator= (const Program&) = default;
Program& Program::operator= (Program&&) = default;

Program Program::createFromHEART (CompileMessageList& messageList, CodeLocation asmCode)
{
    try
    {
        CompileMessageHandler handler (messageList);
        return heart::Parser::parse (std::move (asmCode));
    }
    catch (AbortCompilationException) {}

    return {};
}

Program Program::clone() const                                                          { return pimpl->clone(); }
bool Program::isEmpty() const                                                           { return getModules().empty(); }
Program::operator bool() const                                                          { return ! isEmpty(); }
std::string Program::toHEART() const                                                    { return heart::Printer::getDump (*this); }
const std::vector<pool_ref<Module>>& Program::getModules() const                        { return pimpl->modules; }
void Program::removeModule (Module& module)                                             { return pimpl->removeModule (module); }

pool_ptr<Module> Program::getModuleWithName (const std::string& name) const             { return pimpl->getModuleWithName (name); }
pool_ptr<Module> Program::getModuleContainingFunction (const heart::Function& f) const  { return pimpl->getModuleContainingFunction (f); }
Module& Program::getOrCreateNamespace (const std::string& name)                         { return pimpl->getOrCreateNamespace (name); }
pool_ptr<heart::Function> Program::getFunctionWithName (const std::string& name) const  { return pimpl->getFunctionWithName (name); }
pool_ptr<heart::Variable> Program::getVariableWithName (const std::string& name) const  { return pimpl->getVariableWithName (name); }

heart::Allocator& Program::getAllocator()                                               { return pimpl->allocator; }
Module& Program::addGraph (int index)                                                   { return pimpl->insert (index, Module::createGraph     (*this)); }
Module& Program::addProcessor (int index)                                               { return pimpl->insert (index, Module::createProcessor (*this)); }
Module& Program::addNamespace (int index)                                               { return pimpl->insert (index, Module::createNamespace (*this)); }
pool_ptr<Module> Program::getMainProcessor() const                                      { return pimpl->getMainProcessor(); }
StringDictionary& Program::getStringDictionary()                                        { return pimpl->stringDictionary; }
const StringDictionary& Program::getStringDictionary() const                            { return pimpl->stringDictionary; }
ConstantTable& Program::getConstantTable()                                              { return pimpl->constantTable; }
const ConstantTable& Program::getConstantTable() const                                  { return pimpl->constantTable; }
std::vector<pool_ref<heart::Variable>> Program::getExternalVariables() const            { return pimpl->getExternalVariables(); }
uint32_t Program::getModuleID (Module& m, uint32_t arraySize)                           { return pimpl->getModuleID (m, arraySize); }
const char* Program::getRootNamespaceName()                                             { return "_root"; }
std::string Program::stripRootNamespaceFromQualifiedPath (std::string path)             { return TokenisedPathString::removeTopLevelNameIfPresent (path, getRootNamespaceName()); }

std::string Program::getHash() const
{
    IndentedStream dump;
    heart::Printer::print (*this, dump);
    HashBuilder hash;
    hash << dump.getContent();
    return hash.toString();
}

Module& Program::getMainProcessorOrThrowError() const
{
    auto main = getMainProcessor();

    if (main == nullptr)
        CodeLocation().throwError (Errors::cannotFindMainProcessor());

    SOUL_ASSERT (! main->isNamespace());
    return *main;
}

std::string Program::getVariableNameWithQualificationIfNeeded (const Module& context, const heart::Variable& v) const
{
    return pimpl->getVariableNameWithQualificationIfNeeded (context, v);
}

std::string Program::getExternalVariableName (const heart::Variable& v) const
{
    return pimpl->getExternalVariableName (v);
}

std::string Program::getFunctionNameWithQualificationIfNeeded (const Module& context, const heart::Function& f) const
{
    return pimpl->getFunctionNameWithQualificationIfNeeded (context, f);
}

std::string Program::getStructNameWithQualificationIfNeeded (const Module& context, const Structure& s) const
{
    return pimpl->getStructNameWithQualificationIfNeeded (context, s);
}

std::string Program::getFullyQualifiedStructName (const Structure& s) const
{
    return pimpl->getStructNameWithQualificationIfNeeded ({}, s);
}

std::string Program::getTypeDescriptionWithQualificationIfNeeded (pool_ptr<const Module> context, const Type& type) const
{
    return pimpl->getTypeDescriptionWithQualificationIfNeeded (context, type);
}

std::string Program::getFullyQualifiedTypeDescription (const Type& type) const
{
    return pimpl->getFullyQualifiedTypeDescription (type);
}

std::string Program::getValueDump (const Value& v, bool quoteStrings) const
{
    if (! quoteStrings && v.getType().isStringLiteral())
        return getStringDictionary().getStringForHandle (v.getStringLiteral());

    struct PrettyPrintValue  : public ValuePrinter
    {
        PrettyPrintValue (const Program& pp) : program (pp)
        {
            dictionary = std::addressof (program.getStringDictionary());
        }

        const Program& program;
        std::ostringstream out;
        void print (const std::string& s) override              { out << s; }

        void printInt32 (int32_t v) override
        {
            if (parseNextIntAsMIDI)
            {
                parseNextIntAsMIDI = false;
                const uint8_t bytes[] = { (uint8_t) (v >> 16), (uint8_t) (v >> 8), (uint8_t) v };
                postStructDesc = " = " + getMIDIMessageDescription (bytes, 3);
            }

            print (v > 0xffff ? "0x" + toHexString (v) : std::to_string (v));
        }

        void beginStructMembers (const Type& t) override
        {
            auto name = program.getFullyQualifiedTypeDescription (t);
            print (name + " { ");
            parseNextIntAsMIDI = soul::isMIDIMessageStruct (t);
        }

        void printInt64 (int64_t v) override               { print (v > 0xffff ? "0x" + toHexString (v) : std::to_string (v)); }
        void printStructMemberSeparator() override         { print (", "); }
        void endStructMembers() override                   { print (" }" + postStructDesc); postStructDesc = {}; }
        void beginArrayMembers (const Type& t) override    { print (t.getDescription() + " ("); }
        void printArrayMemberSeparator() override          { print (", "); }
        void endArrayMembers() override                    { print (")"); }
        void beginVectorMembers (const Type& t) override   { print (t.getDescription() + " ("); }
        void printVectorMemberSeparator() override         { print (", "); }
        void endVectorMembers() override                   { print (")"); }

        bool parseNextIntAsMIDI = false;
        std::string postStructDesc;
    };

    PrettyPrintValue pp (*this);
    v.print (pp);
    return pp.out.str();
}

} // namespace soul
