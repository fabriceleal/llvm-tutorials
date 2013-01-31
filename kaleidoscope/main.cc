/*
 This tutorial implements the lexer / parser manually. Cool ... NOT
 */

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <stdio.h>
#include "kaleidoscope.hpp"

extern std::map<char, int> KBinopPrecedence;
extern Module* TheModule;

int main(int argc, char** argv) {
	// Fill precedence table
	KBinopPrecedence['<'] = 10;
	KBinopPrecedence['+'] = 20;
	KBinopPrecedence['-'] = 20;
	KBinopPrecedence['*'] = 40; // higher

  fprintf(stderr, "ready> ");
  getNextToken();

	TheModule = new Module("cool jit", getGlobalContext());

	// Create passes (for optimizations)
	// TODO http://llvm.org/releases/2.8/docs/tutorial/LangImpl4.html

  // Run the main "interpreter loop" now.
  MainLoop();

	TheModule->dump();

	return 0;
}
