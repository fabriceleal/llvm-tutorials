#include <vector>
#include "ast.h"

// AST
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl2.html

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
  if (ParseDefinition()) {
    fprintf(stderr, "Parsed a function definition.\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleExtern() {
  if (ParseExtern()) {
    fprintf(stderr, "Parsed an extern\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
}

static void HandleTopLevelExpression() {
  // Evaluate a top-level expression into an anonymous function.
  if (ParseTopLevelExpr()) {
    fprintf(stderr, "Parsed a top-level expr\n");
  } else {
    // Skip token for error recovery.
    getNextToken();
  }
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
