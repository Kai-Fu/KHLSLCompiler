#pragma once
#include <map>
#include <string>
#include <vector>
#include "parser_tokenizer.h"

namespace SC {
	class Preprocessor
	{
	public:
		Preprocessor(const char* source);
		~Preprocessor();
	
	protected:
		void DoIt();
		void AddProcessedToken(const Token& t);

		bool HandleMacroDefine();
	private:
		std::string mProcessedSource;
		int mCurProcessedLine;
		std::string mErrMessage;
		std::map<std::string, std::string> mDefines;

		Tokenizer mTokenizer;
	};
}