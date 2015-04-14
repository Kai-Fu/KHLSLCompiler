#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace SC_Prep {

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