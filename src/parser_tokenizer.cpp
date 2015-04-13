#include "parser_tokenizer.h"
#include "parser_defines.h"
namespace SC {

	Token Token::sInvalid = Token(NULL, 0, 0, Token::kUnknown);
	Token Token::sEOF = Token(NULL, -1, -1, Token::kUnknown);

	static std::hash_map<std::string, TypeDesc> s_BuiltInTypes;
	static std::hash_map<std::string, KeyWord> s_KeyWords;

	void Initialize_Tokenizer()
	{
		s_BuiltInTypes["float"] = TypeDesc(kFloat, 1, false);
		s_BuiltInTypes["float2"] = TypeDesc(kFloat2, 2, false);
		s_BuiltInTypes["float3"] = TypeDesc(kFloat3, 3, false);
		s_BuiltInTypes["float4"] = TypeDesc(kFloat4, 4, false);
		s_BuiltInTypes["float8"] = TypeDesc(kFloat8, 8, false);

		s_BuiltInTypes["int"] = TypeDesc(kInt, 1, true);
		s_BuiltInTypes["int2"] = TypeDesc(kInt2, 2, true);
		s_BuiltInTypes["int3"] = TypeDesc(kInt3, 3, true);
		s_BuiltInTypes["int4"] = TypeDesc(kInt4, 4, true);
		s_BuiltInTypes["int8"] = TypeDesc(kInt8, 8, true);

		s_BuiltInTypes["bool"] = TypeDesc(kBoolean, 1, true);
		s_BuiltInTypes["bool2"] = TypeDesc(kBoolean2, 2, true);
		s_BuiltInTypes["bool3"] = TypeDesc(kBoolean3, 3, true);
		s_BuiltInTypes["bool4"] = TypeDesc(kBoolean4, 4, true);
		s_BuiltInTypes["bool8"] = TypeDesc(kBoolean8, 8, true);
		s_BuiltInTypes["void"] = TypeDesc(kVoid, 0, true);

		int machine_opt_width = KSC_GetSIMDWidth();
		//s_BuiltInTypes["float_n"] = TypeDesc(VarType(kFloat + machine_opt_width - 1), machine_opt_width, false);
		//s_BuiltInTypes["int_n"] = TypeDesc(VarType(kInt + machine_opt_width - 1), machine_opt_width, false);
		//s_BuiltInTypes["bool_n"] = TypeDesc(VarType(kBoolean + machine_opt_width - 1), machine_opt_width, false);

		s_KeyWords["struct"] = kStructDef;
		s_KeyWords["if"] = kIf;
		s_KeyWords["else"] = kElse;
		s_KeyWords["for"] = kFor;
		s_KeyWords["return"] = kFor;
		s_KeyWords["true"] = kTrue;
		s_KeyWords["false"] = kFalse;
		s_KeyWords["extern"] = kFalse;
	}

	void Finish_Tokenizer()
	{
		s_BuiltInTypes.clear();
		s_KeyWords.clear();
	}


	bool IsBuiltInType(const Token& token, TypeDesc* out_type)
	{
		char tempString[MAX_TOKEN_LENGTH];
		token.ToAnsiString(tempString);
		std::hash_map<std::string, TypeDesc>::iterator it = s_BuiltInTypes.find(tempString);
		if (it != s_BuiltInTypes.end()) {
			if (out_type) *out_type = it->second;
			return true;
		}
		else
			return false;
	}

	bool IsKeyWord(const Token& token, KeyWord* out_key)
	{
		char tempString[MAX_TOKEN_LENGTH];
		token.ToAnsiString(tempString);
		std::hash_map<std::string, KeyWord>::iterator it = s_KeyWords.find(tempString);
		if (it != s_KeyWords.end()) {
			if (out_key) *out_key = it->second;
			return true;
		}
		else
			return false;
	}



	static bool _isAlpha(char ch)
	{
		return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
	}

	static bool _isNumber(char ch)
	{
		return (ch >= '0' && ch <= '9');
	}

	bool IsFirstN_Equal(const char* test_str, const char* dest)
	{
		int i = 0;
		while (test_str[i] != '\0' && dest[i] != '\0') {
			if (test_str[i] != dest[i])
				return false;
			++i;
		}

		if (test_str[i] == '\0' && dest[i] != '\0')
			return false;

		return true;
	}

	Token::Token()
	{
		*this = sInvalid;
	}

	Token::Token(const char* p, int num, int line, Type tp)
	{
		mpData = p;
		mNumOfChar = num;
		mLOC = line;
		mType = tp;
	}

	Token::Token(const Token& ref)
	{
		mpData = ref.mpData;
		mNumOfChar = ref.mNumOfChar;
		mLOC = ref.mLOC;
		mType = ref.mType;
	}

	double Token::GetConstValue() const
	{
		if (mType == kConstInt || mType == kConstFloat) {
			char tempString[MAX_TOKEN_LENGTH + 1];
			memcpy(tempString, mpData, mNumOfChar*sizeof(char));
			tempString[mNumOfChar] = '\0';
			return atof(tempString);
		}
		return 0.0;
	}

	int Token::GetBinaryOpLevel() const
	{
		if (mType != kBinaryOp)
			return 0;

		if (mNumOfChar == 1) {
			switch (*mpData) {
			case '|':
			case '&':
			case '+':
			case '-':
				return 100;
			case '*':
			case '/':
				return 200;
			case '=':
				return 50;
			case '?':
				return 300;
			}
		}

		if (IsEqual("||") || IsEqual("&&"))
			return 70;
		else if (IsEqual("==") || IsEqual("!=") || IsEqual(">=") || IsEqual(">") || IsEqual("<=") || IsEqual("<"))
			return 80;

		return 0;
	}

	Token::Type Token::GetType() const
	{
		return mType;
	}

	int Token::GetLOC() const
	{
		return mLOC;
	}

	bool Token::IsValid() const
	{
		return (mpData != NULL);
	}

	bool Token::IsEOF() const
	{
		return (mNumOfChar == -1);
	}

	bool Token::IsEqual(const char* dest) const
	{
		if (dest == NULL)
			return false;
		size_t len = strlen(dest);
		if (len != mNumOfChar)
			return false;
		for (int i = 0; i < mNumOfChar; ++i) {
			if (dest[i] != mpData[i])
				return false;
		}
		return true;
	}

	bool Token::IsEqual(const Token& dest) const
	{
		size_t len = dest.mNumOfChar;
		if (len != mNumOfChar)
			return false;
		for (int i = 0; i < mNumOfChar; ++i) {
			if (dest.mpData[i] != mpData[i])
				return false;
		}
		return true;
	}

	void Token::ToAnsiString(char* dest) const
	{
		int i = 0;
		for (; i < mNumOfChar; ++i)
			dest[i] = mpData[i];
		dest[i] = '\0';
	}

	const char* Token::GetRawData() const
	{
		return mpData;
	}

	std::string Token::ToStdString() const
	{
		char tempString[MAX_TOKEN_LENGTH + 1];
		ToAnsiString(tempString);
		return std::string(tempString);
	}

	Tokenizer::Tokenizer(const char * content)
	{
		Reset(content);
	}

	Tokenizer::~Tokenizer()
	{
	}

	void Tokenizer::Reset(const char * content)
	{
		mBufferedToken.clear();
		mContentPtr = content;
		mCurParsingPtr = mContentPtr;
		mCurParsingLOC = 1;
	}

	Token Tokenizer::ScanForToken(std::string & errorMsg)
	{
		// First skip white space characters and comments
		//
		while (1) {
			const char* beforeSkip = mCurParsingPtr;
			// Eat the white spaces and new line characters.
			//
			while (*mCurParsingPtr == ' ' || *mCurParsingPtr == '\n' || *mCurParsingPtr == '\t') {
				if (*mCurParsingPtr == '\n')
					++mCurParsingLOC;
				++mCurParsingPtr;
			}

			// Skip the comments, e.g. //... and /* ... */
			//
			if (*mCurParsingPtr == '/') {

				if (*(mCurParsingPtr + 1) == '*') {
					mCurParsingPtr += 2;
					// Seek for the end of the comments(*/)
					bool isLastAsterisk = false;
					while (*mCurParsingPtr != '\0') {
						if (isLastAsterisk && *mCurParsingPtr == '/')
							break;
						if (*mCurParsingPtr == '*')
							isLastAsterisk = true;
						if (*mCurParsingPtr == '\n')
							++mCurParsingLOC;
						++mCurParsingPtr;
					}
					if (*mCurParsingPtr == '\0') {
						errorMsg = "Comments not ended - unexpected end of file.";
						return Token::sEOF;
					}
					else {
						++mCurParsingPtr; // Skip "/"
					}
				}
				else if (*(mCurParsingPtr + 1) == '/') {
					mCurParsingPtr += 2;
					// Go to the end of the line
					while (*mCurParsingPtr != '\0' && *mCurParsingPtr != '\n')
						++mCurParsingPtr;

					if (*mCurParsingPtr == '\n') {
						mCurParsingLOC++;
						++mCurParsingPtr;
					}
				}
			}

			// Break from this loop since the pointer doesn't move forward
			if (beforeSkip == mCurParsingPtr)
				break;
		}

		// Then read the string value
		// 
		/*if (*mCurParsingPtr == '"') {
			mCurParsingPtr++; // Skip the starting " token

			std::string newString;
			while (1) {
				if (*mCurParsingPtr != '\\' && *mCurParsingPtr != '"') {
					newString += *mCurParsingPtr;
					mCurParsingPtr++;
				}
				else if (*mCurParsingPtr == '\\') {
					mCurParsingPtr++;
					switch (*mCurParsingPtr) {
					case 'n':
						mCurParsingLOC++;
						break;
					case ' ':
						break;
					default:
						newString += *mCurParsingPtr;
						break;
					}
					mCurParsingPtr++;
				}
				else {
					mCurParsingPtr++;
					break;
				}
			}

			return Token(&newString[0], (int)newString.length(), mCurParsingLOC, Token::kString);
		}*/

		Token ret = Token::sInvalid;
		if (*mCurParsingPtr == '\0')
			return Token::sEOF;  // Reach the end of the file

								 // Now it is expecting a token.
								 //
		if (IsFirstN_Equal(mCurParsingPtr, "++") ||
			IsFirstN_Equal(mCurParsingPtr, "--") ||
			IsFirstN_Equal(mCurParsingPtr, "||") ||
			IsFirstN_Equal(mCurParsingPtr, "&&") ||
			IsFirstN_Equal(mCurParsingPtr, "==") ||
			IsFirstN_Equal(mCurParsingPtr, "!=") ||
			IsFirstN_Equal(mCurParsingPtr, ">=") ||
			IsFirstN_Equal(mCurParsingPtr, "<=")) {

			ret = Token(mCurParsingPtr, 2, mCurParsingLOC, Token::kBinaryOp);
			mCurParsingPtr += 2;
		}
		else if (IsFirstN_Equal(mCurParsingPtr, "+") ||
			IsFirstN_Equal(mCurParsingPtr, "-") ||
			IsFirstN_Equal(mCurParsingPtr, "*") ||
			IsFirstN_Equal(mCurParsingPtr, "/") ||
			IsFirstN_Equal(mCurParsingPtr, "|") ||
			IsFirstN_Equal(mCurParsingPtr, "&") ||
			IsFirstN_Equal(mCurParsingPtr, "=") ||
			IsFirstN_Equal(mCurParsingPtr, ">") ||
			IsFirstN_Equal(mCurParsingPtr, "<")) {

			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kBinaryOp);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '{') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenCurly);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '}') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseCurly);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '[') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenBraket);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == ']') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseBraket);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '(') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenParenthesis);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == ')') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseParenthesis);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == ',') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kComma);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == ';') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kSemiColon);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '.') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kPeriod);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '!') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kUnaryOp);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == ':') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kQuotation);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '?') {
			ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kBinaryOp);
			++mCurParsingPtr;
		}
		else if (*mCurParsingPtr == '#') {
			if (IsFirstN_Equal(mCurParsingPtr, "#define")) {
				size_t len = strlen("#define");
				ret = Token(mCurParsingPtr, (int)len, mCurParsingLOC, Token::kSharpDefine);
				mCurParsingPtr += len;
			}
			else if (IsFirstN_Equal(mCurParsingPtr, "##")) {
				ret = Token(mCurParsingPtr, 2, mCurParsingLOC, Token::kDoubleSharp);
				mCurParsingPtr += 2;
			}
			else {
				ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kDoubleSharp);
				++mCurParsingPtr;
			}
		}
		else {
			// Now handling for constants for identifiers
			bool isFirstCharNumber = _isNumber(*mCurParsingPtr);
			if (!isFirstCharNumber && !_isAlpha(*mCurParsingPtr) && *mCurParsingPtr != '_') {
				// Unrecoginzed character
				return Token(mCurParsingPtr++, 1, mCurParsingLOC, Token::kUnknown);
			}

			const char* pFirstCh = mCurParsingPtr;
			int idLen = 0;
			while (*mCurParsingPtr != '\0' &&
				(_isAlpha(*mCurParsingPtr) ||
				_isNumber(*mCurParsingPtr) ||
				*mCurParsingPtr == '_')) {
				idLen++;
				++mCurParsingPtr;
			}
			bool hasNonNumber = false;
			for (int i = 0; i < idLen; ++i) {
				if (!_isNumber(pFirstCh[i]))
					hasNonNumber = true;
			}

			if (isFirstCharNumber && hasNonNumber) {
				errorMsg = "Invalid identifier - ";
				errorMsg.append(pFirstCh, idLen);
				return Token(NULL, 0, mCurParsingLOC, Token::kUnknown);
			}

			bool isFloat = false;
			if (isFirstCharNumber) {
				// check for decimal point, e.g. 123.456e-8f
				if (*mCurParsingPtr == '.') {
					isFloat = true;
					mCurParsingPtr++;
					while (_isNumber(*mCurParsingPtr))
						mCurParsingPtr++;
				}

				if (*mCurParsingPtr == 'e' || *mCurParsingPtr == 'E') {
					mCurParsingPtr++;

					if (*mCurParsingPtr == '+' || *mCurParsingPtr == '-')
						mCurParsingPtr++;

					while (_isNumber(*mCurParsingPtr))
						mCurParsingPtr++;
				}

				if (*mCurParsingPtr == 'f' || *mCurParsingPtr == 'F')
					mCurParsingPtr++;
			}

			ret = Token(pFirstCh, int(mCurParsingPtr - pFirstCh), mCurParsingLOC, isFirstCharNumber ?
				(isFloat ? Token::kConstFloat : Token::kConstInt) :
				Token::kIdentifier);
		}

		return ret;
	}

	Token Tokenizer::GetNextToken()
	{
		Token ret = PeekNextToken(0);
		if (ret.IsValid()) {
			mBufferedToken.erase(mBufferedToken.begin());
		}
		return ret;
	}

	bool Tokenizer::IsEOF() const
	{
		return (*mCurParsingPtr == '\0');
	}

	Token Tokenizer::PeekNextToken(int next_i)
	{
		int charParsed = 0;
		int lineParsed = 0;

		int tokenNeeded = 0;
		if ((int)mBufferedToken.size() <= next_i)
			tokenNeeded = next_i - (int)mBufferedToken.size() + 1;

		for (int i = 0; i < tokenNeeded; ++i) {
			Token ret = ScanForToken(mErrorMessage);

			if (ret.IsValid()) {
				mBufferedToken.push_back(ret);
			}
			else {
				break;
			}
		}

		if ((int)mBufferedToken.size() > next_i) {
			std::list<Token>::iterator it = mBufferedToken.begin();
			int itCnt = next_i;
			while (itCnt-- > 0) ++it;
			return *it;
		}
		else
			return mErrorMessage.empty() ? Token::sEOF : Token::sInvalid;
	}


} // namespace SC
