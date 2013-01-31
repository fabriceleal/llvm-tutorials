#include <llvm/DerivedTypes.h>
#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <cstdlib>
#include "kaleidoscope.hpp"

// Filled in main
std::map<char, int> KBinopPrecedence;

// LEXER
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl1.html

enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2, tok_extern = -3,

	// primary
	tok_identifier = -4, tok_number = -5
};

// removed static so its visible outside this header

// global vars; tutorial says this is not pretty :)
std::string IdentifierStr; // if tok_identifier
double NumVal; // if tok_number

// the actual lexer
int gettok() {
	static int LastChar = ' ';
	
	// skip whitespace
	while(isspace(LastChar)) {
		// Reads from stdin
		LastChar = getchar();		
	}

	// identifier [a-zA-Z][a-zA-Z0-9]*
	if(isalpha(LastChar)) {
		IdentifierStr = LastChar;
		while(isalnum((LastChar = getchar()))) {
			IdentifierStr += LastChar;
		}

		if(IdentifierStr == "def") {
			return tok_def;
		}
		if(IdentifierStr == "extern") {
			return tok_extern;
		}

		return tok_identifier;
	}

	// Number: [0-9.]+
	if(isdigit(LastChar) || LastChar == '.') {
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while(isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}
	
	// Comment until end of the line
	if(LastChar == '#') {
		do {
			LastChar = getchar();
		} while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if(LastChar != EOF) {
			return gettok();
		}
	}

	if(LastChar == EOF) {
		return tok_eof;
	}

	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

// simple token buffer.
// all functions should assume that the token that 
// needs to be parsed is CurTok
static int CurTok;
int getNextToken() {
	return CurTok = gettok();
}

// AST
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl2.html

// Error routines
// This is not the most sofisticated error handling one can have,
// but its useful enough
ExprAST* Error(const char* Str) {
	fprintf(stderr, "Error: %s\n", Str);
	return 0;
}

PrototypeAST* ErrorP(const char* Str) {
	Error(Str);
	return 0;
}

FunctionAST* ErrorF(const char* Str) {
	Error(Str);
	return 0;
}

static ExprAST* ParseNumberExpr() {
	ExprAST* Result = new NumberExprAST(NumVal);
	// Consume number
	getNextToken();
	return Result;
}

static ExprAST* ParseParenExpr() {
	getNextToken(); // eat (
	ExprAST* V = ParseExpression();
	if(!V) return 0;
	if(CurTok != ')') {
		return Error("expected ')'");
	}
	getNextToken(); // eat )
	return V;
}

static ExprAST *ParseIdentifierExpr() {
	std::string IdName = IdentifierStr;
	
	getNextToken(); // eat identifier

	if(CurTok != '(') { // simple var ref
		return new VariableExprAST(IdName);
	}

	// call
	getNextToken(); // eat (

	std::vector<ExprAST*> Args;
	if(CurTok != ')') {
		while(1) {
			ExprAST* Arg = ParseExpression();
			if(!Arg) return 0;
			Args.push_back(Arg);

			if(CurTok == ')') break;

			if(CurTok != ',')
				return Error("Expected ')' or ',' in argument list");
			
			getNextToken(); // eat ,
		}
	}	

	// Eat the )
	getNextToken();

	return new CallExprAST(IdName, Args);
}

static ExprAST* ParsePrimary() {
	switch(CurTok) {
	default: return Error("unknown token when expecting an expression");
	case tok_identifier:return ParseIdentifierExpr();
	case tok_number: return ParseNumberExpr();
	case '(': return ParseParenExpr();
	}
}

static int GetTokPrecedence() {
	if(!isascii(CurTok)) {
		return -1;
	}

	int TokPrec = KBinopPrecedence[CurTok];
	if(TokPrec <= 0) {
		return -1;
	}

	return TokPrec;
}

static ExprAST* ParseExpression() {
	ExprAST *LHS = ParsePrimary();
	if(!LHS) return 0;

	return ParseBinOpRHS(0, LHS);
}

static ExprAST* ParseBinOpRHS(int ExprPrec, ExprAST *LHS) {
	// if its a binop, find its precedence
	while(1) {
		int TokPrec = GetTokPrecedence();
		if(TokPrec < ExprPrec)
			return LHS;

		// this is a binop
		int BinOp = CurTok;
		getNextToken(); // eat binop

		// parse primary expression after binary operation
		ExprAST* RHS = ParsePrimary();
		if(!RHS) return 0;

		// if binop binds less tightly with RHS than the operator
		// after RHS, let the pending operator take RHS as its LHS
		int NextPrec = GetTokPrecedence();
		if(TokPrec < NextPrec) {
			RHS = ParseBinOpRHS(TokPrec + 1, RHS);
			if(RHS == 0) return 0;
		}

		LHS = new BinaryExprAST(BinOp, LHS, RHS);
	}
}

// Parsing the Rest

static PrototypeAST* ParsePrototype() {
	if(CurTok != tok_identifier) {
		return ErrorP("Expected function name in prototype");
	}

	std::string FnName = IdentifierStr;
	getNextToken();

	if(CurTok != '(') {
		return ErrorP("Expected '(' in prototype");
	}

	std::vector<std::string> ArgNames;
	while(getNextToken() == tok_identifier) {
		ArgNames.push_back(IdentifierStr);
	}

	if(CurTok != ')') {
		return ErrorP("Expected ')' in prototype");
	}

	getNextToken();

	return new PrototypeAST(FnName, ArgNames);
}

// definition ::= 'def' prototype expression
static FunctionAST* ParseDefinition() {
	getNextToken();
	PrototypeAST* Proto = ParsePrototype();
	if(Proto == 0) return 0;

	if(ExprAST* E = ParseExpression()) {
		return new FunctionAST(Proto, E);
	}

	return 0;
}

// extern ::= 'extern' prototype
static PrototypeAST* ParseExtern() {
	getNextToken();
	return ParsePrototype();
}

// toplevelexpr ::= expression
static FunctionAST* ParseTopLevelExpr() {
	if(ExprAST* E = ParseExpression()) {
		PrototypeAST* Proto = new PrototypeAST("", std::vector<std::string>());
		return new FunctionAST(Proto, E);
	}
	return 0;
}

// TOP LEVEL PARSING

// These were copy-pasted. meh.
static void HandleDefinition() {
  if (FunctionAST* F = ParseDefinition()) {
		if(Function* LF = F->Codegen()) {
			fprintf(stderr, "Parsed a function definition.\n");
			LF->dump();
		}
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (PrototypeAST* P = ParseExtern()) {
		if(Function* F = P->Codegen()) {
			fprintf(stderr, "Parsed an extern\n");
			F->dump();
		}
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (FunctionAST* F = ParseTopLevelExpr()) {
		if(Function* LF = F->Codegen()) {
			fprintf(stderr, "Parsed a top-level expr\n");
			LF->dump();
		}
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

extern "C" double putchard(double X) {
	putchar((char) X);
	return 0.0;
}

void MainLoop() {
	while(1) {
		fprintf(stderr, "ready> ");
		switch(CurTok) {
		case tok_eof: return;
		case ';': getNextToken(); break;
		case tok_def: HandleDefinition(); break;
		case tok_extern: HandleExtern(); break;
		default: HandleTopLevelExpression(); break;
		}
	}
}
