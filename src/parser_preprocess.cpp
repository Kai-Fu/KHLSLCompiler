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
		while (1) {
			if (!std::regex_search(pCurParsingPtr, searchResult, commentPattern)) {
				mProcessedSource += pCurParsingPtr;
				break;
			}

			mProcessedSource.append(pCurParsingPtr, searchResult[0].first);
			mProcessedSource += "\n";
			pCurParsingPtr = searchResult[0].second;
		}
	}
	
}

void SC_Prep::DefineHandler::DoIt(const char * source)
{
}
