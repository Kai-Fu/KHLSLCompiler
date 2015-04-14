#include "parser_preprocess.h"
#include "parser_tokenizer.h"
#include <regex>

using namespace SC_Prep;

Preprocessor::Preprocessor() 
{
	
}

Preprocessor::~Preprocessor()
{

}


void NoComments::DoIt(const char* source)
{
	const char* pCurParsingPtr = source;
	std::string pass0Result;
	// ignore comments in /* bla bla */
	{
		std::regex commentPattern("(/\\*([^*]|(\\*+[^*/]))*\\*+/)");
		std::cmatch re;
		while (1) {
			if (!std::regex_search(pCurParsingPtr, re, commentPattern)) {
				pass0Result += pCurParsingPtr;
				break;
			}
			int newLineCnt = (int)std::count(re[0].first, re[0].second, '\n');
			pass0Result.append(pCurParsingPtr, re[0].first);
			while (newLineCnt-- > 0)
				pass0Result += "\n";
			pCurParsingPtr = re[0].second;
		}
	}
	// ignore comments after // ...
	{
		pCurParsingPtr = pass0Result.c_str();
		std::regex commentPattern("//.*$");
		std::cmatch searchResult;
		mProcessedSource = std::regex_replace(pass0Result, commentPattern, "");
	}
	
}

void SC_Prep::NoMultiLines::DoIt(const char * source)
{
	const char* pCurParsingPtr = source;

	std::regex linePattern("^.*\n");
	std::regex multiLinePattern("\\\\[:blank:]*$");
	std::cmatch re;
	int pendNewline = -1;
	bool isLastMultiLine = false;
	while (1) {
		if (!std::regex_search(pCurParsingPtr, re, linePattern)) {
			mProcessedSource += pCurParsingPtr;
			break;
		}
		pendNewline++;

		const char* lineStart = re[0].first;
		const char* lineEnd = re[0].second;
		pCurParsingPtr = lineEnd;

		if (std::regex_search(re[0].first, re[0].second, re, multiLinePattern)) {
			// This line is not ended
			mProcessedSource.append(lineStart, re[0].first);
			isLastMultiLine = true;
		}
		else {
			mProcessedSource.append(lineStart, lineEnd);
			isLastMultiLine = false;
		}

		if (!isLastMultiLine) {
			while (pendNewline-- > 0)
				mProcessedSource += "\n";
		}

	}
}

void SC_Prep::DefineHandler::DoIt(const char * source)
{
}

