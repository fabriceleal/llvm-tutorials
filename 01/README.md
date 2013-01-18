# LLVM Tutorial 1: A First Function
from http://llvm.org/releases/2.6/docs/tutorial/JITTutorial1.html

## Lessons

* We need a boatload of flags to compile. These flags can be obtained with the output of:

```
llvm-config --cppflags --ldflags --libs core
```

* If we're not carefull, we'll get seg faults.

* The tutorial didn't work verbatim. Thats because it uses llvm 2.6 while I'm using 2.8
