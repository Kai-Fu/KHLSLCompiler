#pragma once
#include <string>
#include <vector>
#include <list>
#include <hash_map>
#include <hash_set>
#include <set>
#include "parser_defines.h"

#define WANT_MEM_LEAK_CHECK

namespace llvm {
	class Value;
	class Function;
	class Type;
}

namespace SC {


	class Token;
	struct TypeDesc;
	class Exp_VarDef;
	class Exp_ValueEval;
	class CG_Context;
	class Exp_If;
	class Exp_For;

	enum KeyWord {
		kStructDef,
		kIf,
		kElse,
		kFor,
		kReturn,
		kTrue,
		kFalse
	};

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
	
	class DataBlock
	{
	private:
		std::vector<unsigned char> mData;

	public:
		void AddFloat(Float f);
		void AddFloat2(Float f0, Float f1);
		void AddFloat3(Float f0, Float f1, Float f2);
		void AddFloat4(Float f0, Float f1, Float f2, Float f3);

		void AddInt(int i);
		void AddInt2(int i0, int i1);
		void AddInt3(int i0, int i1, int i2);
		void AddInt4(int i0, int i1, int i2, int i3);

		void* GetData();
		int GetSize();
	};

	class Expression
	{
	public:
		Expression();
		virtual ~Expression();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

#ifdef WANT_MEM_LEAK_CHECK
		static std::set<Expression*> s_instances;
#endif
	};
	
	class Exp_StructDef;
	class CompilingContext;
	class Exp_FunctionDecl;

	class CodeDomain : public Expression
	{
	protected:
		CodeDomain* mpParentDomain;

		std::hash_map<std::string, Exp_StructDef*> mDefinedStructures;
		std::hash_map<std::string, Exp_VarDef*> mDefinedVariables;
		std::hash_map<std::string, Exp_FunctionDecl*> mDefinedFunctions;
		std::hash_set<std::string> mExternalTypes;
	public:
		std::vector<Expression*> mExpressions;
		enum ParsingStatus {
			kAllowValueExp		= 0x00000001,
			kAllowVarInit		= 0x00000002,
			kAllowVarDef		= 0x00000004,
			kAllowReturnExp		= 0x00000008,
			kAlllowStructDef	= 0x00000010,
			kAlllowFuncDef		= 0x00000020,
			kAllowIfExp			= 0x00000040,
			kAllowForExp		= 0x00000080
		};
		int mExpAllowedFlag;

	private:
		void AddDefinedType(Exp_StructDef* pStructDef);
		void AddDefinedVariable(const Token& t, Exp_VarDef* pDef);
		void AddDefinedFunction(Exp_FunctionDecl* pFunc);

	public:
		CodeDomain(CodeDomain* parent);
		virtual ~CodeDomain();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual bool HasReturnExpForAllPaths();

		CodeDomain* GetParent();

		void AddValueExpression(Exp_ValueEval* exp);
		void AddStructDefExpression(Exp_StructDef* exp);
		virtual void AddVarDefExpression(Exp_VarDef* exp);
		void AddFunctionDefExpression(Exp_FunctionDecl* exp);
		void AddDomainExpression(CodeDomain* exp);
		void AddIfExpression(Exp_If* exp);
		void AddForExpression(Exp_For* exp);
		bool AddExternalType(const std::string& typeName);

		bool IsTypeDefined(const std::string& typeName) const;
		bool IsVariableDefined(const std::string& varName, bool includeParent) const;
		bool IsFunctionDefined(const std::string& funcName) const;

		Exp_StructDef* GetStructDefineByName(const std::string& structName);
		Exp_VarDef* GetVarDefExpByName(const std::string& varName) const;
		Exp_FunctionDecl* GetFunctionDeclByName(const std::string& funcName);
		int GetExpressionCnt() const;
		Expression* GetExpression(int idx);
	};

	class Exp_VarDef : public Expression
	{
	private:
		Token mTypeString;
		Token mVarName;
		Exp_ValueEval* mpInitValue;
		VarType mVarType;
		int mArrayCnt;  // 0 means this variable is not an array, -1 means it is a pointer to the type(variable length array)
		const Exp_StructDef* mpStructDef;

	public:
		Exp_VarDef(VarType type, const Token& var, Exp_ValueEval* pInitValue);
		virtual ~Exp_VarDef();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		static bool Parse(CompilingContext& context, CodeDomain* curDomain, std::vector<Exp_VarDef*>& out_defs);

		void SetStructDef(const Exp_StructDef* pStruct);
		Exp_ValueEval* GetVarInitExp();
		Token GetVarName() const;
		Token GetTypeString() const;
		VarType GetVarType() const;
		const Exp_StructDef* GetStructDef() const;
		int GetArrayCnt() const;
		void MakeIntoArraryPtr();
	};

	class Exp_StructDef : public CodeDomain
	{
	private:
		std::string mStructName;
		std::hash_map<int, Exp_VarDef*> mIdx2ValueDefs;
		std::hash_map<std::string, int> mElementName2Idx;
	public:
		Exp_StructDef(std::string name, CodeDomain* parentDomain);
		virtual ~Exp_StructDef();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual void AddVarDefExpression(Exp_VarDef* exp);

		int GetStructSize() const;
		int GetElementCount() const;
		const std::string& GetStructureName() const;
		VarType GetElementType(int idx, const Exp_StructDef* &outStructDef, int& arraySize) const;
		int GetElementIdxByName(const std::string& name) const;

		void ConvertToDescription(KSC_StructDesc& ref, CG_Context& ctx) const;

		static Exp_StructDef* Parse(CompilingContext& context, CodeDomain* curDomain);
	};

	class Exp_ValueEval : public Expression
	{
	public:
		struct TypeInfo {
			VarType type;
			const Exp_StructDef* pStructDef;
			std::string typeString;
			int arraySize;
			bool assignable;

			TypeInfo();
			bool IsTypeCompatible(const TypeInfo& from, bool& FtoI) const;
			bool IsSameType(const TypeInfo& ref) const;
		};

		struct ValuePtrInfo {
			llvm::Value* valuePtr;
			bool belongToVector;
			bool isFixedArray;
			int vecElemIdx;
		};

		Exp_ValueEval();
		TypeInfo GetCachedTypeInfo() const;
		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg = std::string(), std::vector<std::string>& warnMsg = std::vector<std::string>()) = 0;
		virtual bool IsAssignable(bool allowSwizzle) const;
		virtual void GenerateAssignCode(CG_Context* context, llvm::Value* pValue) const;
		virtual ValuePtrInfo GetValuePtr(CG_Context* context) const;

	protected:
		TypeInfo mCachedTypeInfo;
	};

	class Exp_Constant : public Exp_ValueEval
	{
	private:
		double mValue;
		bool mIsFromFloat;

	public:
		Exp_Constant(double v, bool f);
		virtual ~Exp_Constant();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		double GetValue() const;
		bool IsFloat() const;
		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_TrueOrFalse : public Exp_ValueEval
	{
	private:
		bool mValue;
	public:
		Exp_TrueOrFalse(bool value);
		virtual ~Exp_TrueOrFalse();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		bool GetValue() const;
		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_VariableRef : public Exp_ValueEval
	{
	private:
		Token mVariable;
		Exp_VarDef* mpDef;

	public:
		Exp_VariableRef(Token t, Exp_VarDef* pDef);
		virtual ~Exp_VariableRef();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		const Exp_StructDef* GetStructDef();
		const Exp_VarDef* GetVarDef() const;
		virtual ValuePtrInfo GetValuePtr(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
		virtual bool IsAssignable(bool allowSwizzle) const;
		virtual void GenerateAssignCode(CG_Context* context, llvm::Value* pValue) const;
	};

	class Exp_BuiltInInitializer : public Exp_ValueEval
	{
	private:
		Exp_ValueEval* mpSubExprs[4];
		VarType mType;

	public:
		Exp_BuiltInInitializer(Exp_ValueEval** pExp, int cnt, VarType tp);
		virtual ~Exp_BuiltInInitializer();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_UnaryOp : public Exp_ValueEval
	{
	private:
		std::string mOpType;
		Exp_ValueEval* mpExpr;

	public:
		Exp_UnaryOp(const std::string& op, Exp_ValueEval* pExp);
		virtual ~Exp_UnaryOp();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_BinaryOp : public Exp_ValueEval
	{
	private:
		std::string mOperator;
		Exp_ValueEval* mpLeftExp;
		Exp_ValueEval* mpRightExp;

	public:
		Exp_BinaryOp(const std::string& op, Exp_ValueEval* pLeft, Exp_ValueEval* pRight);
		virtual ~Exp_BinaryOp();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	// A DotOp is either to access the structure member or to perform swizzle for a built-in type
	//
	class Exp_DotOp : public Exp_ValueEval
	{
	private:
		std::string mOpStr;
		Exp_ValueEval* mpExp;
	public:
		Exp_DotOp(const std::string& opStr, Exp_ValueEval* pExp);
		virtual ~Exp_DotOp();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual void GenerateAssignCode(CG_Context* context, llvm::Value* pValue) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
		virtual bool IsAssignable(bool allowSwizzle) const;

		virtual ValuePtrInfo GetValuePtr(CG_Context* context) const;
	};

	class Exp_Indexer : public Exp_ValueEval
	{
	private:
		Exp_ValueEval* mpExp;
		Exp_ValueEval* mpIndex;

	public:
		Exp_Indexer(Exp_ValueEval* pExp, Exp_ValueEval* pIndex);
		virtual ~Exp_Indexer();

		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual void GenerateAssignCode(CG_Context* context, llvm::Value* pValue) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
		virtual bool IsAssignable(bool allowSwizzle) const;

		virtual ValuePtrInfo GetValuePtr(CG_Context* context) const;
	};

	class Exp_FunctionDecl : public CodeDomain
	{
	public:
		struct ArgDesc {
			Exp_ValueEval::TypeInfo typeInfo;
			bool isByRef;
			bool needJITPacked;
			bool isArrayPtr;
			Token token;
			Token typeString;
		};

	private:
		VarType mReturnType;
		const Exp_StructDef* mpRetStruct;
		std::string mFuncName;
		std::vector<ArgDesc> mArgments;
		bool mHasBody;

	public:
		Exp_FunctionDecl(CodeDomain* parent);
		virtual ~Exp_FunctionDecl();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		const std::string GetFunctionName() const;
		VarType GetReturnType(const Exp_StructDef* &retStruct);
		int GetArgumentCnt() const;
		ArgDesc* GetArgumentDesc(int idx);
		bool HasSamePrototype(const Exp_FunctionDecl& ref) const;
		bool HasBody() const;
		void ConvertToDescription(KSC_FunctionDesc& desc, CG_Context& ctx);

		static Exp_FunctionDecl* Parse(CompilingContext& context, CodeDomain* curDomain);
	};

	class Exp_FuncRet : public Exp_ValueEval
	{
	private:
		Exp_ValueEval* mpRetValue;
		Exp_FunctionDecl* mpFuncDecl;
	public:
		Exp_FuncRet(Exp_FunctionDecl* pFuncDecl, Exp_ValueEval* pRet);
		virtual ~Exp_FuncRet();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_FunctionCall : public Exp_ValueEval
	{
	private:
		std::vector<Exp_ValueEval*> mInputArgs;
		Exp_FunctionDecl* mpFuncDef;
	public:
		Exp_FunctionCall(Exp_FunctionDecl* pFuncDef, Exp_ValueEval** ppArgs, int cnt);
		virtual ~Exp_FunctionCall();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_ConstString : public Exp_ValueEval
	{
	private:
		const char* mStringPtr;
	public:
		Exp_ConstString(const char* pString);
		virtual ~Exp_ConstString();
		const char* GetStringData() const;
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class Exp_Select : public Exp_ValueEval
	{
	private:
		Exp_ValueEval* mpCondValue;
		Exp_ValueEval* mpFirstValue;
		Exp_ValueEval* mpSecondValue;
	public:
		Exp_Select();
		virtual ~Exp_Select();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual bool CheckSemantic(TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);

		static Exp_Select* Parse(CompilingContext& context, CodeDomain* curDomain, Exp_ValueEval* pCondValue);
	};

	class Exp_If : public Expression
	{
		friend class CodeDomain;
	private:
		CodeDomain* mpIfDomain;
		CodeDomain* mpElseDomain;
		Exp_ValueEval* mpCondValue;
	public:
		Exp_If(CodeDomain* parent);
		virtual ~Exp_If();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
		static Exp_If* Parse(CompilingContext& context, CodeDomain* curDomain);
	};

	class Exp_For : public Expression
	{
	private:
		CodeDomain* mForBody;
		CodeDomain* mStartStepCond;
	public:
		Exp_For();
		virtual ~Exp_For();
		virtual llvm::Value* GenerateCode(CG_Context* context) const;

		virtual bool CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
		static Exp_For* Parse(CompilingContext& context, CodeDomain* curDomain);
	};

	class Exp_Nop : public Exp_ValueEval
	{
	public:
		Exp_Nop();
		virtual ~Exp_Nop();

		virtual llvm::Value* GenerateCode(CG_Context* context) const;
		virtual bool CheckSemantic(Exp_ValueEval::TypeInfo& outType, std::string& errMsg, std::vector<std::string>& warnMsg);
	};

	class RootDomain : public CodeDomain
	{
	public:
		RootDomain(CodeDomain* pRefDomain);
		virtual ~RootDomain();

		bool CompileToIR(CG_Context* pPredefine, KSC_ModuleDesc& mouduleDesc, CG_Context* pUseCtx = NULL);
	};



	class CompilingContext
	{

	private:
		const char* mContentPtr;
		const char* mCurParsingPtr;
		int mCurParsingLOC;


		std::list<Token> mBufferedToken;
		std::list<std::pair<Token, std::string> > mErrorMessages;
		std::list<std::pair<Token, std::string> > mWarningMessages;
		std::list<std::string> mConstStrings;
	public:
		Exp_FunctionDecl* mpCurrentFunc;

	private:
		bool IsVarDefinePartten(bool allowInit);
		bool IsStructDefinePartten();
		bool IsFunctionDefinePartten();
		bool IsExternalTypeDefParttern();
		bool IsIfExpPartten();
		
		Token ScanForToken(std::string& errorMsg);

	public:
		CompilingContext(const char* content);
		~CompilingContext();

		void AddErrorMessage(const Token& token, const std::string& str);
		bool HasErrorMessage() const;
		void PrintErrorMessage(std::string* outStr = NULL) const;

		void AddWarningMessage(const Token& token, const std::string& str);
		bool IsEOF() const;

		Token GetNextToken();
		Token PeekNextToken(int next_i);
		bool ExpectAndEat(const char* str);
		bool ExpectTypeAndEat(CodeDomain* curDomain, VarType& outType, const Exp_StructDef*& outStructDef);

		RootDomain* Parse(const char* content, CodeDomain* pRefDomain);
		bool ParsePartial(const char* content, CodeDomain* pDomain);

		bool ParseSingleExpression(CodeDomain* curDomain, const char* endT);

		bool ParseCodeDomain(CodeDomain* curDomain, const char* endT);
		// Simple expression means the expression is in the bracket pair or it is a expression other than a binary operation.
		//
		Exp_ValueEval* ParseSimpleExpression(CodeDomain* curDomain);
		// Complex expression means it contains any simple expression and binary operation.
		//
		Exp_ValueEval* ParseComplexExpression(CodeDomain* curDomain, const char* pEndToken0, const char* pEndToken1 = NULL, Exp_ValueEval* pValueExp0 = NULL, Token tOp0 = Token::sInvalid);

	};

} // namespace SC