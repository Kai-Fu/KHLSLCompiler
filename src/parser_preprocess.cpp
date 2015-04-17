#include "parser_preprocess.h"
#include "parser_tokenizer.h"
#include <regex>

using namespace SC_Prep;

#define RE_Token "[[:alpha:]_]\\w*"
#define RE_P_Start "\\("
#define RE_P_End "\\)"
#define RE_NOT_P_End "[^\\)]"
#define RE_Slash "\\\\"
#define RE_Blank_Exp "[[:blank:]]+"
#define RE_Blank_Imp "[[:blank:]]*"

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
		mProcessedSource = std::regex_replace(pass0Result, commentPattern, "");
	}
	
}

void SC_Prep::NoMultiLines::DoIt(const char * source)
{
	const char* pCurParsingPtr = source;

	std::regex linePattern("^.*\n");
	std::regex multiLinePattern("\\\\[[:blank:]]*$");
	std::cmatch re;
	int pendNewline = -1;
	bool isLastMultiLine = false;
	while (1) {
		if (!std::regex_search(pCurParsingPtr, re, linePattern)) {
			mProcessedSource += pCurParsingPtr;
			break;
		}
		pendNewline++;

		const char* lineStart = re[0].first;
		const char* lineEnd = re[0].second;
		pCurParsingPtr = lineEnd;

		if (std::regex_search(re[0].first, re[0].second, re, multiLinePattern)) {
			// This line is not ended
			mProcessedSource.append(lineStart, re[0].first);
			isLastMultiLine = true;
		}
		else {
			mProcessedSource.append(lineStart, lineEnd);
			isLastMultiLine = false;
		}

		if (!isLastMultiLine) {
			while (pendNewline-- > 0)
				mProcessedSource += "\n";
		}

	}
}


void SC_Prep::DefineHandler::DoIt(const char * source)
{
	const char* pCurParsingPtr = source;

	std::regex linePattern("^.*\n");

	std::regex defineStartPartten("^" RE_Blank_Imp "#define" RE_Blank_Exp);

	std::regex defineParttenWithArg(
		"#define" RE_Blank_Exp "(" RE_Token ")" RE_Blank_Imp 
		RE_P_Start "(" RE_NOT_P_End"+" ")" RE_P_End RE_Blank_Imp "(.*)\n");

	std::regex defineParttenWithoutArg(
		"#define" RE_Blank_Exp "(" RE_Token ")" RE_Blank_Exp "(.*)\n");

	std::regex argExtractingPartten(
		RE_Blank_Imp "(" RE_Token ")" RE_Blank_Imp "," );

	//std::regex defineParttenWithArg("#define[[:blank:]]+.*$");
	std::cmatch re;

	struct MacroDefine
	{
		std::vector<std::string> arguments;
		std::string definedString;
	};
	std::map<std::string, MacroDefine> defineMap;
	while (1) {
		if (!std::regex_search(pCurParsingPtr, re, linePattern)) {
			mProcessedSource += pCurParsingPtr;
			break;
		}

		const char* lineStart = re[0].first;
		const char* lineEnd = re[0].second;
		pCurParsingPtr = lineEnd;

		if (std::regex_search(lineStart, lineEnd, re, defineStartPartten)) {
			// This line is a macro definition
			if (std::regex_match(lineStart, lineEnd, re, defineParttenWithArg)) {

				std::string macroName = re[1].str();
				std::string definedString = re[3].str();

				std::string args = re[2].str() + ",";
				std::vector<std::string> argsList;
				const char* argString = args.c_str();
				while (std::regex_search(argString, re, argExtractingPartten)) {
					argString = re[0].second;
					argsList.push_back(re[1].str());
				}

				if (*argString != '\0') {
					mErrMessage = "Invalid macro define arguments.";
					return;
				}

				defineMap[macroName].definedString = definedString;
				defineMap[macroName].arguments = argsList;
			}
			else if (std::regex_match(lineStart, lineEnd, re, defineParttenWithoutArg)) {
				// This line is a valid macro define
				defineMap[re[1].str()].definedString = re[2].str();
			}

			// Move to the next line
			mProcessedSource += "\n";
		}
		else {
			// This line is NOT macro definition, I need to handle the macro expanding.

			mProcessedSource.append(lineStart, lineEnd);
		}

	}
}
