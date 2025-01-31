/*
This Source Code Form is subject to the terms of the Mozilla
Public License, v. 2.0. If a copy of the MPL was not distributed
with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "./include/llvm.hpp"
#include <llvm-c/Target.h>
#include "./include/parser/ast.hpp"
#include "./include/parser/nodes/NodeFunc.hpp"
#include "./include/parser/nodes/NodeStruct.hpp"
#include "./include/parser/nodes/NodeVar.hpp"
#include "./include/parser/nodes/NodeInt.hpp"
#include <iostream>
#include <string>

#include <llvm/Target/TargetMachine.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Analysis/TargetLibraryInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/IR/LegacyPassManager.h>

// Wrapper for the LLVMBuildLoad2 function using RaveValue.
RaveValue LLVM::load(RaveValue value, const char* name, int loc) {
    return {LLVMBuildLoad2(generator->builder, generator->genType(value.type->getElType(), loc), value.value, name), value.type->getElType()};
}

// Checking for LLVM::load operation
bool LLVM::isLoad(LLVMValueRef operation) {
    return LLVMIsALoadInst(operation);
}

void LLVM::undoLoad(RaveValue& value) {
    LLVMValueRef origArg = LLVMGetArgOperand(value.value, 0);
    value.value = origArg;
    value.type = new TypePointer(value.type);
}

// Wrapper for the LLVMBuildCall2 function using RaveValue.
RaveValue LLVM::call(RaveValue fn, LLVMValueRef* args, unsigned int argsCount, const char* name, std::vector<int> byVals) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;

    std::vector<LLVMTypeRef> types;
    for(int i=0; i<tfunc->args.size(); i++) types.push_back(generator->genType(tfunc->args[i]->type, -1));

    RaveValue result = {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -1), types.data(), types.size(), tfunc->isVarArg), fn.value, args, argsCount, name), tfunc->main};

    for(int i=0; i<byVals.size(); i++) LLVMAddCallSiteAttribute(result.value, byVals[i] + 1, LLVMCreateTypeAttribute(generator->context, LLVMGetEnumAttributeKindForName("byval", 5), types[byVals[i]]));

    return result;
}

// Wrapper for the LLVMBuildCall2 function using RaveValue; accepts a vector of RaveValue instead of pointer to LLVMValueRef.
RaveValue LLVM::call(RaveValue fn, std::vector<RaveValue> args, const char* name, std::vector<int> byVals) {
    TypeFunc* tfunc = instanceof<TypePointer>(fn.type) ? (TypeFunc*)fn.type->getElType() : (TypeFunc*)fn.type;
    std::vector<LLVMValueRef> lArgs(args.size());
    std::transform(args.begin(), args.end(), lArgs.begin(), [](RaveValue& arg) { return arg.value; });

    std::vector<LLVMTypeRef> types(tfunc->args.size());
    std::map<int, LLVMTypeRef> byValStructures;

    for(int i=0; i<tfunc->args.size(); i++) {
        Type* argType = tfunc->args[i]->type;

        if(instanceof<TypeStruct>(argType)) {
            TypeStruct* tstruct = static_cast<TypeStruct*>(argType);
            if(AST::structTable.find(tstruct->name) == AST::structTable.end() && generator->toReplace.find(tstruct->name) == generator->toReplace.end())
                types[i] = generator->genType(args[i].type->getElType(), -2);
            else types[i] = generator->genType(argType, -2);
        }
        else types[i] = generator->genType(argType, -2);

        if(std::find(byVals.begin(), byVals.end(), i) != byVals.end()) {
            byValStructures[i] = types[i];
            types[i] = LLVMPointerType(types[i], 0);
        }
    }

    RaveValue result = {LLVMBuildCall2(generator->builder, LLVMFunctionType(generator->genType(tfunc->main, -2), types.data(), types.size(), tfunc->isVarArg), fn.value, lArgs.data(), lArgs.size(), name), tfunc->main};

    for(int i : byVals) {
        LLVMAddCallSiteAttribute(result.value, i + 1, LLVMCreateTypeAttribute(generator->context, LLVMGetEnumAttributeKindForName("byval", 5), byValStructures[i]));
        LLVMAddCallSiteAttribute(result.value, i + 1, LLVMCreateEnumAttribute(generator->context, LLVMGetEnumAttributeKindForName("align", 5), 8));
    }

    return result;
}

// Wrapper for the LLVMConstInBoundsGEP2 function using RaveValue.
RaveValue LLVM::cInboundsGep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount) {
    return {LLVMConstInBoundsGEP2(generator->genType(ptr.type->getElType(), -1), ptr.value, indices, indicesCount), ptr.type->getElType()};
}

// Wrapper for the LLVMBuildGEP2 function using RaveValue.
RaveValue LLVM::gep(RaveValue ptr, LLVMValueRef* indices, unsigned int indicesCount, const char* name) {
    return {LLVMBuildGEP2(generator->builder, generator->genType(ptr.type->getElType(), -1), ptr.value, indices, indicesCount, name), new TypePointer(ptr.type->getElType())};
}

// Wrapper for the LLVMBuildStructGEP2 function using RaveValue.
RaveValue LLVM::structGep(RaveValue ptr, unsigned int idx, const char* name) {
    TypeStruct* ts = instanceof<TypePointer>(ptr.type) ? (TypeStruct*)ptr.type->getElType() : (TypeStruct*)ptr.type;

    return {LLVMBuildStructGEP2(
        generator->builder, generator->genType(ts, -1),
        ptr.value, idx, name
    ), new TypePointer(AST::structTable[ts->name]->getVariables()[idx]->getType())};
}

// Wrapper for the LLVMBuildAlloca function using RaveValue. Builds alloca at the first basic block for saving stack memory (C behaviour).
RaveValue LLVM::alloc(Type* type, const char* name) {
    LLVMPositionBuilder(generator->builder, LLVMGetFirstBasicBlock(generator->functions[currScope->funcName].value), LLVMGetFirstInstruction(LLVMGetFirstBasicBlock(generator->functions[currScope->funcName].value)));
    LLVMValueRef value = LLVMBuildAlloca(generator->builder, generator->genType(type, -1), name);
    LLVMPositionBuilderAtEnd(generator->builder, generator->currBB);
    return {value, new TypePointer(type)};
}

// Wrapper for the LLVMBuildArrayAlloca function using RaveValue.
RaveValue LLVM::alloc(RaveValue size, const char* name) {
    return {LLVMBuildArrayAlloca(generator->builder, LLVMInt8TypeInContext(generator->context), size.value, name), new TypePointer(new TypeBasic(BasicType::Char))};
}

// Enables/disables fast math.
void LLVM::setFastMath(LLVMBuilderRef builder, bool infs, bool nans, bool arcp, bool nsz) {
    llvm::FastMathFlags flags;
    if(infs) flags.setNoInfs(true);
    if(nans) flags.setNoNaNs(true);
    if(arcp) flags.setAllowReciprocal(true);
    if(nsz) flags.setNoSignedZeros(true);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}

// Enables/disables all flags of fast math.
void LLVM::setFastMathAll(LLVMBuilderRef builder, bool value) {
    llvm::FastMathFlags flags;
    flags.setFast(value);
    llvm::unwrap(builder)->setFastMathFlags(flags);
}

// Wrapper for the LLVMConstInt function.
LLVMValueRef LLVM::makeInt(size_t n, unsigned long long value, bool isUnsigned) {
    return LLVMConstInt(LLVMIntTypeInContext(generator->context, n), value, isUnsigned);
}

// Wrapper for LLVMConstArray function.
RaveValue LLVM::makeCArray(Type* ty, std::vector<RaveValue> values) {
    std::vector<LLVMValueRef> data;
    for(int i=0; i<values.size(); i++) data.push_back(values[i].value);
    return {LLVMConstArray(generator->genType(ty, -1), data.data(), data.size()), new TypeArray(new NodeInt(data.size()), ty)};
}

// Wrappers for LLVMAppendBasicBlockInContext
LLVMBasicBlockRef LLVM::makeBlock(std::string name, LLVMValueRef function) {
    return LLVMAppendBasicBlockInContext(generator->context, function, name.c_str());
}

LLVMBasicBlockRef LLVM::makeBlock(std::string name, std::string fName) {
    return LLVM::makeBlock(name, generator->functions[fName].value);
}

void LLVM::makeAsPointer(RaveValue& value) {
    if(LLVM::isLoad(value.value)) LLVM::undoLoad(value);
    else {
        // Make a temp variable
        RaveValue temp = LLVM::alloc(value.type, "temp");

        // Copy value to temp
        LLVMBuildStore(generator->builder, value.value, temp.value);

        // Replace value with temp
        value = temp;
    }
}