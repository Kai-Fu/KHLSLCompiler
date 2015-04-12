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
			HandleMacroDefine();
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
	int codeLine = t0.GetLOC();
	mTokenizer.GetNextToken();
	mTokenizer.GetNextToken();

	Token t3 = mTokenizer.PeekNextToken(0);
	if (t3.GetLOC() != codeLine) {
		// This macro is empty #define
		mDefines[t1.ToStdString()].arguments.clear();
		mDefines[t1.ToStdString()].tokenSequence.clear();
		return true;
	}
	else {
		std::vector<Token> arguments;
		std::vector<Token> tokenSequence;
		
		mTokenizer.GetNextToken();
		if (mTokenizer.PeekNextToken(0).IsEqual("(")) {
			// Macro with arguments
			mTokenizer.GetNextToken();

			while (1) {
				Token arg = mTokenizer.GetNextToken();
				Token commaOrEnd = mTokenizer.GetNextToken();

				if (arg.GetLOC() != codeLine || commaOrEnd.GetLOC() != codeLine) {
					mErrMessage = "Expect argument of #define.";
					return false;
				}

				if (arg.GetType() != Token::kIdentifier) {
					mErrMessage = "Expect identifier after #define.";
					return false;
				}
				if (!commaOrEnd.IsEqual(",") && !commaOrEnd.IsEqual(")")) {
					mErrMessage = "Invalid argument of #define.";
					return false;
				}
				arguments.push_back(arg);

				if (commaOrEnd.IsEqual(")"))
					break;
			}

		}

		while (mTokenizer.PeekNextToken(0).GetLOC() == codeLine &&
			!mTokenizer.PeekNextToken(0).IsEOF()) {
			tokenSequence.push_back(mTokenizer.GetNextToken());
		}

		mDefines[t1.ToStdString()].arguments = arguments;
		mDefines[t1.ToStdString()].tokenSequence = tokenSequence;

		return true;
	}
}

