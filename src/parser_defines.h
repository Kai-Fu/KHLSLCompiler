#pragma once
#define MAX_TOKEN_LENGTH 100
#include "../inc/SC_API.h"
#include <vector>
#include <hash_map>

namespace llvm {
	class Function;
}

namespace SC {

	bool IsBuiltInType(VarType type);
	bool IsFloatType(VarType type);
	bool IsIntegerType(VarType type);
	bool IsBooleanType(VarType type);
	bool IsValueType(VarType type);
	int TypeElementCnt(VarType type);
	int TypePackedSize(VarType type);
	VarType MakeType(VarType baseType, int elemCnt);
	int ConvertSwizzle(const char* swizzleStr, int swizzleIdx[4]);
	bool IsTypeCompatible(VarType dest, VarType from, bool& FtoIwarning);
} // namespace SC


class KSC_StructDesc : public std::vector<KSC_TypeInfo>
{
public:
	~KSC_StructDesc();
	struct MemberInfo
	{
		int idx;
		int mem_offset;
		int mem_size;
		std::string type_string;
	};
	int mStructSize;
	int mAlignment;
	std::hash_map<std::string, MemberInfo> mMemberIndices;
};

class KSC_FunctionDesc
{
public:
	~KSC_FunctionDesc();

	std::vector<KSC_TypeInfo> mArgumentTypes;
	std::vector<std::string> mArgTypeStrings;
	llvm::Function* F;
	std::vector<int> needJITPacked;

	void* pJIT_Func;
};

class KSC_ModuleDesc
{
public:
	~KSC_ModuleDesc();

	std::hash_map<std::string, KSC_StructDesc*> mGlobalStructures;
	std::hash_map<std::string, KSC_FunctionDesc*> mFunctionDesc;

};
