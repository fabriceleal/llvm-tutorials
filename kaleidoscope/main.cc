/*
 This tutorial implements the lexer / parser manually. Cool ... NOT
 */
#include <llvm/DerivedTypes.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/PassManager.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Target/TargetData.h>
#include <llvm/Target/TargetSelect.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Support/IRBuilder.h>

#include <stdio.h>
#include "kaleidoscope.hpp"

extern std::map<char, int> KBinopPrecedence;
extern Module* TheModule;

FunctionPassManager *TheFPM;

ExecutionEngine *TheExecutionEngine;

int main(int argc, char** argv) {
	// Fill precedence table
	KBinopPrecedence['<'] = 10;
	KBinopPrecedence['+'] = 20;
	KBinopPrecedence['-'] = 20;
	KBinopPrecedence['*'] = 40; // higher

  fprintf(stderr, "ready> ");
  getNextToken();

	TheModule = new Module("cool jit", getGlobalContext());

	// Create a JIT. Taks ownership of the module
	TheExecutionEngine = EngineBuilder(TheModule).create();

	// Set up optimizing pipeline
	FunctionPassManager OurFPM(TheModule);
	// Simple "peephole" optimizations and bit-twiddling optzns
	OurFPM.add(new TargetData(*TheExecutionEngine->getTargetData()));
	// Simple "peephole" optimizations and bit-twiddling optzns.
  OurFPM.add(createInstructionCombiningPass());
  // Reassociate expressions.
  OurFPM.add(createReassociatePass());
  // Eliminate Common SubExpressions.
  OurFPM.add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  OurFPM.add(createCFGSimplificationPass());
  OurFPM.doInitialization();

  // Set the global so the code gen can use this.
  TheFPM = &OurFPM;

  // Run the main "interpreter loop" now.
  MainLoop();

	TheModule->dump();

	return 0;
}
