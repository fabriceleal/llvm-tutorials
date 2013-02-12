# Official tutorial for LLVM 2.8

## Kaleidoscope series
from http://llvm.org/docs/tutorial/

## Notes

http://llvm.org/releases/2.8/docs/tutorial/LangImpl1.html#language
The Lexer

http://llvm.org/releases/2.8/docs/tutorial/LangImpl2.html
The Parser and AST

http://llvm.org/releases/2.8/docs/tutorial/LangImpl3.html
Code generation to LLVM IR
Review, comment. There's a bug somewhere (the tut says so), try to fix it ...

http://llvm.org/releases/2.8/docs/tutorial/LangImpl4.html
Ask how do you say that to the JIT/optimizer that an operation is commutative.
For instance, the code generated for:
```
def test(x) (1+2+x)*(x+(1+2));
```
the code is optimized correctly, but this one
```
def test(x) (1+2+x)*(x+1+2);
```
doesnt.

http://llvm.org/releases/2.8/docs/tutorial/LangImpl5.html
Review, comment
