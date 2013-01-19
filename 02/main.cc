#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/PassManager.h>
#include <llvm/CallingConv.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>
#include <iostream>

/*
 This is our goal:

 unsigned gcd(unsigned x, unsigned y) {
	 if(x == y) {
		 return x;
	 } else if(x < y) {
		 return gcd(x, y -x);
	 } else {
		 return gcd(x - y, y);
	 }
 }

*/ 

using namespace llvm;
using namespace std;

Module* makeLLVMModule() {
	Module* mod = new Module("tut2");
	
	Constant* c = mod->getOrInsertFunction(
																				 "gcd",
																				 IntegerType::get(getGlobalContext(), 32),
																				 IntegerType::get(getGlobalContext(), 32),
																				 IntegerType::get(getGlobalContext(), 32),
																				 NULL);
	Function* gcd = cast<Function>(c);

	Function::arg_iterator args = gcd->arg_begin();
	Value* x = args++;
	x->setName("x");
	Value* y = args++;
	y->setName("y");

	// This example has branching, so we create a block for 
	// each "section" of code
	BasicBlock* entry = BasicBlock::Create("entry", gcd);
	BasicBlock* ret = BasicBlock::Create("return", gcd);
	BasicBlock* cond_false = BasicBlock::Create("cond_false", gcd);
	BasicBlock* cond_true = BasicBlock::Create("cond_true", gcd);
	BasicBlock* cond_false_2 = BasicBlock::Create("cond_false_2", gcd);

	// ** entry **
	IRBuilder<> builder(entry);
	// Integer comparison for equality, returns a 1 bit integer result
	Value* xEqualsY = builder.CreateICmpEQ(x, y, "tmp");
	builder.CreateCondBr(xEqualsY, ret, cond_false);
	
	// ** ret **
	builder.SetInsertPoint(ret);
	builder.CreateRet(x);
	
	// ** cond_false **
	builder.SetInsertPoint(cond_false);
	// Integer comparison for unsigned less-than
	Value* xLessThanY = builder.CreateICmpULT(x, y, "tmp");
	builder.CreateCondBr(xLessThanY, cond_true, cond_false_2);

	// ** cond_true **
	builder.SetInsertPoint(cond_true);
	Value* yMinusX = builder.CreateSub(y, x, "tmp");
	std::vector<Value*> args1;
	args1.push_back(x);
	args1.push_back(yMinusX);
	Value* recur_1 = builder.CreateCall(gcd, args1.begin(), args1.end(), "tmp");
	builder.CreateRet(recur_1);

	// ** cond_false_2 **
	builder.SetInsertPoint(cond_false_2);
	Value* xMinusY = builder.CreateSub(x, y, "tmp");
	std::vector<Value*> args2;
	args2.push_back(xMinusY);
	args2.push_back(y);
	Value* recur_2 = builder.CreateCall(gcd, args2.begin(), args2.end(), "tmp");
	builder.CreateRet(recur_2);

	return mod;
}

int main(int argc, char** argv) {
	Module* Mod = makeLLVMModule();

	verifyModule(*Mod, PrintMessageAction);

	PassManager PM;
	PM.add(createPrintModulePass(&outs()));
	PM.run(*Mod);

	delete Mod;

	return 0;
}
