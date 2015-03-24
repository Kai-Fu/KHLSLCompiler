#include "../inc/SC_API.h"
#include "IR_Gen_Context.h"
#include "parser_AST_Gen.h"
#include <string>
#include <list>
#include <stdio.h>
#include <llvm/Support/Host.h>
#include <llvm/IR/Verifier.h>


static std::string			s_lastErrMsg;
SC::RootDomain*				s_predefineDomain = NULL;
SC::CG_Context				s_predefineCtx;
KSC_ModuleDesc*				s_predefineModule = NULL;
std::list<KSC_ModuleDesc*>	s_modules;						

static int __int_pow(int base, int p)
{
	return _Pow_int(base, p);
}

static void SplitStringByDot(const char* inStr, std::vector<std::string>& outStrings)
{
	const char* pCur = inStr;
	outStrings.push_back("");
	while (*pCur != '\0') {
		if (*pCur != '.')
			outStrings.back().push_back(*pCur);
		else
			outStrings.push_back("");
		++pCur;
	}
}

bool KSC_Initialize(const char* sharedCode)
{
	SC::Initialize_AST_Gen();
	bool ret = SC::InitializeCodeGen();

	int CPUInfo[4];
	__cpuid(CPUInfo, 0x80000000);
    int nExIds = CPUInfo[0];
	char CPUBrandString[0x40];
    memset(CPUBrandString, 0, sizeof(CPUBrandString));

	for (int i = 0x80000000; i <= nExIds; ++i) {
		__cpuid(CPUInfo, i);
		if  (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if  (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
	}


	printf("KSC running on CPU %s.\n", CPUBrandString);
	if (ret) {

		SC::CompilingContext preContext(NULL);
		const char* intrinsicFuncDecal = 
			"float sin(float arg);\n"
			"float cos(float arg);\n"
			"float pow(float base, float exp);\n"
			"int ipow(int base, int exp);\n"
			"float sqrt(float arg);\n"
			"float fabs(float arg);\n"
			"float asin(float arg);\n"
			"float acos(float arg);\n";

		KSC_AddExternalFunction("sin", sinf);
		KSC_AddExternalFunction("cos", cosf);
		KSC_AddExternalFunction("pow", powf);
		KSC_AddExternalFunction("ipow", __int_pow);
		KSC_AddExternalFunction("sqrt", sqrtf);
		KSC_AddExternalFunction("fabs", fabsf);
		KSC_AddExternalFunction("asin", asinf);
		KSC_AddExternalFunction("acos", acosf);

		s_predefineDomain = new SC::RootDomain(NULL);
		if (!preContext.ParsePartial(intrinsicFuncDecal, s_predefineDomain))
			return false;

		if (sharedCode) {
			if (!preContext.ParsePartial(sharedCode, s_predefineDomain))
				return false;
		}

		s_predefineModule = new KSC_ModuleDesc();
		ret = s_predefineDomain->CompileToIR(NULL, *s_predefineModule, &s_predefineCtx);
		if (ret)
			return true;
		else {
			delete s_predefineModule;
			s_predefineModule = NULL;
			return false;
		}
	}
	else
		return false;
}


void KSC_Destory()
{
	std::list<KSC_ModuleDesc*>::iterator it = s_modules.begin();
	for (; it != s_modules.end(); ++it) {
		delete *it;
	}
	if (s_predefineModule) {
		delete s_predefineModule;
		s_predefineModule = NULL;
	}

	SC::DestoryCodeGen();
	SC::Finish_AST_Gen();
}


const char* KSC_GetLastErrorMsg()
{
	return s_lastErrMsg.c_str();
}

bool KSC_AddExternalFunction(const char* funcName, void* funcPtr)
{
	SC::CG_Context::sGlobalFuncSymbols[funcName] = funcPtr;
	return true;
}

ModuleHandle KSC_Compile(const char* sourceCode)
{
#ifdef WANT_MEM_LEAK_CHECK
	size_t expInstCnt = SC::Expression::s_instances.size();
#endif	

	KSC_ModuleDesc* ret = NULL;
	{
		KSC_ModuleDesc* pModuleDesc = new KSC_ModuleDesc;
		SC::CompilingContext scContext(NULL);
		std::auto_ptr<SC::RootDomain> scDomain(scContext.Parse(sourceCode, s_predefineDomain));
		if (scDomain.get() == NULL) {
			scContext.PrintErrorMessage(&s_lastErrMsg);
		}
		else {
			if (!scDomain->CompileToIR(&s_predefineCtx, *pModuleDesc)) {
				delete pModuleDesc;
				s_lastErrMsg = "Failed to compile.";
			}
			else{
				s_modules.push_back(pModuleDesc);
				ret = pModuleDesc;
			}
		}
	}

#ifdef WANT_MEM_LEAK_CHECK
	assert(SC::Expression::s_instances.size() == expInstCnt);
#endif
	return ret;
}

void* KSC_GetFunctionPtr(FunctionHandle hFunc, bool bDump)
{
	KSC_FunctionDesc* pFuncDesc = (KSC_FunctionDesc*)hFunc;
	if (!pFuncDesc || !pFuncDesc->F)
		return NULL;

	if (pFuncDesc->pJIT_Func)
		return pFuncDesc->pJIT_Func;

	llvm::Function* wrapperF = SC::CG_Context::CreateFunctionWithPackedArguments(*pFuncDesc);

	if (bDump) {
		printf("------------- Function before JIT wrapping ------------------------\n");
		pFuncDesc->F->dump();
	}
	if (bDump && pFuncDesc->F != wrapperF) {
		printf("------------- Function after JIT wrapping ------------------------\n");
		wrapperF->dump();
	}

	if (!llvm::verifyFunction(*wrapperF)) {
		SC::CG_Context::TheFPM->run(*wrapperF);
		if (bDump) {
			printf("------------- Function after FPM optimization ------------------------\n");
			wrapperF->dump();
		}
		void* ret = (void*)SC::CG_Context::TheExecutionEngine->getFunctionAddress(wrapperF->getName());
		pFuncDesc->pJIT_Func = ret;
		return ret;
	}
	else
		return NULL;
}

FunctionHandle KSC_GetFunctionHandleByName(const char* funcName, ModuleHandle hModule)
{
	KSC_ModuleDesc* pModule = (KSC_ModuleDesc*)hModule;
	if (!pModule)
		return NULL;

	if (pModule->mFunctionDesc.find(funcName) != pModule->mFunctionDesc.end()) {
		return pModule->mFunctionDesc[funcName];
	}
	else
		return NULL;
}

int KSC_GetFunctionArgumentCount(FunctionHandle hFunc)
{
	KSC_FunctionDesc* pFuncDesc = (KSC_FunctionDesc*)hFunc;
	if (!pFuncDesc)
		return NULL;

	return (int)pFuncDesc->mArgumentTypes.size();
}

KSC_TypeInfo KSC_GetFunctionArgumentType(FunctionHandle hFunc, int argIdx)
{
	KSC_TypeInfo ret = {SC::VarType::kInvalid, 0, 0, 0, NULL, NULL, false, false};
	KSC_FunctionDesc* pFuncDesc = (KSC_FunctionDesc*)hFunc;
	if (!pFuncDesc)
		return ret;

	return pFuncDesc->mArgumentTypes[argIdx];
}


KSC_TypeInfo KSC_GetStructTypeByName(const char* structName, ModuleHandle hModule)
{
	KSC_TypeInfo ret = {SC::VarType::kInvalid, 0, 0, 0, NULL, NULL, false, false};
	KSC_ModuleDesc* pModule = hModule ? (KSC_ModuleDesc*)hModule : s_predefineModule;
	if (!pModule)
		return ret;

	if (pModule->mGlobalStructures.find(structName) != pModule->mGlobalStructures.end()) {
		KSC_StructDesc* pStructDesc = pModule->mGlobalStructures[structName];
		ret.type = SC::kStructure;
		ret.arraySize = 0;
		ret.sizeOfType = pStructDesc->mStructSize;
		ret.alignment = pStructDesc->mAlignment;
		ret.hStruct = pStructDesc;
		ret.typeString = NULL;
		ret.isRef = false;
		ret.isKSCLayout = true;

	}

	return ret;
}

KSC_TypeInfo KSC_GetStructMemberType(StructHandle hStruct, const char* member)
{
	KSC_TypeInfo ret = {SC::VarType::kInvalid, 0, 0, 0, NULL, NULL, false, false};
	KSC_StructDesc* pStructDesc = (KSC_StructDesc*)hStruct;
	if (!pStructDesc)
		return ret;
	std::vector<std::string> member_list;
	SplitStringByDot(member, member_list);

	KSC_TypeInfo memberType;
	size_t i = 0;
	for (; i < member_list.size(); ++i) {
		std::hash_map<std::string, KSC_StructDesc::MemberInfo>::iterator it_member = pStructDesc->mMemberIndices.find(member_list[i]);
		if (it_member != pStructDesc->mMemberIndices.end()) {
			memberType = (*pStructDesc)[it_member->second.idx];
			if (memberType.hStruct == NULL) {
				if (i != member_list.size() - 1)
					return ret;
				break;
			}
			pStructDesc = (KSC_StructDesc*)memberType.hStruct;
		}
		else
			return ret;
	}
	if (i == member_list.size()) return ret;

	return memberType;
}

void* KSC_GetStructMemberPtr(StructHandle hStruct, void* pStructVar, const char* member)
{
	int offset = 0;
	KSC_StructDesc* pStructDesc = (KSC_StructDesc*)hStruct;
	if (!pStructDesc)
		return NULL;

	std::vector<std::string> member_list;
	SplitStringByDot(member, member_list);

	KSC_TypeInfo memberType;
	size_t i = 0;
	for (; i < member_list.size(); ++i) {
		std::hash_map<std::string, KSC_StructDesc::MemberInfo>::iterator it_member = pStructDesc->mMemberIndices.find(member);
		if (it_member != pStructDesc->mMemberIndices.end()) {
			offset += it_member->second.mem_offset;
			memberType = (*pStructDesc)[it_member->second.idx];
			if (memberType.hStruct == NULL) {
				if (i != member_list.size() - 1)
					return NULL;
				break;
			}
		}
		else
			return NULL;
	}
	if (i == member_list.size()) return false;

	return ((unsigned char*)pStructVar + offset);
}

bool KSC_SetStructMemberData(StructHandle hStruct, void* pStructVar, const char* member, void* data, int size)
{
	int offset = 0;
	KSC_StructDesc* pStructDesc = (KSC_StructDesc*)hStruct;
	if (!pStructDesc)
		return false;

	std::vector<std::string> member_list;
	SplitStringByDot(member, member_list);

	int memSize = 0;
	KSC_TypeInfo memberType;
	size_t i = 0;
	for (; i < member_list.size(); ++i) {
		std::hash_map<std::string, KSC_StructDesc::MemberInfo>::iterator it_member = pStructDesc->mMemberIndices.find(member);
		if (it_member != pStructDesc->mMemberIndices.end()) {
			offset += it_member->second.mem_offset;
			memberType = (*pStructDesc)[it_member->second.idx];
			if (memberType.hStruct == NULL) {
				if (i != member_list.size() - 1)
					return false;
				memSize = it_member->second.mem_size;
				break;
			}
		}
		else
			return false;
	}
	if (i == member_list.size()) return false;

	memcpy(((unsigned char*)pStructVar + offset), data, size < memSize ? size : memSize);
	return true;
}

int KSC_GetStructSize(StructHandle hStruct)
{
	int offset = 0;
	KSC_StructDesc* pStructDesc = (KSC_StructDesc*)hStruct;
	if (!pStructDesc)
		return 0;
	return pStructDesc->mStructSize;
}

void* _Aligned_Malloc(size_t size, size_t align)
{
	void* ret = NULL;
#ifdef __GNUC__
	ret = posix_memalign(NULL, align, size);
#else
	ret = _aligned_malloc(size, align);
#endif
	if (ret)
		memset(ret, 0, size);

	return ret;
}

void _Aligned_Free(void* ptr)
{
#ifdef __GNUC__
	return free(ptr);
#else
	return _aligned_free(ptr);
#endif

}

void* KSC_AllocMemForType(const KSC_TypeInfo& typeInfo, int arraySize)
{
	return _Aligned_Malloc(typeInfo.sizeOfType * (arraySize == 0 ? 1 : arraySize), typeInfo.alignment);
}

void KSC_FreeMem(void* pData)
{
	_Aligned_Free(pData);
}

int KSC_GetTypePackedSize(const KSC_TypeInfo& typeInfo)
{
	if (typeInfo.hStruct == NULL) {
		return SC::TypePackedSize(typeInfo.type);
	}
	else {
		KSC_StructDesc* pStructDesc = (KSC_StructDesc*)typeInfo.hStruct;
		if (!pStructDesc)
			return 0;
		int packedSize = 0;
		std::vector<KSC_TypeInfo>::iterator it = pStructDesc->begin();
		for (; it != pStructDesc->end(); ++it) {
			if (it->hStruct == NULL) {
				packedSize += SC::TypePackedSize(it->type);
			}
			else {
				packedSize += KSC_GetTypePackedSize(*it);
			}
		}
		return packedSize;
	}
}

bool KSC_GetBuiltInTypeInfo(SC::VarType type, int& alloc_size, int& alignment)
{
	llvm::Type* pType = SC::CG_Context::ConvertToLLVMType(type);
	if (pType) {
		alloc_size = (int)SC::CG_Context::TheDataLayout->getTypeAllocSize(pType);
		alignment = (int)SC::CG_Context::TheDataLayout->getPrefTypeAlignment(pType);
		return true;
	}
	else
		return false;
}

int KSC_GetSIMDWidth()
{
	int CPUInfo[4];
	__cpuid(CPUInfo, 1);

	bool hasSSE = (CPUInfo[3] & (1 << 25)) ? true : false;
	const unsigned AVXBits = (1 << 27) | (1 << 28);
	bool HasAVX = ((CPUInfo[2] & AVXBits) == AVXBits);

	return HasAVX ? 8 : (hasSSE ? 4 : 1);
}