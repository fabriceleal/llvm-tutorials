#include <string>
#include <stdio.h>
#include <cstdlib>

// LEXER
// from http://llvm.org/releases/2.8/docs/tutorial/LangImpl1.html

enum Token {
	tok_eof = -1,

	// commands
	tok_def = -2, tok_extern = -3,

	// primary
	tok_identifier = -4, tok_number = -5
};

// global vars; tutorial says this is not pretty :)
static std::string IdentifierStr; // if tok_identifier
static double NumVal; // if tok_number

// the actual lexer
static int gettok() {
	static int LastChar = ' ';
	
	// skip whitespace
	while(isspace(LastChar)) {
		// Reads from stdin
		LastChar = getchar();		
	}

	// identifier [a-zA-Z][a-zA-Z0-9]*
	if(isalpha(LastChar)) {
		IdentifierStr = LastChar;
		while(isalnum((LastChar = getchar()))) {
			IdentifierStr += LastChar;
		}

		if(IdentifierStr == "def") {
			return tok_def;
		}
		if(IdentifierStr == "extern") {
			return tok_extern;
		}

		return tok_identifier;
	}

	// Number: [0-9.]+
	if(isdigit(LastChar) || LastChar == '.') {
		std::string NumStr;
		do {
			NumStr += LastChar;
			LastChar = getchar();
		} while(isdigit(LastChar) || LastChar == '.');

		NumVal = strtod(NumStr.c_str(), 0);
		return tok_number;
	}
	
	// Comment until end of the line
	if(LastChar == '#') {
		do {
			LastChar = getchar();
		} while(LastChar != EOF && LastChar != '\n' && LastChar != '\r');

		if(LastChar != EOF) {
			return gettok();
		}
	}

	if(LastChar == EOF) {
		return tok_eof;
	}

	int ThisChar = LastChar;
	LastChar = getchar();
	return ThisChar;
}

// simple token buffer.
// all functions should assume that the token that 
// needs to be parsed is CurTok
static int CurTok;
static int getNextToken() {
	return CurTok = gettok();
}
