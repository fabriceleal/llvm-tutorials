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

#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <cstdlib>
#include "kaleidoscope.hpp"

// CODE GENERATION
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl3.html

Module* TheModule;
static IRBuilder<> Builder(getGlobalContext());
static std::map<std::string, Value*> NamedValues;

extern FunctionPassManager *TheFPM;

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

Value* UnaryExprAST::Codegen() {
	Value* OperandV = Operand->Codegen();
	if(OperandV == 0)
		return 0;

	Function* F = TheModule->getFunction(std::string("unary") + Opcode);
	if(F == 0) {
		return ErrorV("Unknown unary operator");
	}

	return Builder.CreateCall(F, OperandV, "unop");
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
		// Convert bool false/true to 0.0/1.0
		return Builder.CreateUIToFP(L, Type::getDoubleTy(getGlobalContext()), 
																"booltmp");
	default: break; //return ErrorV("Invalid binary operator");
	}
	
	// If it wasnt a builtin binary operator, is must be a user defined one
	// emit code to call it
	Function *F = TheModule->getFunction(std::string("binary") + Op);
	assert(F && "binary operator not found");
	
	//Value* Ops[2] = { L, R };
	//return Builder.CreateCall(F, Ops, "binop");
	return Builder.CreateCall2(F, L, R, "binop");
}

Value* IfExprAST::Codegen() {
	//	return (Value*) 0;
	Value *CondV = Cond->Codegen();
  if (CondV == 0) return 0;
  
  // Convert condition to a bool by comparing equal to 0.0.
  CondV = Builder.CreateFCmpONE(CondV, 
																ConstantFP::get(getGlobalContext(), APFloat(0.0)),
                                "ifcond");
	
	Function *TheFunction = Builder.GetInsertBlock()->getParent();
  
  // Create blocks for the then and else cases.  Insert the 'then' block at the
  // end of the function.
  BasicBlock *ThenBB = BasicBlock::Create(getGlobalContext(), "then", TheFunction);
  BasicBlock *ElseBB = BasicBlock::Create(getGlobalContext(), "else");
  BasicBlock *MergeBB = BasicBlock::Create(getGlobalContext(), "ifcont");

  Builder.CreateCondBr(CondV, ThenBB, ElseBB);
	
  // Emit then value.
  Builder.SetInsertPoint(ThenBB);
  
  Value *ThenV = Then->Codegen();
  if (ThenV == 0) return 0;
  
  Builder.CreateBr(MergeBB);
  // Codegen of 'Then' can change the current block, update ThenBB for the PHI.
  ThenBB = Builder.GetInsertBlock();

	// Emit else block.
  TheFunction->getBasicBlockList().push_back(ElseBB);
  Builder.SetInsertPoint(ElseBB);
  
  Value *ElseV = Else->Codegen();
  if (ElseV == 0) return 0;
  
  Builder.CreateBr(MergeBB);
  // Codegen of 'Else' can change the current block, update ElseBB for the PHI.
  ElseBB = Builder.GetInsertBlock();
	
  // Emit merge block.
  TheFunction->getBasicBlockList().push_back(MergeBB);
  Builder.SetInsertPoint(MergeBB);
  PHINode *PN = Builder.CreatePHI(Type::getDoubleTy(getGlobalContext()),
                                  "iftmp");
  
  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}

Value* ForExprAST::Codegen() {
	// Emit the start code first, without 'variable' in scope.
  Value *StartVal = Start->Codegen();
  if (StartVal == 0) return 0;
	
	// Make the new basic block for the loop header, inserting after current
  // block.
  Function *TheFunction = Builder.GetInsertBlock()->getParent();
  BasicBlock *PreheaderBB = Builder.GetInsertBlock();
  BasicBlock *LoopBB = BasicBlock::Create(getGlobalContext(), "loop", TheFunction);
  
  // Insert an explicit fall through from the current block to the LoopBB.
  Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
  Builder.SetInsertPoint(LoopBB);
  
  // Start the PHI node with an entry for Start.
  PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(getGlobalContext()), VarName.c_str());
  Variable->addIncoming(StartVal, PreheaderBB);
		
  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  Value *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Variable;
  
  // Emit the body of the loop.  This, like any other expr, can change the
  // current BB.  Note that we ignore the value computed by the body, but don't
  // allow an error.
  if (Body->Codegen() == 0)
    return 0;
	
	// Emit the step value.
  Value *StepVal;
  if (Step) {
    StepVal = Step->Codegen();
    if (StepVal == 0) return 0;
  } else {
    // If not specified, use 1.0.
    StepVal = ConstantFP::get(getGlobalContext(), APFloat(1.0));
  }
  
  Value *NextVar = Builder.CreateFAdd(Variable, StepVal, "nextvar");

	// Compute the end condition.
  Value *EndCond = End->Codegen();
  if (EndCond == 0) return EndCond;
  
  // Convert condition to a bool by comparing equal to 0.0.
  EndCond = Builder.CreateFCmpONE(EndCond, 
																	ConstantFP::get(getGlobalContext(), APFloat(0.0)),
                                  "loopcond");

	// Create the "after loop" block and insert it.
  BasicBlock *LoopEndBB = Builder.GetInsertBlock();
  BasicBlock *AfterBB = BasicBlock::Create(getGlobalContext(), "afterloop", TheFunction);
  
  // Insert the conditional branch into the end of LoopEndBB.
  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
  
  // Any new code will be inserted in AfterBB.
  Builder.SetInsertPoint(AfterBB);
	
  // Add a new entry to the PHI node for the backedge.
  Variable->addIncoming(NextVar, LoopEndBB);
  
  // Restore the unshadowed variable.
  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);
  
  // for expr always returns 0.0.
  return Constant::getNullValue(Type::getDoubleTy(getGlobalContext()));
	//return 0;
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
	FunctionType* FT = FunctionType::get(Type::getDoubleTy(getGlobalContext()),
																												 Doubles,
																												 false);
	Function* F = Function::Create(FT, Function::ExternalLinkage, Name, TheModule);

	if(F->getName() != Name) {
		F->eraseFromParent();
		F = TheModule->getFunction(Name);

		if(!F->empty()) {
			ErrorF("redefinition of function");
			return 0;
		}

		if(F->arg_size() != Args.size()) {
			ErrorF("redefinition of function with different # args");
			return 0;
		}
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

extern std::map<char, int> KBinopPrecedence;

Function* FunctionAST::Codegen() {
	NamedValues.clear();
	
	Function* TheFunction = Proto->Codegen();
	if(TheFunction == 0) {
		return 0;
	}

	// If this is an operator, install it
	if(Proto->isBinaryOp()) {
		KBinopPrecedence[Proto->getOperatorName()] = Proto->getBinaryPrecedence();
	}

	// Create a new basic block to start insert into.
	BasicBlock* BB = BasicBlock::Create(getGlobalContext(), "entry", TheFunction);
	Builder.SetInsertPoint(BB);

	if(Value* RetVal = Body->Codegen()) {
		Builder.CreateRet(RetVal);

		// Validate (check consistency)
		verifyFunction(*TheFunction);

		// optimize function!
		fprintf(stderr, "Optimizing function ...\n");
		TheFPM->run(*TheFunction);
		fprintf(stderr, "Function optimized...\n");

		return TheFunction;
	}

	TheFunction->eraseFromParent();
	return 0;
}
