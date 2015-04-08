#pragma once
#include <list>
#include "parser_defines.h"

namespace SC {

	class Token;

	void Initialize_AST_Gen();
	void Finish_AST_Gen();

	bool IsBuiltInType(const Token& token, TypeDesc* out_type = NULL);
	bool IsKeyWord(const Token& token, KeyWord* out_key = NULL);

	class Token
	{
	public:
		enum Type {
			kIdentifier,
			kConstInt,
			kConstFloat,
			kOpenCurly, // {
			kCloseCurly, // }
			kOpenParenthesis, // (
			kCloseParenthesis, // )
			kOpenBraket, // [
			kCloseBraket, // ]
			kBinaryOp, // +,=,*,/, etc
			kUnaryOp, // !,
			kComma,
			kSemiColon,
			kQuotation,
			kPeriod,
			kString,
			kUnknown
		};
	private:
		const char* mpData;
		int mNumOfChar;
		int mLOC;
		Type mType;
	public:
		static Token sInvalid;
		static Token sEOF;
	public:
		Token();
		Token(const char* p, int num, int line, Type tp);
		Token(const Token& ref);

		double GetConstValue() const;
		int GetBinaryOpLevel() const;
		Type GetType() const;
		int GetLOC() const;

		bool IsValid() const;
		bool IsEOF() const;
		bool IsEqual(const char* dest) const;
		bool IsEqual(const Token& dest) const;
		void ToAnsiString(char* dest) const;
		const char* GetRawData() const;
		std::string ToStdString() const;
	};

	class Tokenizer
	{
	public:
		Tokenizer(const char* content);
		~Tokenizer();

	private:
		const char* mContentPtr;
		const char* mCurParsingPtr;
		int mCurParsingLOC;

		std::list<Token> mBufferedToken;
	};

}