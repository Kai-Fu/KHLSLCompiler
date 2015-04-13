#pragma once
#include <map>
#include <string>
#include <vector>
#include <memory>

namespace SC_Prep {

	class Preprocessor
	{
	public:
		struct CodeLine
		{
			int loc;
			std::string code;

			CodeLine(int l) { loc = l; }
		};
		std::vector<CodeLine> mProcessedSource;
		std::string mErrMessage;

	public:
		Preprocessor();
		virtual ~Preprocessor();

		const char* GetErrorMessage() const { return mErrMessage.c_str(); }
		void WriteToSting(std::string& processedCode) const;
	protected:
		virtual void DoIt(const char* source) = 0;

	};

	class NoComments : public Preprocessor
	{
	protected:
		virtual void DoIt(const char* source);
	};

	class DefineHandler : public Preprocessor
	{
	protected:
		virtual void DoIt(const char* source);
	};
}