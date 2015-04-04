#pragma once
#include <map>
#include <string>
#include <vector>

namespace SC {
	class Preprocessor
	{
	public:
		Preprocessor(const char* source);
		~Preprocessor();
	
	protected:
		void DoIt(const char* source);

	private:
		std::vector<char> mProcessedSource;
		std::string mErrMessage;
		std::map<std::string, std::string> mDefines;
	};
}