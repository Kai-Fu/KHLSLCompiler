#include "parser_preprocess.h"
#include "parser_tokenizer.h"

using namespace SC_Prep;

Preprocessor::Preprocessor() 
{
	
}

Preprocessor::~Preprocessor()
{

}

void SC_Prep::Preprocessor::WriteToSting(std::string & processedCode) const
{
	size_t len = 0;
	for (auto& line : mProcessedSource) 
		len += line.code.length() + 1;
	
	processedCode.reserve(len);
	int curLine = 1;
	for (auto& line : mProcessedSource) {
		while (curLine++ < line.loc) {
			processedCode += "\n";
		}
		processedCode += line.code;
		processedCode += "\n";
	}

}

void NoComments::DoIt(const char* source)
{
	const char* pCurParsingPtr = source;
	int parsedCodeLine = 1;
	
	while (1) {
		const char* beforeSkip = pCurParsingPtr;
		// Eat the white spaces and new line characters.
		//
		while (*pCurParsingPtr == '\n' || *pCurParsingPtr == '\t') {
			if (*pCurParsingPtr == '\n')
				mProcessedSource.push_back(CodeLine(parsedCodeLine++));
			else
				mProcessedSource.back().code += *pCurParsingPtr;
		}

		// Skip the comments, e.g. //... and /* ... */
		//
		if (SC::IsFirstN_Equal(pCurParsingPtr, "/*")) {

			pCurParsingPtr += 2;
			// Seek for the end of the comments(*/)
			bool isLastAsterisk = false;
			while (*pCurParsingPtr != '\0') {
				if (isLastAsterisk && *pCurParsingPtr == '/')
					break;
				if (*pCurParsingPtr == '*')
					isLastAsterisk = true;
				if (*pCurParsingPtr == '\n')
					mProcessedSource.push_back(CodeLine(parsedCodeLine++));
				pCurParsingPtr++;
			}
			if (*pCurParsingPtr == '\0') {
				mErrMessage = "Comments not ended - unexpected end of file.";
				return;
			}
		
			continue;
		}

		// For comments starting with // ...
		//
		if (SC::IsFirstN_Equal(pCurParsingPtr, "//")) {
			pCurParsingPtr += 2;
			// Go to the end of the line
			while (*pCurParsingPtr != '\0' && *pCurParsingPtr != '\n')
				++pCurParsingPtr;

			if (*pCurParsingPtr == '\n') {
				mProcessedSource.push_back(CodeLine(parsedCodeLine++));
				pCurParsingPtr++;
			}
			continue;
		}

		// Process the "\" token at the end of line
		//
		if (SC::IsFirstN_Equal(pCurParsingPtr, "//\n")) {
			pCurParsingPtr += 2;
			parsedCodeLine++;
			continue;
		}

		// Break from this loop since the pointer doesn't move forward
		if (beforeSkip == pCurParsingPtr)
			break;

		mProcessedSource.back().code += *pCurParsingPtr;
	}
}

void SC_Prep::DefineHandler::DoIt(const char * source)
{
}
