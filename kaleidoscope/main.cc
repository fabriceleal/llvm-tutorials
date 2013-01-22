/*
 This tutorial implements the lexer / parser manually. Cool ... NOT
 */

#include <stdio.h>
#include "ast.h"

extern std::map<char, int> KBinopPrecedence;

int main(int argc, char** argv) {
	// Fill precedence table
	KBinopPrecedence['<'] = 10;
	KBinopPrecedence['+'] = 20;
	KBinopPrecedence['-'] = 20;
	KBinopPrecedence['*'] = 40; // higher

  fprintf(stderr, "ready> ");
  getNextToken();

  // Run the main "interpreter loop" now.
  MainLoop();

	return 0;
}
