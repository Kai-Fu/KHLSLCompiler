#pragma once
#pragma warning(push)

#pragma warning(disable: 4267)
#include <llvm/ExecutionEngine/SectionMemoryManager.h>
#pragma warning(pop)
#include <hash_map>


namespace SC {
	class GobalSymbolMemManager : public llvm::SectionMemoryManager
	{
	public:
		std::hash_map<std::string, void*> mGlobalFuncSymbols;

	public:
		 virtual uint64_t getSymbolAddress(const std::string &Name);
	};
}

