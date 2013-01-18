#include <llvm/LLVMContext.h>
#include <llvm/Module.h>
#include <llvm/Function.h>
#include <llvm/PassManager.h>
#include <llvm/CallingConv.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Assembly/PrintModulePass.h>
#include <llvm/Support/IRBuilder.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

// *The* ultimate function
int mul_add(int x, int y, int z) {
	return x * y + z;
}

Module* makeLLVMModule();

int main(int argc, char** argv) {
	// Create a module. A module contains
	// global vars, fucntion decl and implementations
	Module* Mod = makeLLVMModule();
	
	// Run verifier
	verifyModule(*Mod, PrintMessageAction);

	// Managing optimizations; schedules/invokes/.. passes
	PassManager PM;
	PM.add(createPrintModulePass(&outs()));
	PM.run(*Mod);

	delete Mod;

	return 0;
}

Module* makeLLVMModule() {
	// Module construction
	// Create a module and give it a name
	Module* mod = new Module("test", getGlobalContext());
	
	// Start constructing function; name, return type, arg types
	// This wil return a cast of the existing function if it already
	// exists with another prototype. Since, thats not the case,
	// we can just cast c to Function*
	Constant* c = mod->getOrInsertFunction(
																				 "mul_add", 
																				 IntegerType::get(getGlobalContext(), 32),
																				 IntegerType::get(getGlobalContext(), 32),
																				 IntegerType::get(getGlobalContext(), 32),
																				 NULL);
	Function* mul_add = cast<Function>(c);
	
	// Set calling convention to be the C calling convention
	// Ensures that will interop properly with C
	// Also, give names to the parameters; its cute
	Function::arg_iterator args = mul_add->arg_begin();
	Value* x = args++;
	x->setName("x");
	Value* y = args++;
	y->setName("y");
	Value* z = args++;
	z->setName("z");

	// At this point, we have a function
	// However, it doesnt have a body ... yet
	// To create a body, we need to use blocks
	// IRBuilder will simplify the job, 
	// but exists a more sophisticated interface
	BasicBlock* block = BasicBlock::Create(getGlobalContext(), "entry", mul_add);
	IRBuilder<> builder(block);
	Value* tmp = builder.CreateBinOp(Instruction::Mul, x, y, "tmp");
	Value* tmp2 = builder.CreateBinOp(Instruction::Add, tmp, x, "tmp2");
	builder.CreateRet(tmp2);

	return mod;
}
