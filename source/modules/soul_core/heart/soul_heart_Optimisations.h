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
    This pass does some basic simplifications and optimisations
*/
struct Optimisations
{
    static void removeUnusedVariables (Program& program)
    {
        for (auto& m : program.getModules())
        {
            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                removeDuplicateConstants (f);

            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                convertWriteOnceVariablesToConstants (f);

            m->rebuildVariableUseCounts();

            for (auto f : m->functions)
                removeUnusedVariables (f);
        }
    }

    static void removeUnusedFunctions (Program& program, Module& mainModule)
    {
        removeCallsToVoidFunctionsWithoutSideEffects (program);

        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                f->functionUseTestFlag = false;

        for (auto f : mainModule.functions)
            if (f->isExported)
                recursivelyFlagFunctionUse (f);

        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                if (! f->functionUseTestFlag && f->annotation.getBool ("do_not_optimise"))
                    recursivelyFlagFunctionUse (f);

        for (auto& m : program.getModules())
            removeIf (m->functions, [] (heart::Function& f) { return ! f.functionUseTestFlag; });
    }

    static void removeUnusedProcessors (Program& program)
    {
        auto modules = program.getModules();

        for (auto& m : modules)
            if (m->isProcessor() && m->functions.empty() && m->structs.empty())
                program.removeModule (m);
    }

    static void removeUnusedNamespaces (Program& program)
    {
        auto modules = program.getModules();

        for (auto& m : modules)
            if (m->isNamespace() && m->functions.empty() && m->structs.empty()
                 && m->stateVariables.empty())
                program.removeModule (m);
    }

    static void removeUnusedStructs (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& s : m->structs)
                s->activeUseFlag = false;

        heart::Utilities::visitAllTypes (program, [] (const Type& t) { recursivelyFlagStructUse (t); });

        for (auto& m : program.getModules())
            removeIf (m->structs, [] (const StructurePtr& s) { return ! s->activeUseFlag; });
    }

    struct UnusedStructMembers
    {
        Module& module;
        Structure& structure;
        ArrayWithPreallocation<size_t, 4> unusedMembers;
    };

    static std::vector<UnusedStructMembers> findUnreadStructMembers (Program& program)
    {
        for (auto& module : program.getModules())
            for (auto& s : module->structs)
                for (auto& m : s->getMembers())
                    m.readWriteCount.reset();

        for (auto& module : program.getModules())
        {
            for (auto& f : module->functions)
            {
                f->visitExpressions ([] (pool_ref<heart::Expression>& value, AccessType mode)
                {
                    if (auto s = cast<heart::StructElement> (value))
                        s->getStruct().getMemberWithName (s->memberName).readWriteCount.increment (mode);
                });
            }
        }

        std::vector<UnusedStructMembers> results;

        for (auto& module : program.getModules())
        {
            for (auto& s : module->structs)
            {
                ArrayWithPreallocation<size_t, 4> unusedMembers;

                for (size_t i = 0; i < s->getNumMembers(); ++i)
                    if (s->getMemberReadWriteCount (i).numReads == 0)
                        unusedMembers.push_back (i);

                if (! unusedMembers.empty())
                {
                    std::reverse (unusedMembers.begin(), unusedMembers.end());
                    results.push_back ({ module, *s, unusedMembers });
                }
            }
        }

        return results;
    }

    static void optimiseFunctionBlocks (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto f : m->functions)
                optimiseFunctionBlocks (f, program.getAllocator());
    }

    static void optimiseFunctionBlocks (heart::Function& f, heart::Allocator& allocator)
    {
        f.rebuildBlockPredecessors();
        eliminateEmptyAndUnreachableBlocks (f, allocator);
        eliminateUnreachableBlockCycles (f);
        mergeAdjacentBlocks (f);
    }

    template <typename EndpointConnectionStatusProvider>
    static void removeUnconnectedEndpoints (Module& module, EndpointConnectionStatusProvider& ecsp)
    {
        removeUnconnectedInputs (module, ecsp);
        removeUnconnectedOutputs (module, ecsp);
    }

    static void makeFunctionCallInline (Program& program, heart::Function& parentFunction,
                                        size_t blockIndex, heart::FunctionCall& call)
    {
        SOUL_ASSERT (heart::Utilities::canFunctionBeInlined (program, parentFunction, call));
        SOUL_ASSERT (contains (parentFunction.blocks[blockIndex]->statements, std::addressof (call)));

        Inliner (*program.getModuleContainingFunction (call.getFunction()),
                 parentFunction, blockIndex, call, call.getFunction()).perform();
    }

    static bool inlineAllCallsToFunction (Program& program, heart::Function& functionToInline)
    {
        bool anyChanged = false;

        for (auto& m : program.getModules())
        {
            for (auto& f : m->functions)
            {
                auto result = inlineAllCallsToFunction (program, f, functionToInline);

                if (result == InlineResult::failed)
                    return false;

                if (result == InlineResult::ok)
                    anyChanged = true;
            }
        }

        if (! anyChanged)
            return false;

        removeItem (program.getModuleContainingFunction (functionToInline)->functions, functionToInline);
        return true;
    }

    static void garbageCollectStringDictionary (Program& program)
    {
        std::vector<StringDictionary::Handle> handlesUsed;

        for (auto& m : program.getModules())
            for (auto f : m->functions)
                f->visitExpressions ([&] (pool_ref<heart::Expression>& e, AccessType)
                                     {
                                         if (auto c = cast<heart::Constant> (e))
                                         {
                                             const auto& type = c->value.getType();

                                             if (type.isStringLiteral())
                                             {
                                                 auto handle = c->value.getStringLiteral();

                                                 if (! contains (handlesUsed, handle))
                                                     handlesUsed.push_back (handle);
                                             }
                                         }
                                     });

        removeIf (program.getStringDictionary().strings,
                  [&] (const StringDictionary::Item& item) { return ! contains (handlesUsed, item.handle); });
    }


private:
    static bool eliminateEmptyAndUnreachableBlocks (heart::Function& f, heart::Allocator& allocator)
    {
        return heart::Utilities::removeBlocks (f, [&] (heart::Block& b) -> bool
        {
            if (b.doNotOptimiseAway || f.blocks.front() == b)
                return false;

            if (b.predecessors.empty())
                return true;

            if (! b.statements.empty())
                return false;

            if (b.terminator == nullptr)
                return false;

            auto destinations = b.terminator->getDestinationBlocks();
            auto numDestinations = destinations.size();

            if (numDestinations > 1)
                return false;

            if (numDestinations == 1)
            {
                if (b == destinations.front())
                    return false;

                for (auto pred : b.predecessors)
                {
                    SOUL_ASSERT (pred->terminator != nullptr);
                    heart::Utilities::replaceBlockDestination (pred, b, destinations.front());
                }

                return true;
            }

            if (is_type<heart::ReturnVoid> (b.terminator))
            {
                if (heart::Utilities::areAllTerminatorsUnconditional (b.predecessors))
                {
                    for (auto pred : b.predecessors)
                        pred->terminator = allocator.allocate<heart::ReturnVoid>();

                    return true;
                }
            }

            return false;
        });
    }

    //==============================================================================
    static bool eliminateUnreachableBlockCycles (heart::Function& f)
    {
        return heart::Utilities::removeBlocks (f, [&] (heart::Block& b) -> bool
        {
            return f.blocks.front() != b
                    && ! isReachableFrom (f, b, f.blocks.front());
        });
    }

    static bool isReachableFrom (heart::Function& f, heart::Block& dest, heart::Block& source)
    {
        bool result = false;

        CallFlowGraph::visitUpstreamBlocks (f, dest,
            [&] (heart::Block& b) -> bool
            {
                if (std::addressof (b) == std::addressof (source)) { result = true; return false; }
                return true;
            });

        return result;
    }

    //==============================================================================
    static bool mergeAdjacentBlocks (heart::Function& f)
    {
        return heart::Utilities::removeBlocks (f, [&] (heart::Block& b) -> bool
        {
            if (b.predecessors.size() != 1 || b.doNotOptimiseAway)
                return false;

            auto pred = b.predecessors.front();

            if (pred == b || pred->terminator->isConditional())
                return false;

            SOUL_ASSERT (*pred->terminator->getDestinationBlocks().begin() == b);

            if (auto first = b.statements.begin())
                if (*first != nullptr)
                    pred->statements.append (**first);

            pred->terminator = b.terminator;
            return true;
        });
    }

    //==============================================================================
    static void recursivelyFlagFunctionUse (heart::Function& sourceFn)
    {
        if (! sourceFn.functionUseTestFlag)
        {
            sourceFn.functionUseTestFlag = true;

            sourceFn.visitStatements<heart::FunctionCall> ([] (heart::FunctionCall& fc)
            {
                recursivelyFlagFunctionUse (fc.getFunction());
            });

            sourceFn.visitExpressions ([] (pool_ref<heart::Expression>& value, AccessType)
            {
                if (auto fc = cast<heart::PureFunctionCall> (value))
                    recursivelyFlagFunctionUse (fc->function);
            });
        }
    }

    static void removeCallsToVoidFunctionsWithoutSideEffects (Program& program)
    {
        for (auto& m : program.getModules())
            for (auto& f : m->functions)
                for (auto& b : f->blocks)
                    b->statements.removeMatches ([] (heart::Statement& s)
                                                 {
                                                     if (auto call = cast<heart::FunctionCall> (s))
                                                         return call->target == nullptr && ! call->getFunction().mayHaveSideEffects();

                                                     return false;
                                                 });
    }

    static void recursivelyFlagStructUse (const Type& type)
    {
        if (type.isStruct())
        {
            if (! type.getStructRef().activeUseFlag)
            {
                type.getStructRef().activeUseFlag = true;

                for (auto& m : type.getStructRef().getMembers())
                    recursivelyFlagStructUse (m.type);
            }
        }
        else if (type.isArray())
        {
            recursivelyFlagStructUse (type.getArrayElementType());
        }
    }

    //==============================================================================
    static bool findAndReplaceFirstDuplicateConstant (heart::Function& f)
    {
        for (auto b : f.blocks)
        {
            LinkedList<heart::Statement>::Iterator last;

            for (auto s : b->statements)
            {
                if (auto a = cast<heart::AssignFromValue> (*s))
                {
                    if (auto target = cast<heart::Variable> (a->target))
                    {
                        if (target->isConstant())
                        {
                            if (auto source = cast<heart::Variable> (a->source))
                            {
                                if (source->isConstant())
                                {
                                    b->statements.removeNext (last);

                                    f.visitExpressions ([target, source] (pool_ref<heart::Expression>& value, AccessType mode)
                                    {
                                        if (value == target && mode == AccessType::read)
                                            value = *source;
                                    });

                                    return true;
                                }
                            }
                        }
                    }
                }

                last = *s;
            }
        }

        return false;
    }

    static void removeDuplicateConstants (heart::Function& f)
    {
        while (findAndReplaceFirstDuplicateConstant (f))
        {}
    }

    static void removeUnusedVariables (heart::Function& f)
    {
        for (auto b : f.blocks)
        {
            b->statements.removeMatches ([] (heart::Statement& s)
            {
                if (auto a = cast<heart::Assignment> (s))
                    if (auto target = cast<heart::Variable> (a->target))
                        return target->readWriteCount.numReads == 0 && target->isFunctionLocal();

                return false;
            });
        }
    }

    static void convertWriteOnceVariablesToConstants (heart::Function& f)
    {
        f.visitStatements<heart::Assignment> ([] (heart::Assignment& a)
        {
            if (auto target = cast<heart::Variable> (a.target))
                if (target->readWriteCount.numWrites == 1 && target->isMutableLocal())
                    target->role = heart::Variable::Role::constant;
        });
    }

    //==============================================================================
    template <typename EndpointConnectionStatusProvider>
    static void removeUnconnectedInputs (Module& module, EndpointConnectionStatusProvider& ecsp)
    {
        std::vector<pool_ref<heart::InputDeclaration>> toRemove;

        for (auto& i : module.inputs)
            if (! ecsp.isInputConnected (i))
                toRemove.push_back (i);

        removeFromVector (module.inputs, toRemove);

        removeIf (module.connections,
                  [&] (heart::Connection& connection)
                  {
                      if (connection.sourceProcessor == nullptr)
                          for (auto& i : toRemove)
                              if (connection.sourceEndpoint == i->name.toString())
                                  return true;

                      return false;
                  });

        for (auto& f : module.functions)
        {
            f->visitExpressions ([&] (pool_ref<heart::Expression>& value, AccessType mode)
            {
                if (mode == AccessType::read)
                    if (auto i = cast<heart::InputDeclaration> (value))
                        if (contains (toRemove, i))
                            value = module.allocator.allocateZeroInitialiser (value->getType());
            });
        }
    }

    template <typename EndpointConnectionStatusProvider>
    static void removeUnconnectedOutputs (Module& module, EndpointConnectionStatusProvider& ecsp)
    {
        std::vector<pool_ref<heart::OutputDeclaration>> toRemove;

        for (auto& o : module.outputs)
            if (! ecsp.isOutputConnected (o))
                toRemove.push_back (o);

        removeFromVector (module.outputs, toRemove);

        removeIf (module.connections,
                  [&] (heart::Connection& connection)
                  {
                      if (connection.destProcessor == nullptr)
                          for (auto& i : toRemove)
                              if (connection.destEndpoint == i->name.toString())
                                  return true;

                      return false;
                  });

        for (auto& f : module.functions)
        {
            for (auto& b : f->blocks)
            {
                b->statements.removeMatches ([&] (heart::Statement& s)
                {
                    if (auto w = cast<heart::WriteStream> (s))
                        return contains (toRemove, w->target);

                    return false;
                });
            }
        }
    }

    //==============================================================================
    struct Inliner
    {
        Inliner (Module& m, heart::Function& parentFn, size_t block,
                 heart::FunctionCall& fc, heart::Function& targetFn)
            : module (m), parentFunction (parentFn), call (fc), blockIndex (block), targetFunction (targetFn)
        {
            inlinedFnName = addSuffixToMakeUnique ("_inlined_" + targetFunction.name.toString(),
                                                   [&] (const std::string& nm)
                                                   {
                                                       return heart::Utilities::findBlock (parentFn, "@" + nm) != nullptr;
                                                   });
        }

        void perform()
        {
            auto& postBlock = heart::Utilities::splitBlock (module, parentFunction, blockIndex, call, "@" + inlinedFnName + "_end");
            postCallResumeBlock = postBlock;
            auto& preBlock = parentFunction.blocks[blockIndex].get();

            preBlock.statements.remove (call);

            if (! targetFunction.returnType.isVoid())
            {
                returnValueVar = module.allocate<heart::Variable> (CodeLocation(), targetFunction.returnType,
                                                                   module.allocator.get (inlinedFnName + "_retval"),
                                                                   heart::Variable::Role::mutableLocal);

                postBlock.statements.insertFront (module.allocate<heart::AssignFromValue> (call.location, *call.target, *returnValueVar));
            }

            {
                BlockBuilder builder (module, preBlock);

                for (size_t i = 0; i < targetFunction.parameters.size(); ++i)
                {
                    auto& param = targetFunction.parameters[i].get();
                    auto newParamName = inlinedFnName + "_param_" + makeSafeIdentifierName (param.name.toString());
                    auto& localParamVar = builder.createMutableLocalVariable (param.type, newParamName);
                    builder.addAssignment (localParamVar, call.arguments[i]);
                    remappedVariables[param] = localParamVar;
                }
            }

            newBlocks.reserve (targetFunction.blocks.size());

            for (auto& b : targetFunction.blocks)
            {
                // NB: the name of the first block must be "@" + inlinedFnName, since that's what the unique
                // name picker will look for to make sure there's not a block name clash
                auto name = "@" + inlinedFnName + (newBlocks.empty() ? std::string() : ("_" + std::to_string (newBlocks.size())));
                auto& newBlock = module.allocate<heart::Block> (module.allocator.get (name));
                newBlocks.push_back (newBlock);
                remappedBlocks[b] = newBlock;
            }

            parentFunction.blocks.insert (getIteratorForIndex (parentFunction.blocks, blockIndex + 1),
                                          newBlocks.begin(), newBlocks.end());

            preBlock.terminator = module.allocate<heart::Branch> (newBlocks.front());

            for (size_t i = 0; i < newBlocks.size(); ++i)
                cloneBlock (newBlocks[i], targetFunction.blocks[i]);
        }

        void cloneBlock (heart::Block& target, const heart::Block& source)
        {
            LinkedList<heart::Statement>::Iterator last;

            for (auto s : source.statements)
                last = target.statements.insertAfter (last, cloneStatement (*s));

            if (auto returnValue = cast<heart::ReturnValue> (source.terminator))
                target.statements.insertAfter (last, module.allocate<heart::AssignFromValue> (source.location, *returnValueVar,
                                                                                              cloneExpression (returnValue->returnValue)));

            target.terminator = cloneTerminator (*source.terminator);
        }

        heart::Statement& cloneStatement (heart::Statement& s)
        {
            #define SOUL_CLONE_STATEMENT(Type)     if (auto t = cast<const heart::Type> (s)) return clone (*t);
            SOUL_HEART_STATEMENTS (SOUL_CLONE_STATEMENT)
            #undef SOUL_CLONE_STATEMENT
            SOUL_ASSERT_FALSE;
            return s;
        }

        heart::Terminator& cloneTerminator (heart::Terminator& s)
        {
            #define SOUL_CLONE_TERMINATOR(Type)    if (auto t = cast<const heart::Type> (s)) return clone (*t);
            SOUL_HEART_TERMINATORS (SOUL_CLONE_TERMINATOR)
            #undef SOUL_CLONE_TERMINATOR
            SOUL_ASSERT_FALSE;
            return s;
        }

        heart::Branch& clone (const heart::Branch& old)
        {
            return module.allocate<heart::Branch> (*remappedBlocks[old.target]);
        }

        heart::BranchIf& clone (const heart::BranchIf& old)
        {
            return module.allocate<heart::BranchIf> (cloneExpression (old.condition),
                                                     *remappedBlocks[old.targets[0]],
                                                     *remappedBlocks[old.targets[1]]);
        }

        heart::Terminator& clone (const heart::ReturnVoid&)    { return module.allocate<heart::Branch> (*postCallResumeBlock); }
        heart::Terminator& clone (const heart::ReturnValue&)   { return module.allocate<heart::Branch> (*postCallResumeBlock); }

        heart::AssignFromValue& clone (const heart::AssignFromValue& old)
        {
            return module.allocate<heart::AssignFromValue> (old.location,
                                                            cloneExpression (*old.target),
                                                            cloneExpression (old.source));
        }

        heart::FunctionCall& clone (const heart::FunctionCall& old)
        {
            auto& fc = module.allocate<heart::FunctionCall> (old.location, cloneExpressionPtr (old.target), old.getFunction());

            for (auto& arg : old.arguments)
                fc.arguments.push_back (cloneExpression (arg));

            return fc;
        }

        heart::PureFunctionCall& clone (const heart::PureFunctionCall& old)
        {
            auto& fc = module.allocate<heart::PureFunctionCall> (old.location, old.function);

            for (auto& arg : old.arguments)
                fc.arguments.push_back (cloneExpression (arg));

            return fc;
        }

        heart::ReadStream& clone (const heart::ReadStream& old)
        {
            return module.allocate<heart::ReadStream> (old.location, cloneExpression (*old.target), old.source);
        }

        heart::WriteStream& clone (const heart::WriteStream& old)
        {
            return module.allocate<heart::WriteStream> (old.location, old.target,
                                                        cloneExpressionPtr (old.element),
                                                        cloneExpression (old.value));
        }

        heart::AdvanceClock& clone (const heart::AdvanceClock& a)
        {
            return module.allocate<heart::AdvanceClock> (a.location);
        }

        heart::Expression& cloneExpression (heart::Expression& old)
        {
            if (auto c = cast<heart::Constant> (old))
                return module.allocate<heart::Constant> (c->location, c->value);

            if (auto b = cast<heart::BinaryOperator> (old))
                return module.allocate<heart::BinaryOperator> (b->location,
                                                               cloneExpression (b->lhs),
                                                               cloneExpression (b->rhs),
                                                               b->operation);

            if (auto u = cast<heart::UnaryOperator> (old))
                return module.allocate<heart::UnaryOperator> (u->location, cloneExpression (u->source), u->operation);

            if (auto t = cast<heart::TypeCast> (old))
                return module.allocate<heart::TypeCast> (t->location, cloneExpression (t->source), t->destType);

            if (auto f = cast<heart::PureFunctionCall> (old))
                return clone (*f);

            if (auto v = cast<heart::Variable> (old))
                return getRemappedVariable (*v);

            if (auto s = cast<heart::ArrayElement> (old))
                return cloneArrayElement (*s);

            if (auto s = cast<heart::StructElement> (old))
                return cloneStructElement (*s);

            auto pp = cast<heart::ProcessorProperty> (old);
            SOUL_ASSERT (pp != nullptr);
            return module.allocate<heart::ProcessorProperty> (pp->location, pp->property);
        }

        pool_ptr<heart::Expression> cloneExpressionPtr (pool_ptr<heart::Expression> old)
        {
            if (old != nullptr)
                return cloneExpression (*old);

            return {};
        }

        heart::Variable& getRemappedVariable (heart::Variable& old)
        {
            if (old.isFunctionLocal() || old.isParameter())
            {
                auto& v = remappedVariables[old];

                if (v == nullptr)
                {
                    v = module.allocate<heart::Variable> (old.location, old.type,
                                                          old.name.isValid() ? module.allocator.get (inlinedFnName + "_" + makeSafeIdentifierName (old.name.toString()))
                                                                             : Identifier(),
                                                          old.role);
                    v->annotation = old.annotation;
                }

                return *v;
            }

            return old;
        }

        heart::ArrayElement& cloneArrayElement (const heart::ArrayElement& old)
        {
            auto& s = module.allocate<heart::ArrayElement> (old.location,
                                                            cloneExpression (old.parent),
                                                            old.fixedStartIndex,
                                                            old.fixedEndIndex);

            s.dynamicIndex = cloneExpressionPtr (old.dynamicIndex);
            s.suppressWrapWarning = old.suppressWrapWarning;
            s.isRangeTrusted = old.isRangeTrusted;
            return s;
        }

        heart::StructElement& cloneStructElement (const heart::StructElement& old)
        {
            return module.allocate<heart::StructElement> (old.location,
                                                          cloneExpression (old.parent),
                                                          old.memberName);
        }

        Module& module;
        heart::Function& parentFunction;
        heart::FunctionCall& call;
        size_t blockIndex;
        heart::Function& targetFunction;
        std::string inlinedFnName;
        std::vector<pool_ref<heart::Block>> newBlocks;
        std::unordered_map<pool_ref<heart::Block>, pool_ptr<heart::Block>> remappedBlocks;
        std::unordered_map<pool_ref<heart::Variable>, pool_ptr<heart::Variable>> remappedVariables;
        pool_ptr<heart::Block> postCallResumeBlock;
        pool_ptr<heart::Variable> returnValueVar;
    };

    enum class InlineResult { ok, failed, noneFound };

    static InlineResult inlineNextCall (Program& program, heart::Function& parentFunction, heart::Function& functionToInline)
    {
        for (size_t blockIndex = 0; blockIndex < parentFunction.blocks.size(); ++blockIndex)
        {
            for (auto s : parentFunction.blocks[blockIndex]->statements)
            {
                if (auto call = cast<heart::FunctionCall> (*s))
                {
                    if (call->function == functionToInline)
                    {
                        if (! heart::Utilities::canFunctionBeInlined (program, parentFunction, *call))
                            return InlineResult::failed;

                        makeFunctionCallInline (program, parentFunction, blockIndex, *call);
                        return InlineResult::ok;
                    }
                }
            }
        }

        return InlineResult::noneFound;
    }

    static InlineResult inlineAllCallsToFunction (Program& program, heart::Function& parentFunction, heart::Function& functionToInline)
    {
        bool anyChanged = false;

        for (;;)
        {
            auto result = inlineNextCall (program, parentFunction, functionToInline);

            if (result == InlineResult::failed)
                return result;

            if (result == InlineResult::noneFound)
                return anyChanged ? InlineResult::ok
                                  : InlineResult::noneFound;

            anyChanged = true;
        }
    }
};

} // namespace soul
