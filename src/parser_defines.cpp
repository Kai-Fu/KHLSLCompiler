#include "parser_defines.h"
#include <assert.h>

namespace SC {

bool IsBuiltInType(VarType type)
{
	switch (type) {
	case kFloat:
	case kFloat2:
	case kFloat3:
	case kFloat4:
	case kFloat8:
	case kInt:
	case kInt2:
	case kInt3:
	case kInt4:
	case kInt8:
	case kBoolean:
	case kBoolean2:
	case kBoolean3:
	case kBoolean4:
	case kBoolean8:
		return true;
	}
	return false;
}

bool IsFloatType(VarType type)
{
	switch (type) {
	case kFloat:
	case kFloat2:
	case kFloat3:
	case kFloat4:
	case kFloat8:
		return true;
	}
	return false;
}

bool IsIntegerType(VarType type)
{
	switch (type) {
	case kInt:
	case kInt2:
	case kInt3:
	case kInt4:
	case kInt8:
		return true;
	}
	return false;
}

bool IsBooleanType(VarType type)
{
	switch (type) {
	case kBoolean:
	case kBoolean2:
	case kBoolean3:
	case kBoolean4:
	case kBoolean8:
		return true;
	}
	return false;
}

bool IsValueType(VarType type)
{
	switch (type) {
	case kFloat:
	case kFloat2:
	case kFloat3:
	case kFloat4:
	case kFloat8:

	case kInt:
	case kInt2:
	case kInt3:
	case kInt4:
	case kInt8:

	case kBoolean:
	case kBoolean2:
	case kBoolean3:
	case kBoolean4:
	case kBoolean8:
		return true;
	}
	return false;
}

int TypeElementCnt(VarType type)
{
	switch (type) {
	case kFloat:
		return 1;
	case kFloat2:
		return 2;
	case kFloat3:
		return 3;
	case kFloat4:
		return 4;
	case kFloat8:
		return 8;
	case kInt:
		return 1;
	case kInt2:
		return 2;
	case kInt3:
		return 3;
	case kInt4:
		return 4;
	case kInt8:
		return 8;
	case kBoolean:
		return 1;
	case kBoolean2:
		return 2;
	case kBoolean3:
		return 3;
	case kBoolean4:
		return 4;
	case kBoolean8:
		return 8;
	case kVoid:
		return 0;
	}
	return 0;
}

int TypePackedSize(VarType type)
{
	switch (type) {
	case kFloat:
		return 1*sizeof(Float);
	case kFloat2:
		return 2*sizeof(Float);
	case kFloat3:
		return 3*sizeof(Float);
	case kFloat4:
		return 4*sizeof(Float);
	case kFloat8:
		return 8*sizeof(Float);
	case kBoolean:
	case kInt:
		return 1*sizeof(Int);
	case kBoolean2:
	case kInt2:
		return 2*sizeof(Int);
	case kBoolean3:
	case kInt3:
		return 3*sizeof(Int);
	case kBoolean4:
	case kInt4:
		return 4*sizeof(Int);
	case kInt8:
	case kBoolean8:
		return 8*sizeof(Int);
	case kExternType:
		return sizeof(void*);
	}
	return 0;
}

VarType MakeType(VarType baseType, int elemCnt)
{
	switch (baseType) {
	case kFloat:
	case kFloat2:
	case kFloat3:
	case kFloat4:
	case kFloat8:
		return VarType(VarType::kFloat + elemCnt - 1);
	case kInt:
	case kInt2:
	case kInt3:
	case kInt4:
	case kInt8:
		return VarType(VarType::kInt + elemCnt - 1);
	case kBoolean:
	case kBoolean2:
	case kBoolean3:
	case kBoolean4:
	case kBoolean8:
		return VarType(VarType::kBoolean + elemCnt - 1);
	}

	assert(0);
	return VarType::kInvalid;
}

int ConvertSwizzle(const char* swizzleStr, int swizzleIdx[4])
{
	int cnt = 0;
	const char* pCur = swizzleStr;
	while (*pCur != '\0') {
		switch (*pCur) {
		case 'x':
		case 'r':
			swizzleIdx[cnt] = 0;
			break;
		case 'y':
		case 'g':
			swizzleIdx[cnt] = 1;
			break;
		case 'z':
		case 'b':
			swizzleIdx[cnt] = 2;
			break;
		case 'w':
		case 'a':
			swizzleIdx[cnt] = 3;
			break;
		default:
			return -1;
		}
		++pCur;
		++cnt;
		if (cnt > 4)
			return -1;
	}
	return cnt;
}

bool IsTypeCompatible(VarType dest, VarType from, bool& FtoIwarning)
{
	bool ret = false;
	bool destIsI = false;
	FtoIwarning = false;

	if (dest == VarType::kExternType && from == VarType::kExternType)
		return true;

	switch (dest) {
	case kInt:
	case kInt2:
	case kInt3:
	case kInt4:
	case kInt8:
		destIsI = true;
	case kFloat:
	case kFloat2:
	case kFloat3:
	case kFloat4:
	case kFloat8:

	case kBoolean:
	case kBoolean2:
	case kBoolean3:
	case kBoolean4:
	case kBoolean8:
		ret = TypeElementCnt(dest) <= TypeElementCnt(from);
	}

	if (destIsI && IsFloatType(from))
		FtoIwarning = true;

	return ret;
}

} // namespace SC


KSC_StructDesc::~KSC_StructDesc()
{
	for (int i = 0; i < (int)this->size(); ++i) {
		if ((*this)[i].type == SC::VarType::kStructure)
			delete (KSC_StructDesc*)(*this)[i].hStruct;
	}
}

KSC_ModuleDesc::~KSC_ModuleDesc()
{
	{
		std::hash_map<std::string, KSC_StructDesc*>::iterator it = mGlobalStructures.begin();
		for (; it != mGlobalStructures.end(); ++it) {
			delete it->second;
		}
	}

	{
		std::hash_map<std::string, KSC_FunctionDesc*>::iterator it = mFunctionDesc.begin();
		for (; it != mFunctionDesc.end(); ++it) {
			delete it->second;
		}
	}
}

KSC_FunctionDesc::~KSC_FunctionDesc()
{
	for (int i = 0; i < (int)mArgumentTypes.size(); ++i) {
		if (mArgumentTypes[i].type == SC::VarType::kStructure)
			delete (KSC_StructDesc*)mArgumentTypes[i].hStruct;
	}
}