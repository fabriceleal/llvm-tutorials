// Code generation
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl3.html

#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include "ast.h"


static Module* TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

Value* ErrorV(const char* Str) {
	Error(Str);
	return 0;
}

Value* NumberExprAST::Codegen() {
	return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

Value* VariableExprAST::Codegen() {
	Value* V = NamedValues[Name];
	return V ? V : ErrorV("Unknown variable name");
}

Value* BinaryExprAST::Codegen() {
	Value* L = LHS->Codegen();
	Value* R = RHS->Codegen();

	if(L == 0 || R == 0) {
		return 0;
	}

	switch(Op) {
	case '+': return Builder.CreateFAdd(L, R, "addtmp");
	case '-': return Builder.CreateFSub(L, R, "subtmp");
	case '*': return Builder.CreateFMul(L, R, "multmp");
	case '<':
		L = Builder.CreateFCmpULT(L, R, "cmptmp");
		return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()), 
														 "booltmp");
	default: return ErrorV("Invalid binary operator");
	}
}

Value* CallExprAST::Codegen() {
	Function* CalleeF = TheModule->getFunction(Callee);
	if(CalleeF == 0) {
		return ErrorV("Unknown function reference");
	}

	if(CalleeF->arg_size() != Args.size()) {
		return ErrorV("Incorrect # arguments passed");
	}

	std::vector<Value*> ArgsV;
	for(unsigned i = 0, e = Args.size(); i != e; ++i) {
		ArgsV.push_back(Args[i]->Codegen());
		if(ArgsV.back() == 0) {
			return 0;
		}
	}
	return Builder.CreateCall(CalleeF, ArgsV.begin(), ArgsV.end(), "calltmp");
}

// Function code generation
Function* PrototypeAST::Codegen() {
	std::vector<const Type*> Doubles(Args.size(), 
																	 Type::getDoubleTy(getGlobalContext()));
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(getGlobalContext(),
																												 Doubles,
																												 false));
	Function* F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

	if(F->getName() != Name) {
		F->eraseFromParent();
		F = TheModule->getFunction(Name);
	}

	if(!F->empty()) {
		ErrorF("redefinition of function");
		return 0;
	}

	if(F->arg_size() != Args.size()) {
		ErrorF("redefinition of function with different # args");
		return 0;
	}

	unsigned Idx = 0;
	for(Function::arg_iterator AI = F->arg_begin();
			Idx != Args.size();
			++AI, ++Idx) {
		//--
		AI->setName(Args[Idx]);
		
		NamedValues[Args[Idx]] = AI;
	}

	return F;
}
