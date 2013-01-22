/*
 This tutorial implements the lexer / parser manually. Cool ... NOT
 */

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <stdio.h>
#include "ast.h"

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

  // Run the main "interpreter loop" now.
  MainLoop();

	TheModule->dump();

	return 0;
}
