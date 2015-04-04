#include "parser_preprocess.h"

using namespace SC;

Preprocessor::Preprocessor(const char* source)
{
	DoIt(source);
}

Preprocessor::~Preprocessor()
{

}

void Preprocessor::DoIt(const char* source)
{
	int curLine = 1;
	const char* pCurChar = source;
	std::string lineString;
	lineString.reserve(100);

	while (1) {
		int charCntInLine = 0;

		if (*pCurChar == '\0' || *pCurChar == '\n') {
			
		}


		if (*pCurChar == '\0')
			break;
		
	}

}