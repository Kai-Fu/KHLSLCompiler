#include "global_symbols.h"

namespace SC {
	uint64_t GobalSymbolMemManager::getSymbolAddress(const std::string &Name)
	{
		auto it = mGlobalFuncSymbols.find(Name);
		if (it != mGlobalFuncSymbols.end())
			return (uint64_t)it->second;
		else
			return 0;
	}
}