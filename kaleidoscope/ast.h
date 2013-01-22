// The AST
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl2.html
#include <string>
#include <vector>
#include <map>
#include "lexer.h"

#ifndef DEF_KALEID_AST
#define DEF_KALEID_AST

// simple token buffer.
// all functions should assume that the token that 
// needs to be parsed is CurTok
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}


// Base class for all expression nodes
// All values are double, so no need for "type" field
class ExprAST {
public:
	virtual ~ExprAST() {};
};

// Number AST - for numberals like "1.0"
class NumberExprAST : public ExprAST {
	double Val;
public:
	NumberExprAST(double val) : Val(val) {}
};

class VariableExprAST : public ExprAST {
	std::string Name;
public:
 VariableExprAST(const std::string &name) : Name(name) {};
};

// for binary ops: + - * /
class BinaryExprAST : public ExprAST {
	char Op;
	ExprAST *LHS, *RHS;
public:
 BinaryExprAST(char op, ExprAST* lhs, ExprAST* rhs) :
	Op(op), LHS(lhs), RHS(rhs) {}
};

// for function calls
class CallExprAST : public ExprAST {
	std::string Callee;
	std::vector<ExprAST*> Args;
 public:
 CallExprAST(const std::string &callee, std::vector<ExprAST*> &args) : Callee(callee), Args(args) {}
};

// "prototype" or a function - 
// captures name, argument names (thus implicity the number of args)
class PrototypeAST : public ExprAST {
	std::string Name;
	std::vector<std::string> Args;
 public:
 PrototypeAST(const std::string &name, const std::vector<std::string> &args) : Name(name), Args(args) {}
};

// Function definition
class FunctionAST : public ExprAST {
	PrototypeAST* Proto;
	ExprAST* Body;
 public:
 FunctionAST(PrototypeAST* proto, ExprAST* body) : Proto(proto), Body(body) {}
};

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

// Basic Expression Parsing

// numberexpr :: = number
static ExprAST* ParseNumberExpr();

// parenexpr ::= '(' expression ')'
static ExprAST* ParseParenExpr();

// identifierexpr
//    ::= identifier
//    ::= identifier '(' expression* ')'
static ExprAST *ParseIdentifierExpr();

static ExprAST* ParsePrimary();

static ExprAST* ParseExpression();

static ExprAST* ParseBinOpRHS(int ExprPrec, ExprAST *LHS);

static PrototypeAST* ParsePrototype();

static FunctionAST* ParseDefinition();

static PrototypeAST* ParseExtern();

static FunctionAST* ParseTopLevelExpr();

// Binary Expression Parsing

// Filled in main
std::map<char, int> KBinopPrecedence;

static int GetTokPrecedence();

// Top level parsing

void MainLoop();

#endif
