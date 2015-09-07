#include "SymbolList.h"
#include "../XMUtil.h"


SymbolList::SymbolList(SymbolNameSpace *sns,Symbol *first,bool bang) : SymbolTableBase(sns)
{
	vSymbols.push_back(SymbolListEntry(first, bang));
   pMapRange = '\0';
}

SymbolList::SymbolList(SymbolNameSpace *sns, SymbolList *first) : SymbolTableBase(sns)
{
	vSymbols.push_back(SymbolListEntry(first));
	pMapRange = '\0';
}



SymbolList::~SymbolList(void)
{
// do nothing symbols in one hash table or another
}


void SymbolList::OutputComputable(ContextInfo *info)
{
	if (vSymbols.empty())
		return;
	*info << "[";
	for (size_t i = 0; i < vSymbols.size(); i++)
	{
		if (vSymbols[i].eType == EntryType_SYMBOL)
		{
			if (i)
				*info << ",";
			*info << vSymbols[i].u.pSymbol->GetName();
		}
	}
	*info << "]";
}
