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
static std::map<std::string, AllocaInst*> NamedValues;

extern FunctionPassManager *TheFPM;

static AllocaInst* CreateEntryBlockAlloca(Function* TheFunction, 
																					const std::string &VarName) {
	IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
									 TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(Type::getDoubleTy(getGlobalContext()), 0,
                           VarName.c_str());
}

/// CreateArgumentAllocas - Create an alloca for each argument and register the
/// argument in the symbol table so that references to it will succeed.
void PrototypeAST::CreateArgumentAllocas(Function *F) {
	Function::arg_iterator AI = F->arg_begin();
  for (unsigned Idx = 0, e = Args.size(); Idx != e; ++Idx, ++AI) {
    // Create an alloca for this variable.
    AllocaInst *Alloca = CreateEntryBlockAlloca(F, Args[Idx]);

    // Store the initial value into the alloca.
    Builder.CreateStore(AI, Alloca);

    // Add arguments to variable symbol table.
    NamedValues[Args[Idx]] = Alloca;
  }
}

Value* ErrorV(const char* Str) {
	Error(Str);
	return 0;
}

Value* NumberExprAST::Codegen() {
	return ConstantFP::get(getGlobalContext(), APFloat(Val));
}

Value* VariableExprAST::Codegen() {
	Value* V = NamedValues[Name];
	if(V == 0) ErrorV("Unknown variable name");

	// Load value
	return Builder.CreateLoad(V, Name.c_str());
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
	if(Op == '=') {
		// Assignment requires the LHS to be an identifier.
    VariableExprAST *LHSE = dynamic_cast<VariableExprAST*>(LHS);
    if (!LHSE)
      return ErrorV("destination of '=' must be a variable");

		// Codegen the RHS.
    Value *Val = RHS->Codegen();
    if (Val == 0) return 0;

    // Look up the name.
    Value *Variable = NamedValues[LHSE->getName()];
    if (Variable == 0) return ErrorV("Unknown variable name");

    Builder.CreateStore(Val, Variable);
    return Val;
	}

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

Value *VarExprAST::Codegen() {
	std::vector<AllocaInst *> OldBindings;
  
  Function *TheFunction = Builder.GetInsertBlock()->getParent();

  // Register all variables and emit their initializer.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second;
		// Emit the initializer before adding the variable to scope, this prevents
    // the initializer from referencing the variable itself, and permits stuff
    // like this:
    //  var a = 1 in
    //    var a = a in ...   # refers to outer 'a'.
    Value *InitVal;
    if (Init) {
      InitVal = Init->Codegen();
      if (InitVal == 0) return 0;
    } else { // If not specified, use 0.0.
      InitVal = ConstantFP::get(getGlobalContext(), APFloat(0.0));
    }
    
    AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder.CreateStore(InitVal, Alloca);

    // Remember the old variable binding so that we can restore the binding when
    // we unrecurse.
    OldBindings.push_back(NamedValues[VarName]);
    
    // Remember this binding.
    NamedValues[VarName] = Alloca;
  }

	// Codegen the body, now that all vars are in scope.
  Value *BodyVal = Body->Codegen();
  if (BodyVal == 0) return 0;

	// Pop all our variables from scope.
  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];

  // Return the body computation.
  return BodyVal;
}

Value* ForExprAST::Codegen() {	
	// Make the new basic block for the loop header, inserting after current
  // block.
  Function *TheFunction = Builder.GetInsertBlock()->getParent();

	// Create an alloca for the variable in the entry block.
  AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);

	// Emit the start code first, without 'variable' in scope.
  Value *StartVal = Start->Codegen();
  if (StartVal == 0) return 0;
	
	// Store the value into the alloca.
  Builder.CreateStore(StartVal, Alloca);

  //BasicBlock *PreheaderBB = Builder.GetInsertBlock();
  BasicBlock *LoopBB = BasicBlock::Create(getGlobalContext(), "loop", TheFunction);
  
  // Insert an explicit fall through from the current block to the LoopBB.
  Builder.CreateBr(LoopBB);

	// Start insertion in LoopBB.
  Builder.SetInsertPoint(LoopBB);
  
  // Start the PHI node with an entry for Start.
  //PHINode *Variable = Builder.CreatePHI(Type::getDoubleTy(getGlobalContext()), 
	//																			VarName.c_str());
	
  //Variable->addIncoming(StartVal, PreheaderBB);
		
  // Within the loop, the variable is defined equal to the PHI node.  If it
  // shadows an existing variable, we have to restore it, so save it now.
  AllocaInst *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Alloca;
  
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
  
  //Value *NextVar = Builder.CreateFAdd(Variable, StepVal, "nextvar");

	// Compute the end condition.
  Value *EndCond = End->Codegen();
  if (EndCond == 0) return EndCond;

	// Reload, increment, and restore the alloca.  This handles the case where
  // the body of the loop mutates the variable.
  Value *CurVar = Builder.CreateLoad(Alloca);
  Value *NextVar = Builder.CreateFAdd(CurVar, StepVal, "nextvar");
  Builder.CreateStore(NextVar, Alloca);
  
  // Convert condition to a bool by comparing equal to 0.0.
  EndCond = Builder.CreateFCmpONE(EndCond, 
																	ConstantFP::get(getGlobalContext(), APFloat(0.0)),
                                  "loopcond");

	// Create the "after loop" block and insert it.
  //BasicBlock *LoopEndBB = Builder.GetInsertBlock();
  BasicBlock *AfterBB = BasicBlock::Create(getGlobalContext(), "afterloop", TheFunction);
  
  // Insert the conditional branch into the end of LoopEndBB.
  Builder.CreateCondBr(EndCond, LoopBB, AfterBB);
  
  // Any new code will be inserted in AfterBB.
  Builder.SetInsertPoint(AfterBB);
	
  // Add a new entry to the PHI node for the backedge.
  //Variable->addIncoming(NextVar, LoopEndBB);
  
  // Restore the unshadowed variable.
  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);
  
  // for expr always returns 0.0.
  return Constant::getNullValue(Type::getDoubleTy(getGlobalContext()));
}


/*
// old version, before mutable variables
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
}*/

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
		
		//NamedValues[Args[Idx]] = AI;
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

  // Add all arguments to the symbol table and create their allocas.
  Proto->CreateArgumentAllocas(TheFunction);

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

  if(Proto->isBinaryOp())
    KBinopPrecedence.erase(Proto->getOperatorName());

	return 0;
}
