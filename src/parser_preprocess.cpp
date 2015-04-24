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
static const std::regex re_string_partten("\"(([^\"])|(\\\\.))+[^\\\\]\"");
static const std::regex re_start_string_partten("^\"(([^\"])|(\\\\.))+[^\\\\]\"");


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
		RE_Blank_Imp "(" RE_Token ")" RE_Blank_Imp ",");

	std::regex dollarPartten("\\$");

	std::regex leftParathesesPartten(
		RE_Blank_Imp "(\\()" RE_Blank_Imp );

	std::cmatch re;

	struct MacroDefine
	{
		std::string macroName;
		std::regex searchPartten;
		std::string replacePartten;
		std::vector<std::string> argumentList;
		bool isRedefined;
	};
	std::vector<MacroDefine> macroDefines;
	// Search map to check if the macro is re-define an existing one.
	std::map<std::string, size_t> defineRefs;
	while (1) {
		if (!std::regex_search(pCurParsingPtr, re, linePattern)) {
			mProcessedSource += pCurParsingPtr;
			break;
		}

		const char* lineStart = re[0].first;
		const char* lineEnd = re[0].second;
		pCurParsingPtr = lineEnd;

		if (std::regex_search(lineStart, lineEnd, re, defineStartPartten)) {

			MacroDefine newMacro;
			// This line is a macro definition
			if (std::regex_match(lineStart, lineEnd, re, defineParttenWithArg)) {
				// This line is a valid macro define with argument(s).
				//
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
				
				newMacro.macroName = macroName;
				newMacro.isRedefined = false;
				newMacro.searchPartten = std::regex(std::string("\\b") + newMacro.macroName + "\\b");
				for (auto& arg : argsList) {
					definedString = std::regex_replace(definedString, std::regex(std::string("\\b(##)?") + arg + "(##)?\\b"), std::string("##") + arg + "##");
				}
				newMacro.replacePartten = definedString;
				newMacro.argumentList = argsList;

			}
			else if (std::regex_match(lineStart, lineEnd, re, defineParttenWithoutArg)) {
				// This line is a valid macro define without argument
				//
				newMacro.macroName = re[1].str();
				newMacro.searchPartten = std::regex(std::string("\\b") + newMacro.macroName + "\\b");
				newMacro.replacePartten = re[2].str();
				newMacro.isRedefined = false;

				
			}

			auto it = defineRefs.find(newMacro.macroName);
			if (it != defineRefs.end()) {
				macroDefines[it->second].isRedefined = true;
			}
			
			defineRefs[newMacro.macroName] = macroDefines.size();
			macroDefines.push_back(newMacro);
			// Move to the next line
			mProcessedSource += "\n";
		}
		else {
			// This line is NOT macro definition, I need to handle the macro expanding.
			std::string lineString;
			size_t extraLines = 0;
			for (auto macro = macroDefines.rbegin(); macro != macroDefines.rend(); macro++) {
				if ((*macro).isRedefined)
					continue;

				if (macro->argumentList.empty()) {
					lineString.append(lineStart, lineEnd);
					lineString = Replace_ExcludingString(lineString, (*macro).searchPartten, (*macro).replacePartten);
				}
				else {
					while (std::regex_search(lineStart, lineEnd, re, (*macro).searchPartten)) {

						lineString.append(lineStart, re[0].first);
						// Handle the macro arguments
						//
						const char* macroArgStart = re[0].second;
						bool expendingSucceed = false;
						if (std::regex_search(macroArgStart, re, leftParathesesPartten)) {
							const char* macroArgStart_p = re[1].first;
							std::vector<std::string> argList;
							const char* macroExpandingEnd = GatherMacroArguments(macroArgStart_p, argList);
							std::string expanedMacro = (*macro).replacePartten;
							if (macroExpandingEnd != NULL && argList.size() == (*macro).argumentList.size()) {
								for (size_t i = 0; i < argList.size(); ++i) {
									std::regex tokenReplacePartten(std::string("##") + (*macro).argumentList[i] + "##");
									expanedMacro = std::regex_replace(expanedMacro, tokenReplacePartten, argList[i]);
								}

								extraLines += std::count(macroArgStart, macroExpandingEnd, '\n');
								pCurParsingPtr = macroExpandingEnd;
								lineStart = pCurParsingPtr;
								lineString.append(expanedMacro);
								expendingSucceed = true;
							}
							
						}
						
						if (!expendingSucceed) {
							mErrMessage = 
								std::string("Invalid macro arguments when trying to expanding: \"") +
								(*macro).macroName +
								"\"";
							return;
						}

					}
				}
			}
			mProcessedSource.append(lineString);
			for (size_t i = 0; i < extraLines; ++i) mProcessedSource.push_back('\n');
		}

	}
}

std::string SC_Prep::Replace_ExcludingString(const std::string & src, const std::regex & searchFor, const std::string& replaceBy)
{
	std::string result;
	const char* pStart = src.c_str();
	const char* pEnd = &src.back() + 1;
	std::cmatch re;

	bool bContinue = true;
	while (1) {
		const char* subEnd = pEnd;
		if (std::regex_search(pStart, re, re_string_partten)) {
			subEnd = re[0].first;
		}
		else{
			bContinue = false;
		}

		
		std::string subString = std::regex_replace(std::string(pStart, subEnd), searchFor, replaceBy);
		result.append(subString);

		if (!bContinue)
			break;
		result.append(re[0].first, re[0].second);
		pStart = re[0].second;
	}
	return result;
}

const char* SC_Prep::GatherMacroArguments(const char* src, std::vector<std::string>& outArgList)
{
	const char* pCur = src;
	int pairCnt = 0;
	std::string pendingArg;
	do {
		if (*pCur == '(')
			pairCnt++;
		else if (*pCur == ')')
			pairCnt--;

		if (*pCur == '\"') {
			std::string temp;
			pCur = ParseString(pCur, temp);
		}
		else if (*pCur == ',' && pairCnt == 1) {
			outArgList.push_back(std::move(pendingArg));
		}
		else if (pairCnt > 0) {
			pendingArg.push_back(*pCur);
		}

		pCur++;
	} while (*pCur != '\0' && pairCnt > 0);

	if (!pendingArg.empty())
		outArgList.push_back(std::move(pendingArg));

	return (pairCnt == 0 ? pCur : NULL);
}

const char * SC_Prep::ParseString(const char * src, std::string& outStr)
{
	const char* pStart = src;
	std::cmatch re;
	while (1) {
		const char* strEnd = NULL;
		if (std::regex_search(pStart, re, re_start_string_partten)) {
			strEnd = re[0].second;
			outStr.append(pStart, strEnd);
			pStart = strEnd;
		}
		else 
			break;

		while (*pStart == ' ' || *pStart == '\t' || *pStart == '\n')
			++pStart;
		
	}
	return pStart;
}
