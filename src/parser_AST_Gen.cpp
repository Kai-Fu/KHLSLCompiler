#include "parser_AST_Gen.h"
#include <stdio.h>
#include <assert.h>

namespace SC {

Token Token::sInvalid = Token(NULL, 0, 0, Token::kUnknown);
Token Token::sEOF = Token(NULL, -1, -1, Token::kUnknown);

static std::hash_map<std::string, TypeDesc> s_BuiltInTypes;
static std::hash_map<std::string, KeyWord> s_KeyWords;

struct TypeDesc {
	VarType type;
	int elemCnt;
	bool isInt;

	TypeDesc() {type = VarType::kInvalid; elemCnt = 0; isInt = false;}
	TypeDesc(VarType tp, int cnt, bool i) {type = tp; elemCnt = cnt; isInt = i;}
};

void Initialize_AST_Gen()
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
	s_BuiltInTypes["float_n"] = TypeDesc(VarType(kFloat + machine_opt_width - 1), machine_opt_width, false);
	s_BuiltInTypes["int_n"] = TypeDesc(VarType(kInt + machine_opt_width - 1), machine_opt_width, false);
	s_BuiltInTypes["bool_n"] = TypeDesc(VarType(kBoolean + machine_opt_width - 1), machine_opt_width, false);

	s_KeyWords["struct"] = kStructDef;
	s_KeyWords["if"] = kIf;
	s_KeyWords["else"] = kElse;
	s_KeyWords["for"] = kFor;
	s_KeyWords["return"] = kFor;
	s_KeyWords["true"] = kTrue;
	s_KeyWords["false"] = kFalse;
	s_KeyWords["extern"] = kFalse;
}

void Finish_AST_Gen()
{
	s_BuiltInTypes.clear();
	s_KeyWords.clear();
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
		char tempString[MAX_TOKEN_LENGTH+1];
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

static bool _isAlpha(char ch)
{
	return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z'));
}

static bool _isNumber(char ch)
{
	return (ch >= '0' && ch <= '9');
}

static bool _isFirstN_Equal(const char* test_str, const char* dest)
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

Token CompilingContext::ScanForToken(std::string& errorMsg)
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

	// Then read the constant string value
	// 
	if (*mCurParsingPtr == '"') {
		mCurParsingPtr++; // Skip the starting " token
		mConstStrings.push_back("");
		std::string& newString = mConstStrings.back();
		while (1) {
			if (*mCurParsingPtr != '\\' && *mCurParsingPtr != '"') {
				newString += *mCurParsingPtr;
				mCurParsingPtr++;
			}
			else if (*mCurParsingPtr == '\\') {
				mCurParsingPtr++;
				switch (*mCurParsingPtr) {
				case 'n':
					newString += "\n";
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
	}

	Token ret = Token::sInvalid;
	if (*mCurParsingPtr == '\0') 
		return Token::sEOF;  // Reach the end of the file
	
	// Now it is expecting a token.
	//
	if (_isFirstN_Equal(mCurParsingPtr, "++") ||
		_isFirstN_Equal(mCurParsingPtr, "--") ||
		_isFirstN_Equal(mCurParsingPtr, "||") ||
		_isFirstN_Equal(mCurParsingPtr, "&&") ||
		_isFirstN_Equal(mCurParsingPtr, "==") ||
		_isFirstN_Equal(mCurParsingPtr, "!=") ||
		_isFirstN_Equal(mCurParsingPtr, ">=") ||
		_isFirstN_Equal(mCurParsingPtr, "<=") ) {

		ret = Token(mCurParsingPtr, 2, mCurParsingLOC, Token::kBinaryOp);
		mCurParsingPtr += 2;
	}
	else if (_isFirstN_Equal(mCurParsingPtr, "+") ||
			 _isFirstN_Equal(mCurParsingPtr, "-") ||
			 _isFirstN_Equal(mCurParsingPtr, "*") ||
			 _isFirstN_Equal(mCurParsingPtr, "/") ||
			 _isFirstN_Equal(mCurParsingPtr, "|") ||
			 _isFirstN_Equal(mCurParsingPtr, "&") ||
			 _isFirstN_Equal(mCurParsingPtr, "=") ||
			 _isFirstN_Equal(mCurParsingPtr, ">") ||
			 _isFirstN_Equal(mCurParsingPtr, "<") ) {

		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kBinaryOp);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '{') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenCurly);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '}') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseCurly);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '[') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenBraket);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == ']') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseBraket);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '(') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kOpenParenthesis);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == ')') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kCloseParenthesis);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == ',') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kComma);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == ';') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kSemiColon);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '.') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kPeriod);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '!') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kUnaryOp);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == ':') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kQuotation);
		++mCurParsingPtr;
	}
	else if(*mCurParsingPtr == '?') {
		ret = Token(mCurParsingPtr, 1, mCurParsingLOC, Token::kBinaryOp);
		++mCurParsingPtr;
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
		while (	*mCurParsingPtr != '\0' &&
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

CompilingContext::CompilingContext(const char* content)
{
	mContentPtr = content;
	mCurParsingPtr = mContentPtr;
	mCurParsingLOC = 0;
	mpCurrentFunc = NULL;
}

CompilingContext::~CompilingContext()
{

}

RootDomain* CompilingContext::Parse(const char* content, CodeDomain* pRefDomain)
{
	mBufferedToken.clear();
	mErrorMessages.clear();
	mContentPtr = content;
	mCurParsingPtr = mContentPtr;
	mCurParsingLOC = 1;

	PushStatusCode(kAlllowStructDef | kAlllowFuncDef);
	RootDomain* rootDomain = new RootDomain(pRefDomain);
	while (ParseSingleExpression(rootDomain, NULL));

	if (IsEOF() && mErrorMessages.empty()) {
		return rootDomain;
	}
	else {
		delete rootDomain;
		return NULL;
	}
}

bool CompilingContext::ParsePartial(const char* content, CodeDomain* pDomain)
{
	mBufferedToken.clear();
	mErrorMessages.clear();
	mContentPtr = content;
	mCurParsingPtr = mContentPtr;
	mCurParsingLOC = 1;

	PushStatusCode(kAlllowStructDef | kAlllowFuncDef);
	while (ParseSingleExpression(pDomain, NULL));

	if (IsEOF() && mErrorMessages.empty()) {
		return true;
	}
	else {
		return false;
	}
}

Token CompilingContext::GetNextToken()
{
	Token ret = PeekNextToken(0);
	if (ret.IsValid()) {
		mBufferedToken.erase(mBufferedToken.begin());
	}
	return ret;
}

Token CompilingContext::PeekNextToken(int next_i)
{
	int charParsed = 0;
	int lineParsed = 0;

	int tokenNeeded = 0;
	if ((int)mBufferedToken.size() <= next_i) 
		tokenNeeded = next_i -  (int)mBufferedToken.size() + 1;
	
	for (int i = 0; i < tokenNeeded; ++i) {
		std::string errMsg;
		Token ret = ScanForToken(errMsg);

		if (ret.IsValid()) {
			mBufferedToken.push_back(ret);
		}
		else {
			if (!errMsg.empty())
				AddErrorMessage(Token(NULL, 0, mCurParsingLOC, Token::kUnknown), errMsg);
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
		return mErrorMessages.empty() ? Token::sEOF : Token::sInvalid;
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

void DataBlock::AddFloat(Float f)
{
	unsigned char* pData = (unsigned char*)&f;
	for (int i = 0; i < sizeof(Float); ++i)
		mData.push_back(pData[i]);
}

void DataBlock::AddFloat2(Float f0, Float f1)
{
	AddFloat(f0);
	AddFloat(f1);
}

void DataBlock::AddFloat3(Float f0, Float f1, Float f2)
{
	AddFloat2(f0, f1);
	AddFloat(f2);
}

void DataBlock::AddFloat4(Float f0, Float f1, Float f2, Float f3)
{
	AddFloat3(f0, f1, f2);
	AddFloat(f3);
}

void DataBlock::AddInt(int i)
{
	unsigned char* pData = (unsigned char*)&i;
	for (int j = 0; j < sizeof(int); ++j)
		mData.push_back(pData[j]);
}

void DataBlock::AddInt2(int i0, int i1)
{
	AddInt(i0);
	AddInt(i1);
}

void DataBlock::AddInt3(int i0, int i1, int i2)
{
	AddInt2(i0, i1);
	AddInt(i2);
}

void DataBlock::AddInt4(int i0, int i1, int i2, int i3)
{
	AddInt3(i0, i1, i2);
	AddInt(i3);
}

void* DataBlock::GetData()
{
	if (mData.empty())
		return NULL;
	else
		return &mData[0];
}

int DataBlock::GetSize()
{
	return (int)mData.size();
}

Exp_StructDef::Exp_StructDef(std::string name, CodeDomain* parentDomain) :
	CodeDomain(parentDomain)
{
	mStructName = name;
}

Exp_StructDef::~Exp_StructDef()
{

}

Exp_StructDef* Exp_StructDef::Parse(CompilingContext& context, CodeDomain* curDomain)
{
	Token curT = context.GetNextToken();
	if (!curT.IsValid() || !curT.IsEqual("struct")) {
		context.AddErrorMessage(curT, "Structure definition is not started with keyword \"struct\"");
		return NULL;
	}

	curT = context.GetNextToken();
	if (!curT.IsValid() || curT.GetType() != Token::kIdentifier) {
		context.AddErrorMessage(curT, "Invalid structure name.");
		return NULL;
	}

	if (IsBuiltInType(curT) || IsKeyWord(curT) || curDomain->IsTypeDefined(curT.ToStdString()) || curDomain->IsVariableDefined(curT.ToStdString(), true)) {
		context.AddErrorMessage(curT, "Structure name cannot be the built-in type, keyword, user-defined type or previous defined variable name.");
		return NULL;
	}
	std::string structName = curT.ToStdString();

	curT = context.GetNextToken();
	if (!curT.IsValid() || !curT.IsEqual("{")) {
		context.AddErrorMessage(curT, "\"{\" is expected after defining a structure.");
		return NULL;
	}

	bool succeed = false;
	std::auto_ptr<Exp_StructDef> pStructDef(new Exp_StructDef(structName, curDomain));

	context.PushStatusCode(CompilingContext::kAllowVarDef);
	if (context.ParseCodeDomain(pStructDef.get(), "}"))
		succeed = true;
	context.PopStatusCode();

	curT = context.PeekNextToken(0);
	if (curT.IsEqual("}"))
		context.GetNextToken(); // eat the "}"
	else {
		context.AddErrorMessage(curT, "\"}\" is expected.");
		return NULL;
	}

	if (!succeed || pStructDef->GetElementCount() == 0) {
		
		return NULL;
	}
	else {
		curT = context.GetNextToken();
		if (!curT.IsEqual(";")) {
			context.AddErrorMessage(curT, "\";\" is expected.");
			return NULL;
		}
		return pStructDef.release();
	}
}

void Exp_StructDef::AddVarDefExpression(Exp_VarDef* exp)
{
	CodeDomain::AddVarDefExpression(exp);
	mIdx2ValueDefs[(int)mExpressions.size() - 1] = exp;
	mElementName2Idx[exp->GetVarName().ToStdString()] = (int)mExpressions.size() - 1;
}


int Exp_StructDef::GetElementCount() const
{
	return (int)mDefinedVariables.size();
}

const std::string& Exp_StructDef::GetStructureName() const
{
	return mStructName;
}

VarType Exp_StructDef::GetElementType(int idx, const Exp_StructDef* &outStructDef, int& arraySize) const
{
	std::hash_map<int, Exp_VarDef*>::const_iterator it = mIdx2ValueDefs.find(idx);
	if (it == mIdx2ValueDefs.end())
		return VarType::kInvalid;
	else {
		VarType ret = it->second->GetVarType();
		outStructDef = it->second->GetStructDef();
		arraySize = it->second->GetArrayCnt();
		return ret;
	}
}

int Exp_StructDef::GetElementIdxByName(const std::string& name) const
{
	std::hash_map<std::string, int>::const_iterator it = mElementName2Idx.find(name);
	if (it != mElementName2Idx.end())
		return it->second;
	else
		return -1;
}


void CompilingContext::AddErrorMessage(const Token& token, const std::string& str)
{
	mErrorMessages.push_back(std::pair<Token, std::string>(token, str));
}

bool CompilingContext::HasErrorMessage() const
{
	return !mErrorMessages.empty();
}

void CompilingContext::PrintErrorMessage(std::string* outStr) const
{
	char tempBuf[400];
	std::list<std::pair<Token, std::string> >::const_iterator it = mErrorMessages.begin();
	while (it != mErrorMessages.end()) {
		sprintf_s(tempBuf, "line(%d): %s\n", it->first.GetLOC(), it->second.c_str());
		if (outStr) 
			*outStr = tempBuf;
		else
			printf(tempBuf);
		++it;
	}
}

void CompilingContext::AddWarningMessage(const Token& token, const std::string& str)
{
	mWarningMessages.push_back(std::pair<Token, std::string>(token, str));
}

bool CompilingContext::IsStructDefinePartten()
{
	Token t0 = PeekNextToken(0);
	Token t1 = PeekNextToken(1);
	Token t2 = PeekNextToken(2);

	if (!t0.IsValid() || !t1.IsValid() || !t2.IsValid())
		return false;

	if (!t0.IsEqual("struct"))
		return false;


	if (t1.GetType() != Token::kIdentifier)
		return false;

	if (!t2.IsEqual("{"))
		return false;

	return true;
}

bool CompilingContext::IsFunctionDefinePartten()
{
	int idx = 0;
	Token t0 = PeekNextToken(idx++);
	Token t1 = PeekNextToken(idx++);
	Token t2 = PeekNextToken(idx++);

	if (!t0.IsValid() || !t1.IsValid() || !t2.IsValid())
		return false;

	if (t0.GetType() != Token::kIdentifier || IsKeyWord(t0))
		return false;

	if (t1.GetType() != Token::kIdentifier)
		return false;

	if (!t2.IsEqual("("))
		return false;

	return true;
}

bool CompilingContext::IsExternalTypeDefParttern()
{
	Token t0 = PeekNextToken(0);
	Token t1 = PeekNextToken(1);

	if (t0.IsEqual("extern") && t1.GetType() == Token::kIdentifier) {
		return true;
	}
	else
		return false;
}

bool CompilingContext::IsIfExpPartten()
{
	Token t0 = PeekNextToken(0);
	Token t1 = PeekNextToken(1);

	if (!t0.IsValid() || !t1.IsValid())
		return false;

	if (t0.IsEqual("if") && t1.IsEqual("("))
		return true;
	
	return false;
}

bool CompilingContext::IsVarDefinePartten(bool allowInit)
{
	Token t0 = PeekNextToken(0);
	Token t1 = PeekNextToken(1);

	if (!t0.IsValid() || !t1.IsValid() || t0.IsEqual("void"))
		return false;

	if (t0.GetType() != Token::kIdentifier || IsKeyWord(t0))
		return false;

	if (t1.GetType() != Token::kIdentifier)
		return false;

	return true;
}

bool Exp_VarDef::Parse(CompilingContext& context, CodeDomain* curDomain, std::vector<Exp_VarDef*>& out_defs)
{
	Token curT = context.GetNextToken();
	TypeDesc typeDesc;
	out_defs.clear();

	if (!curT.IsValid() || 
		(!IsBuiltInType(curT, &typeDesc) &&
		!curDomain->IsTypeDefined(curT.ToStdString()))) {
		context.AddErrorMessage(curT, "Invalid token, must be a valid built-in type of user-defined type.");
		return false;
	}

	Token typeString = curT;
	VarType varType = typeDesc.type;
	Exp_StructDef* pStructDef = NULL;
	if (varType == VarType::kInvalid) {
		pStructDef = curDomain->GetStructDefineByName(curT.ToStdString());
		varType = VarType::kStructure;
		if (!pStructDef)
			varType = VarType::kExternType;
	}
	
	std::list<std::auto_ptr<Exp_VarDef> > tempOutDefs;
	bool bContinue = false;;
	do {
		curT = context.GetNextToken();
		if (curT.GetType() != Token::kIdentifier) {
			context.AddErrorMessage(curT, "Invalid token, must be a valid identifier.");
			return false;
		}
		if (IsBuiltInType(curT)) {
			context.AddErrorMessage(curT, "The built-in type cannot be used as variable.");
			return false;
		}
		if (IsKeyWord(curT)) {
			context.AddErrorMessage(curT, "The keyword cannot be used as variable.");
			return false;
		}
		if (curDomain->IsTypeDefined(curT.ToStdString())) {
			context.AddErrorMessage(curT, "A user-defined type cannot be redefined as variable.");
			return false;
		}

		if (curDomain->IsVariableDefined(curT.ToStdString(), false)) {
			context.AddErrorMessage(curT, "Variable redefination is not allowed in the same code block.");
			return false;
		}

		// Test if the coming token is "[", which indicates this variable is an array type.
		//
		int arrayCnt = 0;
		if (context.PeekNextToken(0).IsEqual("[")) {
			context.GetNextToken(); // Eat the "["
			Exp_ValueEval* pOrgValue = context.ParseComplexExpression(curDomain, "]");
			Exp_Constant* arrayCntExp = dynamic_cast<Exp_Constant*>(pOrgValue);

			if (!arrayCntExp || arrayCntExp->IsFloat()) {
				if (pOrgValue) delete pOrgValue;
				context.AddErrorMessage(curT, "Array size must be constant integer.");
				return false;
			}
			
			arrayCnt = (int)arrayCntExp->GetValue();
			delete pOrgValue;
			context.GetNextToken(); // Eat the "]"

		}

		// Test if the coming token is "=", which indicates the variable initialization.
		//
		Exp_ValueEval* pInitValue = NULL;
		if (context.PeekNextToken(0).IsEqual("=")) {

			Token firstT = context.PeekNextToken(0);
			if (context.GetStatusCode() & CompilingContext::kAllowVarInit) {
				context.GetNextToken(); // Eat the "="
				if (varType == VarType::kStructure) {
					context.AddErrorMessage(curT, "Initialization for structure variable is not supported.");
					return false;
				}
				// Handle the variable initialization
				pInitValue = context.ParseComplexExpression(curDomain, ";");
				if (!pInitValue)
					return false;
				Exp_ValueEval::TypeInfo typeInfo;
				std::string errMsg;
				std::vector<std::string> warnMsg;
				if (!pInitValue->CheckSemantic(typeInfo, errMsg, warnMsg)) {
					delete pInitValue;
					return false;
				}
				bool FtoI = false;
				if (!IsTypeCompatible(varType, typeInfo.type, FtoI)) {
					context.AddErrorMessage(firstT, "Bad initializing expression.");
					delete pInitValue;
					return false;
				}
				if (FtoI)
					context.AddWarningMessage(firstT, "Implicit float to int conversion.");
			}
			else {
				context.AddErrorMessage(curT, "Variable initialization not allowed in this domain.");
				return false;
			}
		}

		Exp_VarDef* ret = new Exp_VarDef(varType, curT, pInitValue);
		ret->mTypeString = typeString;
		ret->mArrayCnt = arrayCnt;

		if (varType == VarType::kStructure)
			ret->SetStructDef(pStructDef);

		// Now we can accept this variable
		//
		tempOutDefs.push_back(std::auto_ptr<Exp_VarDef>(ret));

		// Test is the next token is ";", which indicates the end of the variable definition expression.
		//
		if (context.PeekNextToken(0).IsEqual(";")) {
			break;
		}

		bContinue = context.PeekNextToken(0).IsEqual(",");
		if (bContinue) context.GetNextToken(); // eat the ","
	} while (bContinue);

	if (context.PeekNextToken(0).IsEqual(";")) {
		context.GetNextToken(); // eat the ";"
				
		if (!tempOutDefs.empty()) {
			for (std::list<std::auto_ptr<Exp_VarDef> >::iterator it = tempOutDefs.begin(); it != tempOutDefs.end(); ++it) {
				out_defs.push_back((*it).release());
			}
			return true;
		}
		else
			return false;
	}
	else 
		return false;
}

bool CompilingContext::IsEOF() const
{
	return (*mCurParsingPtr == '\0');
}

void CompilingContext::PushStatusCode(int code)
{
	mStatusCode.push_back(code);
}

int CompilingContext::GetStatusCode()
{
	if (mStatusCode.empty())
		return 0;
	else
		return mStatusCode.back();
}

void CompilingContext::PopStatusCode()
{
	mStatusCode.pop_back();
}

Exp_ValueEval::TypeInfo::TypeInfo()
{
	type = VarType::kInvalid;
	pStructDef = 0;
	arraySize = 0;
	assignable = false;
}

bool Exp_ValueEval::TypeInfo::IsTypeCompatible(const TypeInfo& from, bool& FtoI) const
{
	FtoI = false;
	if (from.arraySize > 0 || arraySize > 0)
		return false;  // Array types cannot be involved in any arithmatic except for indexer.

	if (type == VarType::kStructure)
		return pStructDef == from.pStructDef;
	else
		return SC::IsTypeCompatible(type, from.type, FtoI);
}

bool Exp_ValueEval::TypeInfo::IsSameType(const TypeInfo& ref) const
{
	if (type != ref.type ||
		pStructDef != ref.pStructDef ||
		arraySize != ref.arraySize)
		return false;
	else
		return true;
}

bool CompilingContext::ParseSingleExpression(CodeDomain* curDomain, const char* endT)
{
	if (PeekNextToken(0).IsEOF())
		return false;
	
	Token firstT = PeekNextToken(0);

	if (endT && PeekNextToken(0).IsEqual(endT)) {
		return false;
	}
	else if ((GetStatusCode() & kAllowForExp) && PeekNextToken(0).IsEqual("for")) {
		Exp_For* pFor = Exp_For::Parse(*this, curDomain);
		if (!pFor) {
			return false;
		}
		Exp_ValueEval::TypeInfo outType;
		std::string errMsg;
		std::vector<std::string> warnMsg;
		if (!pFor->CheckSemantic(outType, errMsg, warnMsg)) {
			AddErrorMessage(firstT, errMsg);
			delete pFor;
			return false;
		}
		curDomain->AddForExpression(pFor);
	}
	else if ((GetStatusCode() & kAllowIfExp) && IsIfExpPartten()) {
		Exp_If * pIf = Exp_If::Parse(*this, curDomain);
		if (!pIf) {
			return false;
		}
		Exp_ValueEval::TypeInfo outType;
		std::string errMsg;
		std::vector<std::string> warnMsg;
		if (!pIf->CheckSemantic(outType, errMsg, warnMsg)) {
			AddErrorMessage(firstT, errMsg);
			delete pIf;
			return false;
		}
		curDomain->AddIfExpression(pIf);
	}
	else if ((GetStatusCode() & kAlllowFuncDef) && IsFunctionDefinePartten()) {
		// Parse the function declaration.
		if (!Exp_FunctionDecl::Parse(*this, curDomain))
			return false;
	}
	else if ((GetStatusCode() & kAllowVarDef) && IsVarDefinePartten(true)) {
		std::vector<Exp_VarDef*>  varDefs;
		if (Exp_VarDef::Parse(*this, curDomain, varDefs)) {
			for (int i = 0; i < (int)varDefs.size(); ++i)
				curDomain->AddVarDefExpression(varDefs[i]);
		}
		else
			return false;
	}
	else if ((GetStatusCode() & kAlllowStructDef) && IsStructDefinePartten()) {
		Exp_StructDef* structDef = Exp_StructDef::Parse(*this, curDomain);
		if (structDef)
			curDomain->AddStructDefExpression(structDef);
		else
			return false;
	}
	else if ((GetStatusCode() & kAlllowStructDef) && IsExternalTypeDefParttern()) {
		Token curT = GetNextToken();
		if (!curT.IsEqual("extern")) {
			AddErrorMessage(curT, "Keyword \"extern\" is expected.");
			return false;
		}
		curT = GetNextToken();
		if (IsBuiltInType(curT) || IsKeyWord(curT)) {
			AddErrorMessage(curT, "External type cannot conflict with built-in types or keywords.");
			return false;
		}
		
		if (PeekNextToken(0).IsEqual(";")) {
			curDomain->AddExternalType(curT.ToStdString());
			GetNextToken();
		}
		else {
			AddErrorMessage(curT, "\";\" is expected.");
			return false;
		}

	}
	else if (GetStatusCode() & kAllowValueExp) {

		Exp_ValueEval* pNewExp = NULL;
		Exp_ValueEval::TypeInfo funcRetTypeInfo;
		if (PeekNextToken(0).IsEqual("return")) {
			GetNextToken(); // Eat the "return"

			Exp_FunctionDecl* pFuncDecl = mpCurrentFunc;
			assert(pFuncDecl); // return expression should be only allowed in function body.
			Exp_ValueEval* pValue = NULL;
			if (!PeekNextToken(0).IsEqual(";")) {
				pValue = ParseComplexExpression(curDomain, ";");
				if (!pValue) return false;
			}
			if (!PeekNextToken(0).IsEqual(";")) {
				AddErrorMessage(PeekNextToken(0), "Unexpected token.");
				return false;
			}
			GetNextToken(); // Eat the ";"

			pNewExp = new Exp_FuncRet(pFuncDecl, pValue);
			funcRetTypeInfo.type = pFuncDecl->GetReturnType(funcRetTypeInfo.pStructDef);
		}
		else {
			// Try to parse a complex expression
			pNewExp = ParseComplexExpression(curDomain, ";");
			if (!pNewExp) {
				Token curT = PeekNextToken(0);
				AddErrorMessage(curT, "Unexpected end of file");
				return false;
			}
			GetNextToken(); // Eat the ";"
		}

		if (pNewExp) {

			Exp_ValueEval::TypeInfo typeInfo;
			std::string errMsg;
			std::vector<std::string> warnMsg;
			if (!pNewExp->CheckSemantic(typeInfo, errMsg, warnMsg)) {
				AddErrorMessage(firstT, errMsg);
				delete pNewExp;
				return false;
			}
			if (funcRetTypeInfo.type != VarType::kInvalid) {
				bool FtoI = false;

				if (!funcRetTypeInfo.IsTypeCompatible(typeInfo, FtoI) && 
					!(funcRetTypeInfo.type == VarType::kVoid && typeInfo.type == VarType::kVoid)) {
					AddErrorMessage(firstT, "Invalid return type for this function.");
					delete pNewExp;
					return false;
				}
				if (FtoI) 
					AddWarningMessage(firstT, "Implicit float to int conversion.");
			}
			curDomain->AddValueExpression(pNewExp);
		}

	}
	else {
		if (!PeekNextToken(0).IsEOF()) {
			AddErrorMessage(PeekNextToken(0), "Unexpected expression in current domain.");
			return false;
		}
		else
			return false;
	}

	return true;
}

bool CompilingContext::ParseCodeDomain(CodeDomain* curDomain, const char* endT)
{
	if (IsEOF())
		return false;

	bool multiExp = PeekNextToken(0).IsEqual("{");

	if (multiExp) {
		GetNextToken(); // eat the "{"

		CodeDomain* childDomain = new CodeDomain(curDomain);
		curDomain->AddDomainExpression(childDomain);
		if (!ParseCodeDomain(childDomain, "}"))
			return false;

		Token endT = PeekNextToken(0);
		if (endT.IsEqual("}"))
			GetNextToken(); // eat the "}"
		else {
			AddErrorMessage(endT, "\"}\" is expected.");
			return false;
		}
	}
	else {
		while (ParseSingleExpression(curDomain, endT));
		if (!mErrorMessages.empty())
			return false;
	}

	return true;
}

CodeDomain::CodeDomain(CodeDomain* parent)
{
	mpParentDomain = parent;
}

CodeDomain::~CodeDomain()
{

	std::vector<Expression*>::iterator it_exp = mExpressions.begin();
	for (; it_exp != mExpressions.end(); ++it_exp) {
		delete *it_exp;
	}
}

bool CodeDomain::HasReturnExpForAllPaths()
{
	for (int i = 0; i < (int)mExpressions.size(); ++i) {
		if (dynamic_cast<Exp_FuncRet*>(mExpressions[i]))
			return true;
		CodeDomain* childDomain = dynamic_cast<CodeDomain*>(mExpressions[i]);
		if (childDomain && childDomain->HasReturnExpForAllPaths())
			return true;
		Exp_If* pIfExp = dynamic_cast<Exp_If*>(mExpressions[i]);
		if (pIfExp) {
			bool isEndingIf = (pIfExp->mpIfDomain && pIfExp->mpIfDomain->HasReturnExpForAllPaths());
			bool isEndingElse = (pIfExp->mpElseDomain && pIfExp->mpElseDomain->HasReturnExpForAllPaths());
			if (isEndingIf && isEndingElse)
				return true;	
		}
	}
	return false;
}

CodeDomain* CodeDomain::GetParent()
{
	return mpParentDomain;
}

void CodeDomain::AddValueExpression(Exp_ValueEval* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
	}
}

void CodeDomain::AddStructDefExpression(Exp_StructDef* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
		mDefinedStructures[exp->GetStructureName()] = exp;
	}
}

void CodeDomain::AddVarDefExpression(Exp_VarDef* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
		mDefinedVariables[exp->GetVarName().ToStdString()] = exp;
	}
}

void CodeDomain::AddFunctionDefExpression(Exp_FunctionDecl* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
		mDefinedFunctions[exp->GetFunctionName()] = exp;
	}
}

void CodeDomain::AddDomainExpression(CodeDomain* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
	}
}

void CodeDomain::AddIfExpression(Exp_If* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
	}
}

void CodeDomain::AddForExpression(Exp_For* exp)
{
	if (exp) {
		mExpressions.push_back(exp);
	}
}

bool CodeDomain::AddExternalType(const std::string& typeName)
{
	if (mExternalTypes.find(typeName) != mExternalTypes.end())
		return false;
	else {
		mExternalTypes.insert(typeName);
		return true;
	}
}

void CodeDomain::AddDefinedType(Exp_StructDef* pStructDef)
{
	if (pStructDef)
		mDefinedStructures[pStructDef->GetStructureName()] = pStructDef;
}

bool CodeDomain::IsTypeDefined(const std::string& typeName) const
{
	if (mDefinedStructures.find(typeName) == mDefinedStructures.end() && mExternalTypes.find(typeName) == mExternalTypes.end()) 
		return mpParentDomain ? mpParentDomain->IsTypeDefined(typeName) : false;
	else
		return true;
}

void CodeDomain::AddDefinedVariable(const Token& t, Exp_VarDef* pDef)
{
	mDefinedVariables[t.ToStdString()] = pDef;
}

bool CodeDomain::IsVariableDefined(const std::string& varName, bool includeParent) const
{
	if (mDefinedVariables.find(varName) == mDefinedVariables.end()) {
		if (includeParent && mpParentDomain) 
			return mpParentDomain->IsVariableDefined(varName, includeParent);
		else
			return false;
	}
	else
		return true;
}

void CodeDomain::AddDefinedFunction(Exp_FunctionDecl* pFunc)
{
	std::string funcName = pFunc->GetFunctionName();
	std::hash_map<std::string, Exp_FunctionDecl*>::iterator it = mDefinedFunctions.find(funcName);
	if (it == mDefinedFunctions.end())
		mDefinedFunctions[funcName] = pFunc;
	else
		assert(0); 
}

bool CodeDomain::IsFunctionDefined(const std::string& funcName) const
{
	std::hash_map<std::string, Exp_FunctionDecl*>::const_iterator it = mDefinedFunctions.find(funcName);
	if (it == mDefinedFunctions.end())
		return mpParentDomain ? mpParentDomain->IsFunctionDefined(funcName) : false;
	else
		return  true;
}

Exp_StructDef* CodeDomain::GetStructDefineByName(const std::string& structName)
{
	std::hash_map<std::string, Exp_StructDef*>::iterator it = mDefinedStructures.find(structName);
	if (it != mDefinedStructures.end())
		return it->second;
	else 
		return mpParentDomain ? mpParentDomain->GetStructDefineByName(structName) : NULL;
}

Exp_VarDef* CodeDomain::GetVarDefExpByName(const std::string& varName) const
{
	std::hash_map<std::string, Exp_VarDef*>::const_iterator it = mDefinedVariables.find(varName);
	if (it != mDefinedVariables.end())
		return it->second;
	else 
		return mpParentDomain ? mpParentDomain->GetVarDefExpByName(varName) : NULL;
}

Exp_VarDef::Exp_VarDef(VarType type, const Token& var, Exp_ValueEval* pInitValue)
{
	mVarType = type;
	mpStructDef = NULL;
	mVarName = var;
	mArrayCnt = 0;
	mpInitValue = pInitValue;
}

Exp_VarDef::~Exp_VarDef()
{
	if (mpInitValue)
		delete mpInitValue;
}

void Exp_VarDef::SetStructDef(const Exp_StructDef* pStruct)
{
	mpStructDef = pStruct;
}

Exp_ValueEval* Exp_VarDef::GetVarInitExp()
{
	return mpInitValue;
}

Token Exp_VarDef::GetVarName() const
{
	return mVarName;
}

Token Exp_VarDef::GetTypeString() const
{
	return mTypeString;
}

VarType Exp_VarDef::GetVarType() const
{
	return mVarType;
}

const Exp_StructDef* Exp_VarDef::GetStructDef() const
{
	return mpStructDef;
}

int Exp_VarDef::GetArrayCnt() const
{
	return mArrayCnt;
}

void Exp_VarDef::MakeIntoArraryPtr()
{
	mArrayCnt = -1;
}

RootDomain::RootDomain(CodeDomain* pRefDomain) :
	CodeDomain(pRefDomain)
{

}

RootDomain::~RootDomain()
{

}

Exp_BinaryOp::Exp_BinaryOp(const std::string& op, Exp_ValueEval* pLeft, Exp_ValueEval* pRight)
{
	mOperator = op;
	mpLeftExp = pLeft;
	mpRightExp = pRight;
}

Exp_BinaryOp::~Exp_BinaryOp()
{
	delete mpLeftExp;
	delete mpRightExp;
}

bool CompilingContext::ExpectAndEat(const char* str)
{
	Token curT = GetNextToken();
	if (curT.IsEqual(str)) 
		return true;
	else {
		std::string errMsg = str;
		errMsg += " is expected.";
		AddErrorMessage(curT, errMsg);
		return false;
	}
}

bool CompilingContext::ExpectTypeAndEat(CodeDomain* curDomain, VarType& outType, const Exp_StructDef*& outStructDef)
{
	TypeDesc retType;
	Token curT = GetNextToken();
	if (!IsBuiltInType(curT, &retType) && !curDomain->IsTypeDefined(curT.ToStdString())) {
		AddErrorMessage(curT, "Expect built-in type or a predefined structure.");
		return false;
	}
	outType = retType.type;
	outStructDef = NULL;
	if (retType.type == VarType::kInvalid) {
		outType = VarType::kStructure;
		outStructDef = curDomain->GetStructDefineByName(curT.ToStdString());
		if (!outStructDef)
			outType = VarType::kExternType;
	}

	return true;
}

Exp_ValueEval* CompilingContext::ParseSimpleExpression(CodeDomain* curDomain)
{
	Token curT = GetNextToken();

	if (!curT.IsEqual("-") &&
		curT.GetType() != Token::kIdentifier && 
		curT.GetType() != Token::kUnaryOp && 
		curT.GetType() != Token::kConstFloat &&
		curT.GetType() != Token::kConstInt &&
		curT.GetType() != Token::kOpenParenthesis) {
		AddErrorMessage(curT, "Invalid token for value expression.");
		return NULL;
	}

	std::auto_ptr<Exp_ValueEval> result;

	if (curT.GetType() == Token::kIdentifier) {
		// This identifier must be a already defined variable, built-in type or constant
		TypeDesc tpDesc;
		if (IsBuiltInType(curT, &tpDesc) && (IsFloatType(tpDesc.type) || IsIntegerType(tpDesc.type))) {
			// Parse and return the built-in type initializer
			if (!ExpectAndEat("(")) return NULL;

			std::auto_ptr<Exp_ValueEval> exp[4];
			
			bool succeed = false;
			Token lastT;
			for (int i = 0; i < tpDesc.elemCnt; ++i) {
				exp[i].reset(ParseComplexExpression(curDomain, ")", ","));
				if (!exp[i].get()) return NULL;
				// Eat the end token ")" or ","
				if (GetNextToken().IsEqual(")")) 
					break;
			}

			Exp_ValueEval* expArray[4] = {exp[0].get(), exp[1].get(), exp[2].get(), exp[3].get()};
			for (int i = 0; i < tpDesc.elemCnt; ++i)
				exp[i].release();
			result.reset(new Exp_BuiltInInitializer(expArray, tpDesc.elemCnt, tpDesc.type));
		}
		else if (curDomain->IsVariableDefined(curT.ToStdString(), true)) {
			// Return a value ref expression
			result.reset(new Exp_VariableRef(curT, curDomain->GetVarDefExpByName(curT.ToStdString())));
		}
		else if (curT.IsEqual("true") ||
				 curT.IsEqual("false")) {
			bool value = curT.IsEqual("true"); // Eat the "true" or "false"
			result.reset(new Exp_TrueOrFalse(value));
		}
		else if (curDomain->IsFunctionDefined(curT.ToStdString())) {
			// This should be a function call
			if (!PeekNextToken(0).IsEqual("(")) {
				AddErrorMessage(PeekNextToken(0), "\"(\" is expected.");
				return NULL;
			}
			GetNextToken(); // Eat the "("
			Exp_FunctionDecl* pFuncDecl = curDomain->GetFunctionDeclByName(curT.ToStdString());
			int argCnt = pFuncDecl->GetArgumentCnt();
			std::vector<std::auto_ptr<Exp_ValueEval> > argExp(argCnt);
			for (int i = 0; i < argCnt; ++i) {
				argExp[i].reset(ParseComplexExpression(curDomain, (i == (argCnt-1)) ? ")" : ","));
				if (!argExp[i].get()) {
					AddErrorMessage(PeekNextToken(0), "Invalid function call.");
					return NULL;
				}
				GetNextToken(); // Eat the ")" or ","
			}
			std::vector<Exp_ValueEval*> argExpArray(argCnt);
			for (int i = 0; i < argCnt; ++i) {
				argExpArray[i] = argExp[i].release();
			}
			result.reset(new Exp_FunctionCall(pFuncDecl, &argExpArray[0], argCnt));
		}
		else {
			AddErrorMessage(curT, "Unexpected token.");
			return NULL;
		}
	}
	else if (curT.GetType() == Token::kUnaryOp ||
			 curT.IsEqual("-")) {  // Need special handling for "-" token
		// Generate a unary operator expression
		Exp_ValueEval* ret = ParseSimpleExpression(curDomain);
		if (ret)
			result.reset(new Exp_UnaryOp(curT.ToStdString(), ret));
	}
	else if (curT.GetType() == Token::kOpenParenthesis) {
		// This expression starts with "(", so it needs to end up with ")".
		Exp_ValueEval* ret = ParseComplexExpression(curDomain, ")");
		if (ret) {
			result.reset(ret);
			GetNextToken(); // Eat the ")"
		}
	}
	else {
		// return a constant expression
		if (curT.GetType() != Token::kConstFloat && curT.GetType() != Token::kConstInt) {
			AddErrorMessage(curT, "Unexpected token.");
			return NULL;
		}

		result.reset(new Exp_Constant(curT.GetConstValue(), curT.GetType() == Token::kConstFloat));
	}

	if (!result.get()) {
		AddErrorMessage(curT, "Unexpected token.");
		return NULL;
	}
	// This is not the end of simple expression, I need to check for the next token to see if it has swizzle or structure member access.
	// e.g. myVar.xyz, myVar.myVar
	while (PeekNextToken(0).IsEqual(".") || PeekNextToken(0).IsEqual("[")) {
		// Need to deal with dot operator
		curT = GetNextToken();  // Eat the "." or "["
		if (curT.IsEqual(".")) {
			curT = GetNextToken();
			if (curT.GetType() != Token::kIdentifier) {
				AddErrorMessage(curT, "Unexpected token, identifier is expected.");
				return NULL;
			}
			result.reset(new Exp_DotOp(curT.ToStdString(), result.release()));
		}
		else {
			Exp_ValueEval* idx = ParseComplexExpression(curDomain, "]");
			if (idx == NULL) {
				AddErrorMessage(curT, "Invalid indexing expression.");
				return NULL;
			}
			GetNextToken(); // Eat the ending "]"
			result.reset(new Exp_Indexer(result.release(), idx));
		}

		
	}

	return result.release();
}

Exp_ValueEval* CompilingContext::ParseComplexExpression(CodeDomain* curDomain, const char* pEndToken0, const char* pEndToken1, Exp_ValueEval* pValueExp0, Token tOp0)
{
	Token curT = PeekNextToken(0);
	if (curT.IsEqual(pEndToken0) || curT.IsEqual(pEndToken1))
		return NULL;
	
	std::auto_ptr<Exp_ValueEval> simpleExp0;
	if (pValueExp0) 
		simpleExp0.reset(pValueExp0);
	else 
		simpleExp0.reset(ParseSimpleExpression(curDomain));

	if (!simpleExp0.get()) {
		// Must have some error message if it failed to parse a simple expression
		assert(!mErrorMessages.empty()); 
		return NULL;
	}

	Exp_ValueEval* ret = NULL;
	curT = pValueExp0 ? tOp0 : PeekNextToken(0);
	if (curT.IsEqual(pEndToken0) || curT.IsEqual(pEndToken1)) {
		ret = simpleExp0.release();
	}
	else {
		if (curT.GetType() != Token::kBinaryOp) {
			AddErrorMessage(curT, "Expect a binary operator.");
			return NULL;
		}

		if (pValueExp0 == NULL)
			GetNextToken(); // Eat the binary operator

		int op0_level = curT.GetBinaryOpLevel();
		std::string op0_str = curT.ToStdString();
		tOp0 = curT;

		std::auto_ptr<Exp_ValueEval> simpleExp1(ParseSimpleExpression(curDomain));
		if (!simpleExp1.get()) {
			// Must have some error message if it failed to parse a simple expression
			assert(!mErrorMessages.empty()); 
			return NULL;
		}

		Token nextT = PeekNextToken(0);
		// Get the next token to decide if the next binary operator is in high level of priority
		if (nextT.IsEqual(pEndToken0) || nextT.IsEqual(pEndToken1)) {
			// we are done for this complex value expression, return it.
			Exp_BinaryOp* pBinaryOp = new Exp_BinaryOp(op0_str, simpleExp0.release(), simpleExp1.release());
			ret = pBinaryOp;
		}
		else if (nextT.GetType() == Token::kBinaryOp) {

			GetNextToken(); // Eat the binary operator
			int op1_level = nextT.GetBinaryOpLevel();
			std::string op1_str = nextT.ToStdString();

			if (op1_str != "?") {
				if (op1_level > op0_level) {
					Exp_ValueEval* simpleExpRight = ParseComplexExpression(curDomain, pEndToken0, pEndToken1, simpleExp1.release(), nextT);
					if (simpleExpRight) {
						Exp_BinaryOp* pBinaryOp = new Exp_BinaryOp(op0_str, simpleExp0.release(), simpleExpRight);
						ret = pBinaryOp;
					}
				}
				else {
					Exp_BinaryOp* pBinaryOpLeft = new Exp_BinaryOp(op0_str, simpleExp0.release(), simpleExp1.release());
					Exp_ValueEval* simpleExpRet = ParseComplexExpression(curDomain, pEndToken0, pEndToken1, pBinaryOpLeft, nextT);
					ret = simpleExpRet;
				}
			}
			else {
				std::auto_ptr<Exp_ValueEval> selectValue(Exp_Select::Parse(*this, curDomain, simpleExp1.release()));
				Exp_BinaryOp* pBinaryOp = new Exp_BinaryOp(op0_str, simpleExp0.release(), selectValue.release());
				ret = pBinaryOp;
			}
		}
	}

	return ret;
}

Exp_Constant::Exp_Constant(double v, bool f)
{
	mValue = v;
	mIsFromFloat = f;
}

Exp_Constant::~Exp_Constant()
{

}

double Exp_Constant::GetValue() const
{
	return mValue;
}

bool Exp_Constant::IsFloat() const
{
	return mIsFromFloat;
}

bool Exp_Constant::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	outType.type = mIsFromFloat ? VarType::kFloat : VarType::kInt;
	outType.pStructDef = NULL;
	outType.arraySize = 0;
	outType.assignable = false;
	mCachedTypeInfo = outType;
	return true;
}

Exp_VariableRef::Exp_VariableRef(Token t, Exp_VarDef* pDef)
{
	mVariable = t;
	mpDef = pDef;
}

Exp_VariableRef::~Exp_VariableRef()
{

}

bool Exp_VariableRef::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	outType.type = mpDef->GetVarType();
	outType.pStructDef = mpDef->GetStructDef();
	outType.arraySize = mpDef->GetArrayCnt();
	outType.assignable = true;
	mCachedTypeInfo = outType;
	return true;
}

bool Exp_VariableRef::IsAssignable(bool allowSwizzle) const
{
	if (mpDef->GetVarType() == VarType::kInvalid ||
		mpDef->GetVarType() == VarType::kVoid)
		return false;
	else {
		return mpDef->GetArrayCnt() == 0;
	}
}

const Exp_StructDef* Exp_VariableRef::GetStructDef()
{
	if (mpDef) 
		return mpDef->GetStructDef();
	else
		return NULL;
}

const Exp_VarDef* Exp_VariableRef::GetVarDef() const
{
	return mpDef;
}

Exp_BuiltInInitializer::Exp_BuiltInInitializer(Exp_ValueEval** pExp, int cnt, VarType tp)
{
	mType = tp;
	mpSubExprs[0] = NULL;
	mpSubExprs[1] = NULL;
	mpSubExprs[2] = NULL;
	mpSubExprs[3] = NULL;

	int aCnt = cnt > 4 ? 4 : cnt;
	for (int i = 0; i < aCnt; ++i) {
		mpSubExprs[i] = pExp[i];
	}
}

Exp_BuiltInInitializer::~Exp_BuiltInInitializer()
{
	for (int i = 0; i < 4; ++i) {
		if (mpSubExprs[i])
			delete mpSubExprs[i];
	}
}

bool Exp_BuiltInInitializer::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	int ElemCnt = TypeElementCnt(mType);
	int ElemCntGiven = 0;
	bool hasFloat = false;
	bool SubExpCnt = 0;
	for (int i = 0; i < 4; ++i) {
		Exp_ValueEval* curExp = mpSubExprs[i];
		if (curExp) {
			TypeInfo typeInfo;
			if (!curExp->CheckSemantic(typeInfo, errMsg, warnMsg)) 
				return false;
			if (!IsFloatType(typeInfo.type) && !IsIntegerType(typeInfo.type)) {
				errMsg = "Must be float or integer types.";
				return false;
			}
			if (IsFloatType(typeInfo.type))
				hasFloat = true;
			ElemCntGiven += TypeElementCnt(typeInfo.type);
		}
	}

	if (ElemCntGiven != ElemCnt) {
		errMsg = "Bad built-in type initialization.";
		return false;
	}

	if (IsIntegerType(mType)) {
		if (hasFloat) warnMsg.push_back("Integer implicitly converted to float type.");
	}
	outType.type = mType;
	outType.pStructDef = NULL;
	outType.arraySize = 0;
	outType.assignable = false;
	mCachedTypeInfo = outType;
	return true;
}

Exp_UnaryOp::Exp_UnaryOp(const std::string& op, Exp_ValueEval* pExp)
{
	mOpType = op;
	mpExpr = pExp;
}

Exp_UnaryOp::~Exp_UnaryOp()
{
	if (mpExpr)
		delete mpExpr;
}

bool Exp_UnaryOp::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	// Currently only "!" and "-" are supported unary operator
	if (!mpExpr->CheckSemantic(outType, errMsg, warnMsg))
		return false;

	if (outType.arraySize > 0) {
		errMsg = "Array type can only be used with indexer.";
		return false;
	}

	if (mOpType == "!" && !IsBooleanType(outType.type)) {
		errMsg = "\"!\" must be followed with boolean expression.";
		return false;
	}

	mCachedTypeInfo = outType;
	return true;
}


bool Exp_BinaryOp::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	TypeInfo leftType, rightType;
	if (!mpLeftExp->CheckSemantic(leftType, errMsg, warnMsg) || !mpRightExp->CheckSemantic(rightType, errMsg, warnMsg))
		return false;
	if (leftType.arraySize > 0 || rightType.arraySize > 0) {
		errMsg = "Array type can only be used with indexer.";
		return false;
	}

	if (leftType.type == VarType::kStructure || rightType.type == VarType::kStructure) {
		// Only "=" operator can accept structure as the arguments
		if (mOperator == "=") {
			if (leftType.pStructDef != rightType.pStructDef) {
				errMsg = "Cannot assign from different structure.";
				return false;
			}
			else {
				outType.pStructDef = leftType.pStructDef;
				outType.type = VarType::kStructure;
				mCachedTypeInfo = outType;
				return true;
			}
		}
		else {
			errMsg = "Cannot do binary operation between structure or built-in types.";
			return false;
		}
	}

	if (leftType.type == VarType::kExternType || rightType.type == VarType::kExternType) {
		if (mOperator == "=") {

			if (leftType.type != rightType.type) {
				errMsg = "Cannot do binary operation between external type and internal type";
				return false;
			}
			else
				return true;
		}
		else {
			errMsg = "Cannot do binary operation between external types";
			return false;
		}
	}

	if (!IsValueType(leftType.type) || !IsValueType(rightType.type)) {
		errMsg = "Non-value type cannot perform binary operation.";
		return false;
	}

	bool isArithmetric = (mOperator == "+" || mOperator == "-" || mOperator == "*" || mOperator == "/");
	bool isCompareOp = (mOperator == "==" || mOperator == "!=" || mOperator == ">=" || mOperator == ">" || mOperator == "<=" || mOperator == "<");
	bool isLogicOp = (mOperator == "||" || mOperator == "&&");
	bool isBitwizeOp = (mOperator == "|" || mOperator == "&");

	if (isCompareOp) {


		if (leftType.type == VarType::kStructure || rightType.type == VarType::kStructure) {
			errMsg = "Comparison operator cannot be performed with structures.";
			return false;
		}

		// "greater than" or "less than" operators can only be performed on numerical scalar values
		if (IsBooleanType(leftType.type) || IsBooleanType(rightType.type)) {
			errMsg = mOperator;
			errMsg += " operator cannot be performed with boolean values.";
			return false;
		}

		if (TypeElementCnt(rightType.type) > 1) {
			if (TypeElementCnt(leftType.type) > TypeElementCnt(rightType.type)) {
				errMsg = mOperator;
				errMsg += " Cannot do comparison with right argument of less elements.";
				return false;
			}
		}

		outType.pStructDef = NULL;
		outType.type = MakeType(VarType::kBoolean, TypeElementCnt(leftType.type));
		outType.arraySize = 0;
		outType.assignable = false;
		mCachedTypeInfo = outType;
		return true;
	}

	if (isArithmetric || isBitwizeOp || mOperator == "=") {

		// Perform the additional check for bitwize operation
		if (isBitwizeOp) {
			if (!IsIntegerType(leftType.type) || !IsIntegerType(rightType.type)) {
				errMsg = "Cannot do bitwise operation with non-integer types.";
				return false;
			}
		}

		if (leftType.type != rightType.type) {
			if (TypeElementCnt(leftType.type) > TypeElementCnt(rightType.type)) {
				if ((isArithmetric ||  mOperator == "=") && TypeElementCnt(rightType.type) == 1) {
					// If the right value is scalar, it means "splats" for arithmetric or assign operator

				}
				else {
					errMsg = "Cannot do binary operation with right argument of less elements.";
					return false;
				}
			}
			if (IsIntegerType(leftType.type) && !IsIntegerType(rightType.type))
				warnMsg.push_back("Integer implicitly converted to float type.");
		}

		if (isArithmetric) {
			if (IsBooleanType(leftType.type) || IsBooleanType(rightType.type)) {
				errMsg = "Cannot do binary operation with boolean values.";
				return false;
			}
		}

		if (mOperator == "=") {
			if (!mpLeftExp->IsAssignable(true)) {
				errMsg = "Cannot do assignment with left value.";
				return false;
			}
		}

		outType.pStructDef = NULL;
		outType.type = leftType.type;
		outType.arraySize = 0;
		outType.assignable = false;
		mCachedTypeInfo = outType;
		return true;
	}
	
	if (isLogicOp) {
		if (!IsBooleanType(leftType.type) || !IsBooleanType(rightType.type)) {
			errMsg = "Cannot do logic operation with non-boolean types.";
			return false;
		}
		else {
			outType.type = MakeType(VarType::kBoolean, TypeElementCnt(leftType.type));
			outType.pStructDef = NULL;
			outType.arraySize = 0;
			outType.assignable = false;
			mCachedTypeInfo = outType;
			return true;
		}
	}

	return false;
}

Exp_DotOp::Exp_DotOp(const std::string& opStr, Exp_ValueEval* pExp)
{
	mOpStr = opStr;
	mpExp = pExp;
}

Exp_DotOp::~Exp_DotOp()
{
	if (mpExp)
		delete mpExp;
}

bool Exp_DotOp::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	TypeInfo parentType;
	if (!mpExp->CheckSemantic(parentType, errMsg, warnMsg))
		return false;
	if (mpExp->GetCachedTypeInfo().arraySize > 0) {
		errMsg = "Must use indexer before swizzling or accessing its members.";
		return false;
	}

	if (parentType.type == VarType::kStructure) {
		if (parentType.pStructDef->IsVariableDefined(mOpStr, false)) {
			Exp_VarDef* pDef = parentType.pStructDef->GetVarDefExpByName(mOpStr);
			assert(pDef);
			outType.type = pDef->GetVarType();
			if (outType.type == VarType::kStructure)
				outType.pStructDef = pDef->GetStructDef();
			else
				outType.pStructDef = NULL;
			outType.arraySize = pDef->GetArrayCnt();
			outType.assignable = parentType.assignable;
		}
		else {
			errMsg = "Invalid structure member name.";
			return false;
		}
	}
	else {
		// The dotOp is the swizzle operation.
		int swizzleIdx[4];
		int elemCnt = ConvertSwizzle(mOpStr.c_str(), swizzleIdx);
		if (elemCnt <= 0) {
			errMsg = "Invalid swizzle expression.";
			return false;
		}

		int parentElemCnt = TypeElementCnt(parentType.type);
		for (int i = 0; i < elemCnt; ++i) {
			if (swizzleIdx[i] >= parentElemCnt) {
				errMsg = "Invalid swizzle expression - element out of range.";
				return false;
			}
		}

		outType.type = MakeType(parentType.type, elemCnt);
		outType.pStructDef = NULL;
		outType.arraySize = 0;
		outType.assignable = (parentType.assignable && elemCnt == 1) ? true : false;
	}
	mCachedTypeInfo = outType;
	return true;
}

bool Exp_DotOp::IsAssignable(bool allowSwizzle) const
{
	// assume the CheckSemantic() is already called before invoking this function.
	//
	if (!allowSwizzle && mpExp->GetCachedTypeInfo().type != VarType::kStructure)
		return false; // reject the swizzling case
	
	return GetCachedTypeInfo().assignable;
}

Exp_FunctionDecl::Exp_FunctionDecl(CodeDomain* parent) :
	CodeDomain(parent)
{
	mReturnType = VarType::kInvalid;
	mpRetStruct = NULL;
	mHasBody = false;
}

Exp_FunctionDecl::~Exp_FunctionDecl()
{

}

const std::string Exp_FunctionDecl::GetFunctionName() const
{
	return mFuncName;
}

VarType Exp_FunctionDecl::GetReturnType(const Exp_StructDef* &retStruct)
{
	retStruct = mpRetStruct;
	return mReturnType;
}

int Exp_FunctionDecl::GetArgumentCnt() const
{
	return (int)mArgments.size();
}

Exp_FunctionDecl::ArgDesc* Exp_FunctionDecl::GetArgumentDesc(int idx)
{
	return &mArgments[idx];
}

bool Exp_FunctionDecl::HasSamePrototype(const Exp_FunctionDecl& ref) const
{
	if (mFuncName != ref.mFuncName)
		return false;
	if (mReturnType != ref.mReturnType)
		return false;
	if (mpRetStruct != ref.mpRetStruct)
		return false;
	if (mArgments.size() != ref.mArgments.size())
		return false;
	for (int i = 0; i < (int)mArgments.size(); ++i) {
		if (mArgments[i].isByRef != ref.mArgments[i].isByRef ||
			!mArgments[i].typeInfo.IsSameType(ref.mArgments[i].typeInfo) )
			return false;
	}
	return true;
}

bool Exp_FunctionDecl::HasBody() const
{
	return mHasBody;
}


Exp_FunctionDecl* CodeDomain::GetFunctionDeclByName(const std::string& funcName)
{
	std::hash_map<std::string, Exp_FunctionDecl*>::iterator it = mDefinedFunctions.find(funcName);
	if (it != mDefinedFunctions.end())
		return it->second;
	else
		return mpParentDomain ? mpParentDomain->GetFunctionDeclByName(funcName) : NULL;
}

int CodeDomain::GetExpressionCnt() const
{
	return (int)mExpressions.size();
}

Expression* CodeDomain::GetExpression(int idx)
{
	return mExpressions[idx];
}

Exp_FunctionDecl* Exp_FunctionDecl::Parse(CompilingContext& context, CodeDomain* curDomain)
{
	std::auto_ptr<Exp_FunctionDecl> result(new Exp_FunctionDecl(curDomain));
	// The first token needs to be the returned type of the function
	//
	if (!context.ExpectTypeAndEat(curDomain, result->mReturnType, result->mpRetStruct))
		return NULL;

	// The second token should be the function name
	//
	Token curT = context.GetNextToken();
	Token funcNameT = curT;
	bool alreadyDefined = (curDomain->IsFunctionDefined(curT.ToStdString()));
	result->mFuncName = curT.ToStdString();

	// The coming tokens should be the function arguments in a pair of brackets
	// e.g. (Type0 arg0, Type1 arg1)
	//
	if (!context.ExpectAndEat("("))
		return NULL;
	while (!context.PeekNextToken(0).IsEqual(")")) {
		Exp_FunctionDecl::ArgDesc argDesc;
		Token argTypeString = context.PeekNextToken(0);
		if (!context.ExpectTypeAndEat(curDomain, argDesc.typeInfo.type, argDesc.typeInfo.pStructDef))
			return NULL;

		argDesc.needJITPacked = false;
		argDesc.isByRef = false;
		argDesc.isArrayPtr = false;
		if (context.PeekNextToken(0).IsEqual("&") || context.PeekNextToken(0).IsEqual("%")) {
			argDesc.needJITPacked = context.GetNextToken().IsEqual("&");
			argDesc.isByRef = true;
		}
		Token argT = context.GetNextToken();
		for (int i = 0; i < (int)result->mArgments.size(); ++i) {
			if (argT.IsEqual(result->mArgments[i].token)) {
				context.AddErrorMessage(curT, "Function argument redefined.");
				return NULL;
			}
		}
		// function argument can be of array type, e.g. "float3% var[]". Note it cannot have JIT-Packed directive for arguments of array type.
		if (context.PeekNextToken(0).IsEqual("[")) {
			context.GetNextToken(); // eat the "["

			curT = context.GetNextToken();
			if (!curT.IsEqual("]")) {
				context.AddErrorMessage(curT, "\"]\" is expected.");
				return NULL;
			}
			if (!argDesc.isByRef) {
				context.AddErrorMessage(curT, "Argument of array type must be passed by reference.");
				return NULL;
			}

			if (argDesc.needJITPacked) {
				context.AddErrorMessage(curT, "Argument of array type cannot be JIT-Packed.");
				return NULL;
			}
			argDesc.isArrayPtr = true;
		}
		argDesc.token = argT;
		argDesc.typeString = argTypeString;
		result->mArgments.push_back(argDesc);

		if (context.PeekNextToken(0).IsEqual(","))
			context.GetNextToken(); // Eat the ","

	} 

	context.GetNextToken(); // Eat the ")"

	Exp_FunctionDecl* pFuncDef = NULL;
	if (alreadyDefined) {
		pFuncDef = curDomain->GetFunctionDeclByName(result->mFuncName);
		assert(pFuncDef);
		if (!result->HasSamePrototype(*pFuncDef)) {
			context.AddErrorMessage(curT, "Function declared with different prototype.");
			return NULL;
		}
		result.reset(NULL);
	}
	else {
		pFuncDef = result.get();
		curDomain->AddFunctionDefExpression(result.release());
	}

	Exp_FunctionDecl* ret = pFuncDef;
	if (!ret->mHasBody && context.PeekNextToken(0).IsEqual("{")) {
		// The function body is expected, if there is already a function body for this function, kick it out.
		if (alreadyDefined) {
			if (!pFuncDef->mExpressions.empty()) {
				context.AddErrorMessage(curT, "Function already has a body.");
				return NULL;
			}
		}

		for (int i = 0; i < (int)pFuncDef->mArgments.size(); ++i) {
			Exp_VarDef* pExp = new Exp_VarDef(pFuncDef->mArgments[i].typeInfo.type, pFuncDef->mArgments[i].token, NULL);
			if (pFuncDef->mArgments[i].typeInfo.type == VarType::kStructure)
				pExp->SetStructDef(pFuncDef->mArgments[i].typeInfo.pStructDef);
			if (pFuncDef->mArgments[i].isArrayPtr)
				pExp->MakeIntoArraryPtr();
			pFuncDef->AddVarDefExpression(pExp);
		}

		// Now parse the function body
		assert(context.mpCurrentFunc == NULL);
		context.mpCurrentFunc = pFuncDef;
		context.PushStatusCode(CompilingContext::kAlllowStructDef | CompilingContext::kAllowReturnExp | 
			CompilingContext::kAllowValueExp | CompilingContext::kAllowVarDef |
			CompilingContext::kAllowVarInit | CompilingContext::kAllowIfExp |
			CompilingContext::kAllowForExp);
		if (!context.ParseCodeDomain(pFuncDef, NULL))
			ret = NULL;
		context.PopStatusCode();
		context.mpCurrentFunc = NULL;
		if (!ret)
			return NULL; // Bad function body

		// If this function returns a value except void type, check if all the code paths have return expressions.
		if (pFuncDef->mReturnType != VarType::kVoid) {
			if (!pFuncDef->HasReturnExpForAllPaths()) {
				context.AddErrorMessage(funcNameT, "Not all path has the return value.");
				ret = NULL;
				return NULL;
			}
		}
		ret->mHasBody = true;
	}
	else {
		if (context.PeekNextToken(0).IsEqual(";")) {
			context.GetNextToken(); // Eat the ending ";"
		}
		else {
			context.AddErrorMessage(funcNameT, "Bad function declaration.");
			ret = NULL;
		}
	}

	return ret;
}

Exp_FuncRet::Exp_FuncRet(Exp_FunctionDecl* pFuncDecl, Exp_ValueEval* pRet)
{
	mpRetValue = pRet;
	mpFuncDecl = pFuncDecl;
}

Exp_FuncRet::~Exp_FuncRet()
{
	if (mpRetValue)
		delete mpRetValue;
}

bool Exp_FuncRet::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	if (mpRetValue) {
		if (!mpRetValue->CheckSemantic(outType, errMsg, warnMsg))
			return false;
		if (outType.arraySize > 0) {
			errMsg = "Function cannot return array type.";
			return false;
		}
		assert(mpFuncDecl);
		TypeInfo funcRetInfo;
		funcRetInfo.type = mpFuncDecl->GetReturnType(funcRetInfo.pStructDef);
		bool FtoI = false;
		if (!funcRetInfo.IsTypeCompatible(outType, FtoI)) {
			errMsg = "Invalid return expression.";
			return false;
		}
		if (FtoI)
			warnMsg.push_back("Implicit float to int conversion.");

	}
	else {
		outType.type = VarType::kVoid;
		outType.pStructDef = NULL;
	}

	mCachedTypeInfo = outType;
	return true;
}

Exp_TrueOrFalse::Exp_TrueOrFalse(bool value)
{
	mValue = value;
}

Exp_TrueOrFalse::~Exp_TrueOrFalse()
{

}

bool Exp_TrueOrFalse::GetValue() const
{
	return mValue;
}

bool Exp_TrueOrFalse::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	outType.type = VarType::kBoolean;
	outType.pStructDef = NULL;
	outType.arraySize = 0;
	outType.assignable = false;
	mCachedTypeInfo = outType;
	return true;
}

Exp_ValueEval::Exp_ValueEval()
{
	mCachedTypeInfo.type = VarType::kInvalid;
	mCachedTypeInfo.pStructDef = NULL;
}

Exp_ValueEval::TypeInfo Exp_ValueEval::GetCachedTypeInfo() const
{
	return mCachedTypeInfo;
}

bool Exp_ValueEval::IsAssignable(bool allowSwizzle) const
{
	return false;
}

void Exp_ValueEval::GenerateAssignCode(CG_Context* context, llvm::Value* pValue) const
{
	assert(0);
}

Exp_ValueEval::ValuePtrInfo Exp_ValueEval::GetValuePtr(CG_Context* context) const
{
	ValuePtrInfo ptrInfo;
	ptrInfo.belongToVector = false;
	ptrInfo.isFixedArray = false;
	ptrInfo.valuePtr = NULL;
	ptrInfo.vecElemIdx = -1;
	return ptrInfo;
}

Exp_FunctionCall::Exp_FunctionCall(Exp_FunctionDecl* pFuncDef, Exp_ValueEval** ppArgs, int cnt)
{
	mpFuncDef = pFuncDef;
	for (int i = 0; i < cnt; ++i) 
		mInputArgs.push_back(ppArgs[i]);
}

Exp_FunctionCall::~Exp_FunctionCall()
{
	for (int i = 0; i < (int)mInputArgs.size(); ++i) 
		delete mInputArgs[i];
}

bool Exp_FunctionCall::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	if (!mpFuncDef) {
		errMsg = "Bad function call.";
		return false;
	}
	int reqArgCnt = mpFuncDef->GetArgumentCnt();
	if (reqArgCnt != mInputArgs.size()) {
		errMsg = "Invalid function argument count.";
		return false;
	}

	for (int i = 0; i < reqArgCnt; ++i) {
		Exp_FunctionDecl::ArgDesc* pArgDesc = mpFuncDef->GetArgumentDesc(i);
		TypeInfo argTypeInfo;
		if (!mInputArgs[i]->CheckSemantic(argTypeInfo, errMsg, warnMsg))
			return false;
		if (pArgDesc->isByRef && !mInputArgs[i]->IsAssignable(false)) {
			errMsg = "Argument passed as reference is not assignable.";
			return false;
		}
		if (pArgDesc->isByRef && !pArgDesc->typeInfo.IsSameType(argTypeInfo)) {
			errMsg = "Argument passed as reference should be of the same type(no implicit conversion).";
			return false;
		}
		bool FtoI = false;
		if (pArgDesc->isByRef) {
			if (!pArgDesc->typeInfo.IsSameType(argTypeInfo)) {
				errMsg = "Reference argument must recieve the value of the same type.";
				return false;
			}
		}
		else {
			if (!pArgDesc->typeInfo.IsTypeCompatible(argTypeInfo, FtoI)) {
				errMsg = "Cannot perform implicity type conversion for argument.";
				return false;
			}
		}
		if (FtoI)
			warnMsg.push_back("Implicit float to int conversion.");
	}
	outType.type = mpFuncDef->GetReturnType(outType.pStructDef);
	// TODO: handle array types?
	mCachedTypeInfo = outType;
	return true;
}

Exp_ConstString::Exp_ConstString(const char* pString)
{
	mStringPtr = pString;
}

Exp_ConstString::~Exp_ConstString()
{

}

const char* Exp_ConstString::GetStringData() const
{
	return mStringPtr;
}

bool Exp_ConstString::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	outType.type = VarType::kExternType;
	outType.assignable = false;
	outType.pStructDef = NULL;
	outType.arraySize = 0;

	mCachedTypeInfo = outType;
	return true;
}

Exp_Indexer::Exp_Indexer(Exp_ValueEval* pExp, Exp_ValueEval* pIndex)
{
	mpExp = pExp;
	mpIndex = pIndex;
}

Exp_Indexer::~Exp_Indexer()
{
	delete mpExp;
	delete mpIndex;
}

bool Exp_Indexer::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	TypeInfo idxType;
	if (!mpIndex->CheckSemantic(idxType, errMsg, warnMsg))
		return false;

	if (idxType.type != VarType::kInt) {
		errMsg = "Indexer must be integer type.";
		return false;
	}

	TypeInfo expType;
	if (!mpExp->CheckSemantic(expType, errMsg, warnMsg))
		return false;
	if (expType.arraySize == 0) {
		errMsg = "Indexer must be applied to variable of array type.";
		return false;
	}
		
	outType.type = expType.type;
	outType.pStructDef = expType.pStructDef;
	outType.arraySize = 0;
	outType.assignable = true;
	mCachedTypeInfo = outType;
	return true;
}

bool Exp_Indexer::IsAssignable(bool allowSwizzle) const
{
	return GetCachedTypeInfo().assignable;
}

Exp_If::Exp_If(CodeDomain* parent)
{
	mpCondValue = NULL;
	mpIfDomain = NULL;
	mpElseDomain = NULL;
}

Exp_If::~Exp_If()
{
	if (mpCondValue)
		delete mpCondValue;
	if (mpIfDomain)
		delete mpIfDomain;
	if (mpElseDomain)
		delete mpElseDomain;
}

bool Exp_If::CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	Exp_ValueEval::TypeInfo condType;
	if (!mpCondValue->CheckSemantic(condType, errMsg, warnMsg)) {
		return false;
	}

	if (condType.type != VarType::kBoolean && 
		condType.type != VarType::kFloat && 
		condType.type != VarType::kInt &&
		condType.type != VarType::kExternType) {
		errMsg = "The condition value of if expression must be type of single value.";
		return false;
	}


	return true;
}

Exp_If* Exp_If::Parse(CompilingContext& context, CodeDomain* curDomain)
{
	Token curT = context.GetNextToken();
	if (!curT.IsEqual("if")) {
		context.AddErrorMessage(curT, "\"if\" is expected.");
		return NULL;
	}

	curT = context.GetNextToken();
	if (!curT.IsEqual("(")) {
		context.AddErrorMessage(curT, "\"(\" is expected.");
		return NULL;
	}

	// Parse the condition expression
	//
	std::auto_ptr<Exp_If> result(new Exp_If(curDomain));
	result->mpCondValue = context.ParseComplexExpression(curDomain, ")");
	if (!result->mpCondValue) {
		// Bad condition expression
		return NULL;
	}

	context.GetNextToken(); // Eat the ending ")" of condition expression

	// Parse the if code block
	//
	curT = context.PeekNextToken(0);
	result->mpIfDomain = new CodeDomain(curDomain);
	if (curT.IsEqual("{")) {
		context.GetNextToken();  // Eat the "{"
		if (!context.ParseCodeDomain(result->mpIfDomain, "}")) {
			return NULL;
		}
		context.GetNextToken();  // Eat the "}"
	}
	else {
		if (!context.ParseSingleExpression(result->mpIfDomain, ";")) {
			context.AddErrorMessage(curT, "Invalid if expression.");
			return NULL;
		}
	}

	// Optionally parse the else code block
	//
	curT = context.PeekNextToken(0);
	if (curT.IsEqual("else")) {
		context.GetNextToken(); // Eat the "else"
		result->mpElseDomain = new CodeDomain(curDomain);
		curT = context.PeekNextToken(0);
		if (curT.IsEqual("{")) {
			context.GetNextToken();  // Eat the "{"
			if (!context.ParseCodeDomain(result->mpElseDomain, "}")) {
				return NULL;
			}
			context.GetNextToken();  // Eat the "}"
		}
		else {
			if (!context.ParseSingleExpression(result->mpElseDomain, ";")) {
				context.AddErrorMessage(curT, "Invalid else expression.");
				return NULL;
			}
		}
	}
	return result.release();
}

Exp_For::Exp_For()
{
	mForBody = NULL;
	mStartStepCond = NULL;
}

Exp_For::~Exp_For()
{
	if (mForBody) delete mForBody;
	if (mStartStepCond) delete mStartStepCond;
}

bool Exp_For::CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	Exp_ValueEval::TypeInfo contCondType;
	Exp_ValueEval* pValueExp = dynamic_cast<Exp_ValueEval*>(mStartStepCond->GetExpression(1));
	if (!pValueExp || !pValueExp->CheckSemantic(contCondType, errMsg, warnMsg))
		return false;
	if (contCondType.type != VarType::kBoolean) {
		errMsg = "The for ending condition must be boolean value";
		return false;
	}
	return true;
}

Exp_For* Exp_For::Parse(CompilingContext& context, CodeDomain* curDomain)
{
	Token curT = context.GetNextToken();
	if (!curT.IsEqual("for"))
		return NULL;
	curT = context.GetNextToken();
	if (!curT.IsEqual("("))
		return NULL;
	std::auto_ptr<Exp_For> result(new Exp_For());
	result->mStartStepCond = new CodeDomain(curDomain);

	// Parse start expression
	if (!context.ParseSingleExpression(result->mStartStepCond, ";")) {
		if (context.HasErrorMessage())
			return NULL;
		result->mStartStepCond->AddValueExpression(new Exp_Nop());
	}
	assert(result->mStartStepCond->GetExpressionCnt() == 1);
	
	if (context.PeekNextToken(0).IsEqual(";"))
		context.GetNextToken(); // Eat the ending ";"
	// Parse continuing condition expression
	Exp_ValueEval* condValue = context.ParseComplexExpression(result->mStartStepCond, ";");
	if (condValue == NULL) {
		context.AddErrorMessage(curT, "Invalid for condition expression.");
		return NULL;
	}

	context.GetNextToken(); // Eat the ending ";"
	result->mStartStepCond->AddValueExpression(condValue);
	assert(result->mStartStepCond->GetExpressionCnt() == 2);

	curT = context.PeekNextToken(0);
	// Parse step expression
	Exp_ValueEval* stepValue = context.ParseComplexExpression(result->mStartStepCond, ")");
	if (stepValue == NULL) {
		if (context.HasErrorMessage()) {
			context.AddErrorMessage(curT, "Invalid for step expression.");
			return NULL;
		}
	}
	context.GetNextToken(); // Eat the ending ")"
	if (stepValue) 
		result->mStartStepCond->AddValueExpression(stepValue);
	else
		result->mStartStepCond->AddValueExpression(new Exp_Nop());

	assert(result->mStartStepCond->GetExpressionCnt() == 3);

	curT = context.PeekNextToken(0);
	result->mForBody = new CodeDomain(result->mStartStepCond);
	if (curT.IsEqual("{")) {
		context.GetNextToken();  // Eat the "{"
		if (!context.ParseCodeDomain(result->mForBody, "}")) {
			return NULL;
		}
		context.GetNextToken();  // Eat the "}"
	}
	else {
		if (!context.ParseSingleExpression(result->mForBody, ";")) {
			context.GetNextToken(); // Eat the ";"
			context.AddErrorMessage(curT, "Invalid for body expression.");
			return NULL;
		}
	}

	return result.release();
}

Exp_Nop::Exp_Nop()
{

}

Exp_Nop::~Exp_Nop()
{

}

llvm::Value* Exp_Nop::GenerateCode(CG_Context* context) const
{
	return NULL;
}

bool Exp_Nop::CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	outType.arraySize = 0;
	outType.assignable = false;
	outType.pStructDef = NULL;
	outType.type = VarType::kInvalid;
	mCachedTypeInfo = outType;
	return true;
}

Exp_Select::Exp_Select()
{
	mpCondValue = NULL;
	mpFirstValue = NULL;
	mpSecondValue = NULL;
}

Exp_Select::~Exp_Select()
{
	if (mpCondValue)
		delete mpCondValue;
	if (mpFirstValue)
		delete mpFirstValue;
	if (mpSecondValue)
		delete mpSecondValue;
}


bool Exp_Select::CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg)
{
	TypeInfo condType;
	if (!mpCondValue->CheckSemantic(condType, errMsg, warnMsg))
		return false;

	TypeInfo trueType;
	if (!mpFirstValue->CheckSemantic(trueType, errMsg, warnMsg))
		return false;

	TypeInfo falseType;
	if (!mpSecondValue->CheckSemantic(falseType, errMsg, warnMsg))
		return false;

	if (!IsBooleanType(condType.type)) {
		errMsg = "Conditional type must be boolean.";
		return false;
	}

	if (trueType.type != falseType.type) {
		errMsg = "Select operation must be applied to the same types.";
		return false;
	}

	if (TypeElementCnt(condType.type) != TypeElementCnt(trueType.type)) {
		errMsg = "Select operation must be applied to the same types with same element number.";
		return false;
	}

	outType.type = trueType.type;
	return true;
}

Exp_Select* Exp_Select::Parse(CompilingContext& context, CodeDomain* curDomain, Exp_ValueEval* pCondValue)
{
	if (context.PeekNextToken(0).IsEqual("?")) {
		Token curT = context.GetNextToken();
		if (!curT.IsEqual("?")) {
			context.AddErrorMessage(curT, "Invalid expression for select.");
			return NULL;
		}
	}

	Token curT = context.PeekNextToken(0);
	std::auto_ptr<Exp_ValueEval> pTrueValue(context.ParseComplexExpression(curDomain, ":"));
	if (!pTrueValue.get()) {
		context.AddErrorMessage(curT, "Invalid true expression for select.");
		return NULL;
	}
	if (context.PeekNextToken(0).IsEqual(":"))
		context.GetNextToken();

	std::auto_ptr<Exp_ValueEval> pFalseValue(context.ParseComplexExpression(curDomain, ";"));
	if (!pFalseValue.get()) {
		context.AddErrorMessage(curT, "Invalid false expression for select.");
		return NULL;
	}

	Exp_Select* ret = new Exp_Select();
	ret->mpCondValue = pCondValue;
	ret->mpFirstValue = pTrueValue.release();
	ret->mpSecondValue = pFalseValue.release();

	return ret;
}


#ifdef WANT_MEM_LEAK_CHECK
std::set<Expression*> Expression::s_instances;
Expression::Expression()
{
	s_instances.insert(this);
}

Expression::~Expression()
{
	s_instances.erase(this);
}

#else
Expression::Expression()
{
}

Expression::~Expression()
{
}

#endif

} // namespace SC