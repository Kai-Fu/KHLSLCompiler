#include "parser_preprocess.h"

using namespace SC;

Preprocessor::Preprocessor(const char* source) :
	mTokenizer(source)
{
	DoIt();
}

Preprocessor::~Preprocessor()
{

}

void Preprocessor::DoIt()
{
	mCurProcessedLine = 1;
	mProcessedSource.clear();

	while (1) {

		Token curT = mTokenizer.GetNextToken();
		std::map<std::string, std::string>::iterator it_defiine;
		if (curT.IsEqual("#")) {
			if (mTokenizer.PeekNextToken(0).IsEqual("define")) {
				mTokenizer.GetNextToken();
				// Handle the macro definition

			}
		}
		else if ((it_defiine = mDefines.find(curT.ToStdString())) != mDefines.end()) {
			// Handle the macro expanding

		}
		else {
			AddProcessedToken(curT);
		}
		if (curT.IsEOF())
			break;
	}

}

void SC::Preprocessor::AddProcessedToken(const Token & t)
{
	while (t.GetLOC() > mCurProcessedLine) {
		mProcessedSource += "\n";
		mCurProcessedLine++;
	}
	mProcessedSource += t.ToStdString();
}
