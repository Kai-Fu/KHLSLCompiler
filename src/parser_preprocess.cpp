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

		Token curT = mTokenizer.PeekNextToken(0);
		
		if (curT.IsEqual("#define")) {
			
		}
		else {
			AddProcessedToken(curT);
			mTokenizer.GetNextToken();
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

bool SC::Preprocessor::HandleMacroDefine()
{
	Token t0 = mTokenizer.PeekNextToken(0);
	Token t1 = mTokenizer.PeekNextToken(1);
	if (t0.GetLOC() != t1.GetLOC()) {
		mErrMessage = "Expect identifier after #define.";
		return false;
	}

	if (!t0.IsEqual("#define") || t1.GetType() != Token::kIdentifier) {
		mErrMessage = "Invalid macro define.";
		return false;
	}

	mTokenizer.GetNextToken();
	mTokenizer.GetNextToken();

	Token t3 = mTokenizer.PeekNextToken(0);
	if (t3.GetLOC() != t1.GetLOC()) {
		// This macro is empty #define
		mDefines[t1.ToStdString()] = "";
		return true;
	}
	else {
		std::string macroString;
		while (t3.GetLOC() == t1.GetLOC()) {
			macroString += t3.ToStdString();
			t3 = mTokenizer.GetNextToken();
		}
		mDefines[t1.ToStdString()] = macroString;
		return true;
	}
}

