#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
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

		struct MacroDefine {
			std::vector<Token> arguments;
			std::vector<Token> tokenSequence;
		};
		std::map<std::string, MacroDefine> mDefines;

		Tokenizer mTokenizer;
	};
}