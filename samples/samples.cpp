// SC.cpp : Defines the entry point for the console application.
//

#include <stdio.h>
#include "SC_API.h"
#include <string.h>
#include <vector>

void CompareTwoInt(int a, int b)
{
	int ret = a - b;
	printf("test value is %d (%d, %d)", ret, a, b);
}


int main(int argc, char* argv[])
{
	std::vector<char> common_code;
	{
		// Process the common.fx file
		FILE* f = NULL;
		fopen_s(&f, "common.fx", "r");
		if (f == NULL)
			return NULL;
		fseek(f, 0, SEEK_END);
		long len = ftell(f);
		fseek(f, 0, SEEK_SET);

		common_code.resize(len + 1);
		common_code[0] = '\0';
		size_t readSize = fread(&common_code.front(), 1, len, f);
		fclose(f);
		if (readSize > 0) {
			common_code[readSize] = '\0';
		}
	}

	
	KSC_Initialize(&common_code.front());
	KSC_AddExternalFunction("CompareTwoInt", CompareTwoInt);

	FILE* f = NULL;
	const char* fileNameBase = "test_";
	const char* fileNameExt = ".fx";
	
	int test_cast_idx = 0;
	while (1) {
		char fileNameBuf[200];
		sprintf_s(fileNameBuf, "%s%.2d%s", fileNameBase, test_cast_idx, fileNameExt);
		
		ModuleHandle hModule = KSC_CompileFile(fileNameBuf);
		if (!hModule) {
			printf(KSC_GetLastErrorMsg());
			return -1;
		}

		typedef void (*PFN_run_test)();
		FunctionHandle hFunc = KSC_GetFunctionHandleByName("run_test", hModule);
		PFN_run_test run_test = (PFN_run_test)KSC_GetFunctionPtr(hFunc, true);
			
		printf("Test %.2d:", test_cast_idx);
		run_test();
		printf("\n");
		++test_cast_idx;
	}

	KSC_Destory();
	return 0;
}

