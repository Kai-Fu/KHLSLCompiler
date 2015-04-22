#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <regex>

namespace SC_Prep {

	// This function replace the partten defined by "searchFor" argument in the input "src" string. 
	// It defers from std::regex_replace in that it does excluding the matches within strings(if there exists)
	// starting and ending with quotes(but not the quote following a backslash, just like strings in C++).
	//
	std::string Replace_ExcludingString(const std::string& src, const std::regex& searchFor, const std::string& replaceBy);

	const char* GatherMacroArguments(const char* src, std::vector<std::string>& outArgList);

	const char* ParseString(const char* src, std::string& outStr);

	class Preprocessor
	{
	public:
		std::string mProcessedSource;
		std::string mErrMessage;

	public:
		Preprocessor();
		virtual ~Preprocessor();

	
		virtual void DoIt(const char* source) = 0;

	};

	class NoComments : public Preprocessor
	{
	public:
		virtual void DoIt(const char* source);
	};

	class NoMultiLines : public Preprocessor
	{
	public:
		virtual void DoIt(const char* source);
	};

	class DefineHandler : public Preprocessor
	{
	public:
		virtual void DoIt(const char* source);
	};
}