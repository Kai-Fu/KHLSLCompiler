#pragma once
#include <stdio.h>

#ifdef KSC_EXPORTS
#define KSC_API __declspec(dllexport)
#else
#define KSC_API
#endif

/*!
	This file contains the APIs for KSC(Kai's Shader Compiler).
	The primary goal of KSC is to provide the compilation and JIT support for HLSL-like language, 
	so that the hosting C++ application can compile the HLSL-like code on the fly and JIT the requested
	functions.
	The HLSL-like language, which KSC supports, is called KSCL. Its language spec is described in
	another document, however here in short you can assume it is a superset of C language with HLSL swizzling support.
	Several of vector types are native built-in types, such as float4, float3 and int4. Differing from C,
	the pointer type is not supported for safety reason(KSCL is a "safe" language, see more for this in KSCL sepc). 
	Passing-by-reference is supported and the JIT-ed function will treat the referenced arguments as C++ pointers, 
	thus it gives the ablity to modify the data from C++ side in KSCL-implemented functions.
	The KSC APIs are kept as simple as possible, you may refer to each API for the details.
*/

/**
	The module handle is the representation for one compiling session. The compiling infomation are kept within 
	the domain of module so different modules cannot share any information. 
*/
typedef void* ModuleHandle;

/**
	The structure handle is the representation of one user-defined structure type in KSCL.
*/
typedef void* StructHandle;

/**
	The function handle is the representation of one user-defined function in KSCL.
*/
typedef void* FunctionHandle;

namespace SC {
	// The following are the single-value types that KSC support.
	typedef float Float;
	typedef int Int;
	typedef int Boolean;
	
	// This enum contains all the supported types including vector types.
	// Note external type(kExternType) is treated as void pointer.
	enum VarType {
		kFloat,
		kFloat2,
		kFloat3,
		kFloat4,
		kFloat8 = kFloat4 + 4,

		kInt,
		kInt2,
		kInt3,
		kInt4,
		kInt8 = kInt4 + 4,

		kBoolean,
		kBoolean2,
		kBoolean3,
		kBoolean4,
		kBoolean8 = kBoolean4 + 4,

		kStructure,
		kVoid,
		kExternType,
		kInvalid
	};

}

/** 
	The type information retrieved KSC APIs.

	If the "type" is a structure, then the hStruct member will be set to non-zero, 
	which can be passed to APIs for other information.

	The "sizeOfType" takes the alignment requirement into account, which is platform-dependent
	as well as the "alignment".

	The "typeString" can be used to distinguish the actual external types so the hosting C++
	side can do actions based on it.

	The "isRef" flag indicates whether the function argument is passed by reference. In hosting C++ side,
	the passed-by-reference argument will be JIT-ed as pointer to the associated type. 

	The "isKSCLayout" flag indicates whether this function argument is depending on KSC internal implementation
	for different platforms. In hosting C++ side if this flag is set, passing the pointer of structure with the same declaration 
	as in KSCL code will not be guaranteed to work, unless the type alignment and the padding between structure members are respected.
	It is highly suggested to use the "KSC_AllocMemForType" API to allocate the KSC compatible memory layout of structure and use 
	"KSC_GetStructMemberPtr" and "KSC_SetStructMemberData" APIs to retrieve and modify the member data of KSC structures.
*/
struct KSC_TypeInfo
{
	SC::VarType type;
	int arraySize;
	int sizeOfType;
	int alignment;
	StructHandle hStruct;
	const char* typeString;
	bool isRef;
	bool isKSCLayout;
};

extern "C" {

	/**
		The initialization function of KSC. It should be called before any other APIs get called.
		The argument "sharedCode" is the code that will be shared between multiple modules, e.g. some global
		functions or structure definitions. If the shared code contains bad syntax this function will fail.
	*/
	KSC_API bool KSC_Initialize(const char* sharedCode = NULL);

	/**
		The destroy function of KSC. It should be called when the client application is done for KSC,
		which means all the handles, type information as well as JIT-ed functions are invalid after
		the invoking of this function. For applications that initialize KSC only once, there's no need
		to call this function before exit since the resource is auto-cleaned when the application quits.
	*/
	KSC_API void KSC_Destory();

	/**
		If any KSC API fails for whatever reason including compiling error, call this function to retrieve
		the error message.
	*/
	KSC_API const char* KSC_GetLastErrorMsg();

	/**
		There're cases that the client application wants its KSC code to be able to invoke a function that is
		implemented in hosting C++ code. Use this function to tell KSC the function pointer and its function name
		that will link with KSCL.
		In KSCL side, you still need to declare the function without body implementation in order to let KSC know
		it should look up in the external symbol for the implementation of this function.
	*/
	KSC_API bool KSC_AddExternalFunction(const char* funcName, void* funcPtr);

	/**
		This function compiles the KSCL code, it will return the module handle on succeed otherwise return NULL.
	*/
	KSC_API ModuleHandle KSC_Compile(const char* sourceCode);

	/**
		This funtion is to JIT the function with the function handle specified.
	*/
	KSC_API void* KSC_GetFunctionPtr(FunctionHandle hFunc, bool bDump = false);

	/**
		This function returns the function handle with the specified name. If the function with the name is not
		found in the KSCL code, NULL will be returned.
	*/
	KSC_API FunctionHandle KSC_GetFunctionHandleByName(const char* funcName, ModuleHandle hModule);

	/**
		This function returns the function argument count.
	*/
	KSC_API int KSC_GetFunctionArgumentCount(FunctionHandle hFunc);

	/**
		This function returns the argument type info with the argument index specifed. 
		Note the caller must ensure the "argIdx" in range otherwise unexpected result will occur.
	*/
	KSC_API KSC_TypeInfo KSC_GetFunctionArgumentType(FunctionHandle hFunc, int argIdx);

	/**
		This function returns the structure handle with the name specifed.
	*/
	KSC_API KSC_TypeInfo KSC_GetStructTypeByName(const char* structName, ModuleHandle hModule);

	/**
		This function returns the structure member type info with the member name specified. If the member
		itself is another structure type, the "hStruct" in the return type info will be set to non-zero. 
		The hosting C++ code can recursively retrieve the member information of the nested structure.
	*/
	KSC_API KSC_TypeInfo KSC_GetStructMemberType(StructHandle hStruct, const char* member);

	/**
		This function allocates the memory regarding the type's alignment requirement.
	*/
	KSC_API void* KSC_AllocMemForType(const KSC_TypeInfo& typeInfo, int arraySize);
	/**
		This function frees the member allocated by "KSC_AllocMemForType".
	*/
	KSC_API void KSC_FreeMem(void* pData);

	/**
		This function returns the pointer to member variables by calculating the internal memory offset based on 
		the pointer to the structure data.
	*/
	KSC_API void* KSC_GetStructMemberPtr(StructHandle hStruct, void* pStructVar, const char* member);

	/**
		This function modifies the member variable with the content pointed by "data".
		Note the caller must ensure the provided buffer is large enough for the type of the member variable.
	*/
	KSC_API bool KSC_SetStructMemberData(StructHandle hStruct, void* pStructVar, const char* member, void* data, int size);

	/**
		This function returns the KSC structure size(not the one of the same declaration in your host C++ code).
	*/
	KSC_API int KSC_GetStructSize(StructHandle hStruct);

	/**
		This function returns the packed structure size, it should be the same as if it is declared in C++ code(supposing the alignment is one).
	*/
	KSC_API int KSC_GetTypePackedSize(const KSC_TypeInfo& typeInfo);

	/**
		This function returns the allocation size and alignment requirement for the built-in types.
	*/
	KSC_API bool KSC_GetBuiltInTypeInfo(SC::VarType type, int& alloc_size, int& alignment);

	/**
		This function returns the optimized SIMD width on the target machine.
	*/
	KSC_API int KSC_GetSIMDWidth();

}

