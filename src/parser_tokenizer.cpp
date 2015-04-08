#include "parser_tokenizer.h"
#include "parser_defines.h"
namespace SC {

	Token Token::sInvalid = Token(NULL, 0, 0, Token::kUnknown);
	Token Token::sEOF = Token(NULL, -1, -1, Token::kUnknown);

	static std::hash_map<std::string, TypeDesc> s_BuiltInTypes;
	static std::hash_map<std::string, KeyWord> s_KeyWords;

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

	void Finish_AST_Gen()
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

} // namespace SC
