// The AST
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl2.html

#ifndef DEF_KALEID_AST
#define DEF_KALEID_AST

//#include <llvm/DerivedTypes.h>
//#include <llvm/LLVMContext.h>
//#include <llvm/Module.h>
//#include <llvm/Analysis/Verifier.h>
#include <llvm/Support/IRBuilder.h>
#include <string>
#include <vector>
#include <map>

using namespace llvm;

int getNextToken();

// Base class for all expression nodes
// All values are double, so no need for "type" field
class ExprAST {
public:
	virtual ~ExprAST() {};
	virtual Value* Codegen() = 0;
};

// Number AST - for numberals like "1.0"
class NumberExprAST : public ExprAST {
	double Val;
public:
	NumberExprAST(double val) : Val(val) {}
	virtual Value* Codegen();
};

class VariableExprAST : public ExprAST {
	std::string Name;
public:
 VariableExprAST(const std::string &name) : Name(name) {};
	virtual Value* Codegen();
};

// for binary ops: + - * /
class BinaryExprAST : public ExprAST {
	char Op;
	ExprAST *LHS, *RHS;
public:
 BinaryExprAST(char op, ExprAST* lhs, ExprAST* rhs) :
	Op(op), LHS(lhs), RHS(rhs) {}

	virtual Value* Codegen();
};

// for function calls
class CallExprAST : public ExprAST {
	std::string Callee;
	std::vector<ExprAST*> Args;
 public:
 CallExprAST(const std::string &callee, std::vector<ExprAST*> &args) : Callee(callee), Args(args) {}
	virtual Value* Codegen();
};

class IfExprAST: public ExprAST {
	ExprAST *Cond, *Then, *Else;
public:
	IfExprAST(ExprAST *cond, ExprAST *then, ExprAST* _else) : 
		Cond(cond), Then(then), Else(_else) {}
	virtual Value *Codegen();
};

class ForExprAST : public ExprAST {
	std::string VarName;
  ExprAST *Start, *End, *Step, *Body;
public:
  ForExprAST(const std::string &varname, ExprAST *start, ExprAST *end,
             ExprAST *step, ExprAST *body)
    : VarName(varname), Start(start), End(end), Step(step), Body(body) {}
  virtual Value *Codegen();
};

// "prototype" or a function - 
// captures name, argument names (thus implicity the number of args)
class PrototypeAST {
	std::string Name;
	std::vector<std::string> Args;
 public:
 PrototypeAST(const std::string &name, const std::vector<std::string> &args) : Name(name), Args(args) {}

	Function* Codegen();
};

// Function definition
class FunctionAST {
	PrototypeAST* Proto;
	ExprAST* Body;
 public:
 FunctionAST(PrototypeAST* proto, ExprAST* body) : Proto(proto), Body(body) {}

	Function* Codegen();
};


ExprAST* Error(const char* Str);

PrototypeAST* ErrorP(const char* Str);

FunctionAST* ErrorF(const char* Str);

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

static int GetTokPrecedence();

// Top level parsing

void MainLoop();

#endif
