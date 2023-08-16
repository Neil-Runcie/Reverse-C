Purpose
Reverse-C is parody "programming language" made as a simple yet comical demonstration of the NDR Lexer and Parser Utility. Reverse-C is not a typical programming language as it does not actually need a lexical analysis utility nor a true parser since it only consists of 0s and 1s in multiples of eight. Furthermore, it does not compile into machine language but rather it tranpiles into C to then be compiled by a C compiler (GCC was used for development). With that being said, Reverse-C does effectively demonstrate the functionality provided by the NDR_LAP library which can be found here https://github.com/Neil-Runcie/NDR_LAP.git

How To Use
The project is set up to be built using cmake. After running cmake and make commands to build the project, the "reverse-c" executable will be present for use with a code file

Running the reverse-c transpiler from the command line can be done by passing the text file containing the code as the first parameter to the application such as "./reverse-c code.txt" on linux for example

Documentation
For  documentation of the Reverse-C language, open the instruction.html page in your browser
