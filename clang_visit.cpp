
#include <iostream>
#include <fstream>
#include <array>
#include <clang-c/Index.h>
#include <string>
#include <set>
#include <vector>
#include <map>
#include<chrono>
#include<functional>
std::string output_filename;
bool g_bDebug = false;
bool g_bDefault_Params = true;
bool g_bJustDisplay = false;
std::string g_strKeyword = "export_lua";

std::string getClangString(CXString str)
{
	const char* pStr = clang_getCString(str);
	std::string strString;
	if (pStr)
	{
		strString.assign(pStr);
	}
	clang_disposeString(str);
	return strString;
}


struct Visitor_Content
{
	Visitor_Content(std::string name = "", Visitor_Content* pParent = nullptr, std::string accessname = "")
		:m_name(name), m_pParent(pParent)
		, m_accessname(accessname)
	{
		if (m_pParent)
			m_pParent->m_setChild[name] = this;
	}
	~Visitor_Content()
	{
		destory();
	}
	void destory()
	{
		for (auto& v : m_setChild)
		{
			delete v.second;
		}
		m_setChild.clear();
	}

	std::string getWholeName()
	{
		if (m_pParent == nullptr)
			return m_name;
		return m_pParent->getWholeName() + m_name;
	}
	std::string getAccessName()
	{
		if (bNameSpace)
		{
			return m_pParent->getAccessName() + "::" + m_accessname;
		}
		return m_accessname;
	}
	std::string getAccessPrifix()
	{
		if (bNameSpace)
		{
			if (m_pParent->getAccessPrifix().empty())
				return m_accessname + "::";
			return m_pParent->getAccessPrifix() + m_accessname + "::";
		}
		if (m_accessname.empty())
			return "";
		return m_accessname + "::";
	}

	std::string getWholePrifix()
	{
		if (m_pParent == nullptr)
			return "";
		return m_pParent->getWholeName();
	}

	bool hasChild(const std::string& name)
	{
		return m_setChild.find(name) != m_setChild.end();
	}

	bool bClass = false;
	bool bNameSpace = false;
	std::string m_name;
	std::string m_accessname;
	Visitor_Content* m_pParent = nullptr;
	std::map<std::string, Visitor_Content*> m_setChild;
	typedef std::vector<std::string> ParamsDefaultVal;
	struct OverLoadData
	{
		bool is_static;
		std::string func_type;
		std::string funcptr_type;
		ParamsDefaultVal default_val;
	};
	std::map<std::string, std::map<std::string, OverLoadData> > m_vecFuncName;
	std::map<std::string, std::map<std::string, OverLoadData> > m_vecConName;
	std::map<std::string, bool > m_vecValName;
	std::set<std::string> m_vecInhName;
	std::set<std::string> m_vecEnumName;
};


CXTranslationUnit TU;
typedef std::set<int> export_line_set;
std::map<CXFileUniqueID, export_line_set> g_export_loc;

std::string get_default_params(CXCursor cursor, int params_idx)
{
	//std::string varname = getClangString(clang_getCursorSpelling(cursor));

	CXSourceRange range = clang_getCursorExtent(cursor);
	unsigned numtokens;
	CXToken *tokens;
	clang_tokenize(TU, range, &tokens, &numtokens);

	int defaultTokenStart = 0;
	int defaultTokenEnd = 0;
	// This tokensequence needs some cleanup. There may be a default value
	// included, and sometimes there are extra tokens.
	for (unsigned int i = 0; i < numtokens; ++i)
	{
		auto token_i = tokens[i];
		if (clang_getTokenKind(token_i) == CXToken_Punctuation)
		{
			std::string p = getClangString(clang_getTokenSpelling(TU, token_i));
			if (p == "=")
			{
				defaultTokenStart = i + 1;
				defaultTokenEnd = numtokens;
				break;
			}
		}
	}

	std::string default_val;
	if (defaultTokenStart > 0)
	{
		for (int i = defaultTokenStart; i < defaultTokenEnd - 1; ++i)
		{
			auto token_i = tokens[i];
			std::string p = getClangString(clang_getTokenSpelling(TU, token_i));
			default_val += p;
		}
	}

	clang_disposeTokens(TU, tokens, numtokens);

	return default_val;
}

void visit_function(CXCursor cursor, Visitor_Content* pContent)
{
	std::string nsname = getClangString(clang_getCursorSpelling(cursor));
	std::string typestr = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
	auto& refMap = pContent->m_vecFuncName[nsname];
	if (refMap.find(typestr) != refMap.end())
		return;
	auto& overload_data = refMap[typestr];
	{
		overload_data.funcptr_type = getClangString(clang_getTypeSpelling(clang_getResultType(clang_getCursorType(cursor))));

		if (pContent->bClass == true)
		{
			overload_data.funcptr_type += "(";
			overload_data.funcptr_type += pContent->getAccessName();
			overload_data.funcptr_type += "::*)(";

		}
		else
		{
			overload_data.funcptr_type += "(*)(";
		}

	}
	overload_data.func_type = typestr;
	overload_data.is_static = clang_CXXMethod_isStatic(cursor) != 0;
	//reg gloabl_func
	int nArgs = clang_Cursor_getNumArguments(cursor);
	{
		for (int i = 0; i < nArgs; i++)
		{
			CXCursor args_cursor = clang_Cursor_getArgument(cursor, i);

			overload_data.funcptr_type += getClangString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
			if (i < nArgs - 1)
				overload_data.funcptr_type += ", ";

			if (g_bDefault_Params == true)
			{
				std::string default_val = get_default_params(args_cursor, i);
				if (default_val.empty() == false)
					overload_data.default_val.push_back(default_val);
			}
		}
	}
	overload_data.funcptr_type += ")";
	if (clang_CXXMethod_isConst(cursor))
	{
		overload_data.funcptr_type += "const";
	}


	CXCursor* overridden_cursors;
	unsigned num_overridden = 0;
	clang_getOverriddenCursors(cursor, &overridden_cursors, &num_overridden);
	for (unsigned int i = 0; i < num_overridden; i++)
	{
		CXCursor overridden = overridden_cursors[i];
		visit_function(overridden, pContent);
	}
	if (num_overridden > 0)
	{
		clang_disposeOverriddenCursors(overridden_cursors);
	}

}

void visit_constructor(CXCursor cursor, Visitor_Content* pContent)
{
	std::string nsname = getClangString(clang_getCursorSpelling(cursor));
	std::string typestr = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
	auto& refMap = pContent->m_vecConName[nsname];
	if (refMap.find(typestr) != refMap.end())
		return;
	auto& overload_data = refMap[typestr];
	//reg gloabl_func
	int nArgs = clang_Cursor_getNumArguments(cursor);
	{
		for (int i = 0; i < nArgs; i++)
		{
			CXCursor args_cursor = clang_Cursor_getArgument(cursor, i);
			overload_data.func_type += ", ";
			overload_data.func_type += getClangString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
			if (g_bDefault_Params == true)
			{
				std::string default_val = get_default_params(args_cursor, i);
				if (default_val.empty() == false)
					overload_data.default_val.push_back(default_val);
			}
		}
	}

}


enum CXChildVisitResult visit_enum(CXCursor cursor,
	CXCursor parent,
	CXClientData client_data)
{
	Visitor_Content* pContent = (Visitor_Content*)client_data;

	auto kind = clang_getCursorKind(cursor);
	switch (kind)
	{
	case CXCursor_EnumDecl:
	case CXCursor_EnumConstantDecl:
		{
			std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			//clang_getEnumConstantDeclValue(cursor);
			pContent->m_vecEnumName.insert(nsname);
		}
		break;
	default:
		break;
	}

	return CXChildVisit_Continue;
}


CXFileUniqueID g_lastfile = { 0,0,0 };
std::set<CXFileUniqueID> g_finish_file;
bool operator < (const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
	if (lft.data[0] == rht.data[0])
	{
		if (lft.data[1] == rht.data[1])
		{
			return lft.data[2] < rht.data[2];
		}
		else
			return lft.data[1] < rht.data[1];
	}
	else
		return lft.data[0] < rht.data[0];
}

bool operator > (const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
	if (lft.data[0] == rht.data[0])
	{
		if (lft.data[1] == rht.data[1])
		{
			return lft.data[2] > rht.data[2];
		}
		else
			return lft.data[1] > rht.data[1];
	}
	else
		return lft.data[0] > rht.data[0];
}

bool operator== (const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
	return (lft < rht) == false && (lft > rht) == false;
}
bool operator!= (const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
	return !(lft == rht);
}

void AddSkipFile(CXFileUniqueID id)
{
	g_finish_file.insert(id);
}

bool NeedSkipByFile(CXFileUniqueID id)
{
	auto itFind = g_finish_file.find(id);
	if (itFind != g_finish_file.end())
	{
		if (g_bDebug)
		{
			printf("skip:%llx %llx %llx \n", id.data[0], id.data[1], id.data[2]);
		}
		return true;
	}

	return false;
}


static void printString(const char *name, CXString string)
{
	const char *cstr = clang_getCString(string);
	if (cstr && *cstr) {
		printf("%s: %s ", name, cstr);
	}
	clang_disposeString(string);
}

static void printCursor(CXCursor cursor)
{
	CXFile file;
	unsigned int off, line, col;
	CXSourceLocation location = clang_getCursorLocation(cursor);
	if (clang_Location_isInSystemHeader(location))
		return;
	clang_getSpellingLocation(location, &file, &line, &col, &off);
	CXString fileName = clang_getFileName(file);
	const char *fileNameCStr = clang_getCString(fileName);
	if (fileNameCStr) {
		CXSourceRange range = clang_getCursorExtent(cursor);
		unsigned int start, end;
		clang_getSpellingLocation(clang_getRangeStart(range), 0, 0, 0, &start);
		clang_getSpellingLocation(clang_getRangeEnd(range), 0, 0, 0, &end);
		printf("%s:%d:%d (%d, %d-%d) ", fileNameCStr, line, col, off, start, end);
	}
	clang_disposeString(fileName);
	printString("kind", clang_getCursorKindSpelling(clang_getCursorKind(cursor)));
	printString("display name", clang_getCursorDisplayName(cursor));
	printString("usr", clang_getCursorUSR(cursor));
	if (clang_isCursorDefinition(cursor))
		printf("definition ");
	printf("\n");
}

static enum CXChildVisitResult visit_display(CXCursor cursor, CXCursor parent, CXClientData userData)
{
	(void)parent;
	int indent = *(int*)userData;
	int i;
	for (i = 0; i < indent; ++i) {
		printf("  ");
	}
	printCursor(cursor);
	CXCursor ref = clang_getCursorReferenced(cursor);
	if (!clang_isInvalid(clang_getCursorKind(ref)) && !clang_equalCursors(ref, cursor)) {
		for (i = 0; i < indent; ++i) {
			printf("  ");
		}
		printf("-> ");
		printCursor(ref);
	}
	++indent;
	clang_visitChildren(cursor, visit_display, &indent);
	return CXChildVisit_Continue;
}

void display_debug_cursor(CXCursor& cursor, CXCursorKind& kind, CXSourceLocation& source_loc)
{

	std::string kindname = getClangString(clang_getCursorKindSpelling(kind));

	CXFile file;
	unsigned line;
	unsigned column;
	unsigned offset;
	clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

	std::string filename = getClangString(clang_getFileName(file));
	std::string nsname = getClangString(clang_getCursorSpelling(cursor));
	printf("find:%d %s name:%s in file:%s\n", kind, kindname.c_str(), nsname.c_str(), filename.c_str());

}

bool g_bPreProcessing = true;
enum CXChildVisitResult TU_visitor(CXCursor cursor,
	CXCursor parent,
	CXClientData client_data)
{
	Visitor_Content* pContent = (Visitor_Content*)client_data;



	auto kind = clang_getCursorKind(cursor);

	if (g_bPreProcessing && clang_isPreprocessing(kind) == 0)
	{
		//first out of preprcessing check need export
		g_bPreProcessing = false;
		if (g_strKeyword.empty())
			return CXChildVisit_Continue;
		if (g_export_loc.empty())
			return CXChildVisit_Break;
	}


	switch (kind)
	{
	case CXCursor_MacroExpansion:
		{
			if (g_strKeyword.empty())
				return CXChildVisit_Continue;
			std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			if (nsname == g_strKeyword)
			{
				auto source_loc = clang_getCursorLocation(cursor);
				if (g_bDebug)
				{
					display_debug_cursor(cursor, kind, source_loc);
				}
				CXFile file;
				unsigned line;
				unsigned column;
				unsigned offset;
				clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

				CXFileUniqueID id;
				clang_getFileUniqueID(file, &id);

				if (NeedSkipByFile(id) == true)
					return CXChildVisit_Continue;


				auto& refSet = g_export_loc[id];
				refSet.insert(line);
			}

		}
		break;
	case CXCursor_Namespace:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;


			std::string nsname = getClangString(clang_getCursorSpelling(cursor));

			//reg class
			if (pContent->hasChild(nsname) == false)
			{
				Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent, nsname);
				pNewContent->bNameSpace = true;
				clang_visitChildren(cursor, &TU_visitor, pNewContent);
			}
			else
			{
				clang_visitChildren(cursor, &TU_visitor, pContent->m_setChild[nsname]);
			}

		}
		break;
	case CXCursor_UnionDecl:
		break;
	case CXCursor_StructDecl:
	case CXCursor_ClassDecl:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			//std::string filename = getClangString(clang_getFileName(file));

			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;
			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					return CXChildVisit_Continue;
				}
			}

			std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			std::string tname = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));

			//reg class
			if (pContent->hasChild(nsname) == false)
			{
				if (g_bDebug)
				{
					printf("do:class\n");
				}
				Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent, tname);
				pNewContent->bClass = true;
				clang_visitChildren(cursor, &TU_visitor, pNewContent);

			}
			else
			{
				if (g_bDebug)
				{
					printf("skip:class\n");
				}
			}


		}
		break;
	case CXCursor_CXXMethod:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			//std::string filename = getClangString(clang_getFileName(file));

			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					if (g_bDebug)
					{
						printf("skip:CXXMethod\n");
					}
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:CXXMethod\n");
					}
					return CXChildVisit_Continue;
				}
			}
			if (g_bDebug)
			{
				printf("do:CXXMethod\n");
			}
			visit_function(cursor, pContent);


		}
		break;
	case CXCursor_Constructor:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			//std::string filename = getClangString(clang_getFileName(file));
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:Constructor\n");
					}
					return CXChildVisit_Continue;
				}
			}
			if (g_bDebug)
			{
				printf("do:Constructor\n");
			}
			visit_constructor(cursor, pContent);

		}
		break;
	case CXCursor_Destructor:
		{
		}
		break;
	case CXCursor_FieldDecl:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			//std::string filename = getClangString(clang_getFileName(file));
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:FieldDecl\n");
					}
					return CXChildVisit_Continue;
				}
			}
			std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			pContent->m_vecValName[nsname] = false;
			if (g_bDebug)
			{
				printf("do:FieldDecl\n");
			}

		}
		break;
	case CXCursor_CXXBaseSpecifier:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;

			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			//reg inh
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

			//std::string filename = getClangString(clang_getFileName(file));
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:CXXBase\n");
					}
					return CXChildVisit_Continue;
				}
			}
			//std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			std::string tname = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
			pContent->m_vecInhName.insert(tname);
			if (g_bDebug)
			{
				printf("do:CXXBase\n");
			}

		}
		break;
	case CXCursor_EnumDecl:
	case CXCursor_EnumConstantDecl:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			//std::string filename = getClangString(clang_getFileName(file));
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:Enum\n");
					}
					return CXChildVisit_Continue;
				}
			}
			if (g_bDebug)
			{
				printf("do:Enum\n");
			}
			//reg global val;	
			clang_visitChildren(cursor, &visit_enum, pContent);


		}
		break;
	case CXCursor_VarDecl:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;
			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:VarDecl\n");
					}
					return CXChildVisit_Continue;
				}
			}
			if (g_bDebug)
			{
				printf("do:VarDecl\n");
			}
			//reg global val;
			std::string nsname = getClangString(clang_getCursorSpelling(cursor));
			//std::string tname = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
			pContent->m_vecValName[nsname] = true;



		}
		break;
	case CXCursor_FunctionDecl:
		{
			auto source_loc = clang_getCursorLocation(cursor);
			if (clang_Location_isInSystemHeader(source_loc))
				return CXChildVisit_Continue;

			if (g_bDebug)
			{
				display_debug_cursor(cursor, kind, source_loc);
			}
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			CXFileUniqueID id;
			clang_getFileUniqueID(file, &id);
			if (NeedSkipByFile(id) == true)
				return CXChildVisit_Continue;

			if (g_strKeyword.empty() == false)
			{
				auto itFind = g_export_loc.find(id);
				if (itFind == g_export_loc.end())
				{
					return CXChildVisit_Continue;
				}

				auto& refSet = itFind->second;
				if (refSet.find(line) == refSet.end())
				{
					if (g_bDebug)
					{
						printf("skip:funciton\n");
					}
					return CXChildVisit_Continue;
				}
			}

			if (g_bDebug)
			{
				printf("do:funciton\n");
			}
			visit_function(cursor, pContent);



		}
		break;
	default:
		{
		}
		break;
	}
	return CXChildVisit_Continue;
}


void visit_contnet(Visitor_Content* pContent, std::string& os)
{
	char szBuf[4096];
	//class add
	if (pContent->bClass)
	{
		sprintf_s(szBuf, 4096, "lua_tinker::class_add<%s>(L, \"%s\",true);\n", pContent->getAccessName().c_str(), pContent->getWholeName().c_str());	//class_add
		os += szBuf;
	}
	for (const auto& v : pContent->m_vecInhName)
	{

		sprintf_s(szBuf, 4096, "lua_tinker::class_inh<%s,%s>(L);\n", pContent->getAccessName().c_str(), v.c_str());
		os += szBuf;
	}

	//global_func

	if (pContent->bClass == false)
	{
		for (const auto& v : pContent->m_vecFuncName)
		{
			const auto& refDataSet = v.second;
			if (refDataSet.size() == 1) //no overload
			{
				const auto& refData = refDataSet.begin()->second;
				std::string def_params;
				for (const auto& dv : refData.default_val)
				{
					def_params += ", ";
					def_params += dv;
				}

				sprintf_s(szBuf, 4096, "lua_tinker::def(L, \"%s\",&%s%s);\n", (pContent->getWholeName() + v.first).c_str(), (pContent->getAccessPrifix() + v.first).c_str(), def_params.c_str());
				os += szBuf;

			}
			else
			{
				//overload
				std::string overload_params;
				std::string def_params;
				size_t nDefaultParamsStart = 1;
				for (const auto& it_refData : refDataSet)
				{
					const auto& refData = it_refData.second;
					std::string def_params_decl;
					for (const auto& dv : refData.default_val)
					{
						def_params += ", ";
						def_params += dv;
					}
					if (refData.default_val.empty() == false)
					{
						sprintf_s(szBuf, 4096, ",%u /*default_args_count*/, %u /*default_args_start*/ ", refData.default_val.size(), nDefaultParamsStart);
						nDefaultParamsStart += refData.default_val.size();
						def_params_decl = szBuf;
					}

					if (overload_params.empty() == false)
						overload_params += ", ";
					sprintf_s(szBuf, 4096, "\n\tlua_tinker::make_functor_ptr((%s)(&%s)%s)", refData.funcptr_type.c_str(), (pContent->getAccessPrifix() + v.first).c_str(), def_params_decl.c_str());
					overload_params += szBuf;
				}

				sprintf_s(szBuf, 4096, "lua_tinker::def(L, \"%s\", lua_tinker::args_type_overload_functor(%s)%s);\n", (pContent->getWholeName() + v.first).c_str(), overload_params.c_str(), def_params.c_str());
				os += szBuf;
			}


		}
	}
	else
	{
		//class CXXMethod
		for (const auto& v : pContent->m_vecFuncName)
		{

			const auto& refDataSet = v.second;
			if (refDataSet.size() == 1) //no overload
			{
				const auto& refData = refDataSet.begin()->second;
				std::string def_params;
				for (const auto& dv : refData.default_val)
				{
					def_params += ", ";
					def_params += dv;
				}


				if (refData.is_static)
					sprintf_s(szBuf, 4096, "lua_tinker::class_def_static<%s>(L, \"%s\",&%s%s);\n", pContent->getAccessName().c_str(), v.first.c_str(), (pContent->getAccessPrifix() + v.first).c_str(), def_params.c_str());
				else
					sprintf_s(szBuf, 4096, "lua_tinker::class_def<%s>(L, \"%s\",&%s%s);\n", pContent->getAccessName().c_str(), v.first.c_str(), (pContent->getAccessPrifix() + v.first).c_str(), def_params.c_str());
				os += szBuf;


			}
			else
			{
				//overload
				std::string overload_params;
				size_t nDefaultParamsStart = 1;
				std::string def_params;
				for (const auto& it_refData : refDataSet)
				{
					const auto& refData = it_refData.second;
					std::string def_params_decl;
					for (const auto& dv : refData.default_val)
					{
						def_params += ", ";
						def_params += dv;
					}
					if (refData.default_val.empty() == false)
					{
						sprintf_s(szBuf, 4096, ",%u /*default_args_count*/, %u /*default_args_start*/ ", refData.default_val.size(), nDefaultParamsStart);
						nDefaultParamsStart += refData.default_val.size();
						def_params_decl = szBuf;
					}

					if (overload_params.empty() == false)
						overload_params += ", ";
					sprintf_s(szBuf, 4096, "\n\tlua_tinker::make_member_functor_ptr((%s)(&%s)%s)", refData.funcptr_type.c_str(), (pContent->getAccessPrifix() + v.first).c_str(), def_params_decl.c_str());
					overload_params += szBuf;
				}

				sprintf_s(szBuf, 4096, "lua_tinker::class_def<%s>(L, \"%s\", lua_tinker::args_type_overload_member_functor(%s)%s);\n",
					pContent->getAccessName().c_str(), v.first.c_str(), overload_params.c_str(), def_params.c_str());


				os += szBuf;
			}


		}
	}

	for (const auto& v : pContent->m_vecConName)
	{


		const auto& refDataSet = v.second;
		if (refDataSet.size() == 1) //no overload
		{
			const auto& refData = refDataSet.begin()->second;

			std::string def_params;
			for (const auto& dv : refData.default_val)
			{
				if (def_params.empty() == false)
					def_params += ", ";
				def_params += dv;
			}
			sprintf_s(szBuf, 4096, "lua_tinker::class_con<%s>(L, lua_tinker::constructor<%s%s>::invoke%s);\n", pContent->getAccessName().c_str(), pContent->getAccessName().c_str(), refData.func_type.c_str(), def_params.c_str());	//normal
			os += szBuf;
		}
		else
		{
			//overload
			std::string overload_params;
			size_t nDefaultParamsStart = 1;
			std::string def_params;
			for (const auto& it_refData : refDataSet)
			{
				const auto& refData = it_refData.second;
				std::string def_params_decl;
				for (const auto& dv : refData.default_val)
				{
					def_params += ", ";
					def_params += dv;
				}
				if (refData.default_val.empty() == false)
				{
					sprintf_s(szBuf, 4096, "%u /*default_args_count*/, %u /*default_args_start*/ ", refData.default_val.size(), nDefaultParamsStart);
					nDefaultParamsStart += refData.default_val.size();
					def_params_decl = szBuf;
				}
				sprintf_s(szBuf, 4096, "\n\tnew lua_tinker::constructor<%s%s>(%s)", pContent->getAccessName().c_str(), refData.func_type.c_str(), def_params_decl.c_str());	//normal
				if (overload_params.empty() == false)
					overload_params += ", ";
				overload_params += szBuf;
			}

			sprintf_s(szBuf, 4096, "lua_tinker::class_con<%s>(L,lua_tinker::args_type_overload_constructor(%s)%s);\n", pContent->getAccessName().c_str(), overload_params.c_str(), def_params.c_str());
			os += szBuf;
		}


	}

	if (pContent->bClass == false)
	{
		for (const auto& v : pContent->m_vecValName)
		{
			sprintf_s(szBuf, 4096, "lua_tinker::set(L,\"%s\",%s);\n", (pContent->getWholeName() + v.first).c_str(), (pContent->getAccessPrifix() + v.first).c_str());
			os += szBuf;
		}
	}
	else
	{
		for (const auto& v : pContent->m_vecValName)
		{
			if (v.second == true)
				sprintf_s(szBuf, 4096, "lua_tinker::class_mem_static<%s>(L,\"%s\",&%s);\n", pContent->getAccessName().c_str(), v.first.c_str(), (pContent->getAccessPrifix() + v.first).c_str());
			else
				sprintf_s(szBuf, 4096, "lua_tinker::class_mem<%s>(L,\"%s\",&%s);\n", pContent->getAccessName().c_str(), v.first.c_str(), (pContent->getAccessPrifix() + v.first).c_str());
			os += szBuf;
		}
	}

	for (const auto& v : pContent->m_vecEnumName)
	{
		sprintf_s(szBuf, 4096, "lua_tinker::set(L, \"%s\",%s);\n", (pContent->getWholeName() + v).c_str(), (pContent->getAccessPrifix() + v).c_str());
		os += szBuf;
	}

	for (auto& v : pContent->m_setChild)
	{
		visit_contnet(v.second, os);
	}

}

void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters)
{
	// Skip delimiters at beginning.  
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".  
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);
	while (std::string::npos != pos || std::string::npos != lastPos)
	{
		// Found a token, add it to the vector.  
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"  
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"  
		pos = str.find_first_of(delimiters, lastPos);
	}
}

void visit_includes(CXFile included_file, CXSourceLocation* inclusion_stack, unsigned include_len, CXClientData client_data)
{
	CXFileUniqueID id;
	clang_getFileUniqueID(included_file, &id);
	AddSkipFile(id);

}

template<typename T, int N>
inline size_t sizeofArray(T(&_array)[N])
{
	return N;
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "need source file name" << std::endl;
		return 0;
	}

	std::vector<std::string> szCppFiles;

	std::vector<const char*> szParams;
	std::vector<std::string> szHeaderDirs;
	for (int i = 1; i < argc; i++)
	{
		std::string strCmd(argv[i]);
		if (strCmd[0] == '-') //cmd perfix
		{
			std::string cmdPerfix = strCmd.substr(0, 2);
			if (cmdPerfix == "-I")
			{
				szHeaderDirs.push_back(strCmd);
			}
			else if (cmdPerfix == "-v")
			{
				g_bDebug = true;
			}
			else if (cmdPerfix == "--")
			{
				std::string cmd = strCmd.substr(2);
				std::string::size_type pos = cmd.find_first_of('=');
				std::string cmd_first;
				std::string cmd_second;

				if (std::string::npos != pos)
				{
					cmd_first = cmd.substr(0, pos);
					cmd_second = cmd.substr(pos + 1);
				}
				else
				{
					cmd_first = cmd;
				}
				if (cmd_first == "include")
				{
					//read file
					std::ifstream input_file(fopen(cmd_second.c_str(), "r"));
					if (input_file.is_open())
					{
						std::string include_dirs;
						input_file >> include_dirs;
						Tokenize(include_dirs, szHeaderDirs, ";");
					}
				}
				else if (cmd_first == "cpps")
				{
					//read file
					std::ifstream input_file(fopen(cmd_second.c_str(), "r"));
					if (input_file.is_open())
					{
						std::string cpp_files;
						input_file >> cpp_files;
						Tokenize(cpp_files, szCppFiles, ";");
					}

				}
				else if (cmd_first == "output")
				{
					output_filename = cmd_second;
				}
				else if (cmd_first == "justdisplay")
				{
					if (cmd_second == "on")
					{
						g_bJustDisplay = true;
					}
				}
				else if (cmd_first == "keyword")
				{
					g_strKeyword = cmd_second;
				}
				else if (cmd_first == "default_params")
				{
					if (cmd_second == "off")
					{
						g_bDefault_Params = false;
					}
				}

				


			}
		}
		else
		{
			szCppFiles.push_back(strCmd);
		}
	}

	const char* ext_cxx_flag[] =
	{
		"-xc++",
		"-std=c++14",
		"-w",
		"-fno-spell-checking",
		"-fsyntax-only",
		//"-Dexport_lua="
	};

	for (auto& v : szHeaderDirs)
	{
		szParams.push_back(v.c_str());
	}
	for (size_t i = 0; i < sizeofArray(ext_cxx_flag); i++)
	{
		szParams.push_back(ext_cxx_flag[i]);
	}


	std::string os;

	Visitor_Content content;
	CXIndex Index = clang_createIndex(0, 0);

	for (auto& v : szCppFiles)
	{
		std::chrono::high_resolution_clock clock;
		if (output_filename.empty() == false)
		{
			printf("file:%s process start\n", v.c_str());
		}
		auto _startTime = clock.now();
		TU = clang_parseTranslationUnit(Index, v.c_str(), szParams.data(), szParams.size(),
			0, 0, CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies);
		auto _stopTime1 = clock.now();

		CXCursor C = clang_getTranslationUnitCursor(TU);
		if (g_bJustDisplay)
		{
			int indent = 0;
			clang_visitChildren(C, visit_display, &indent);
		}
		else
		{
			g_bPreProcessing = true;
			clang_visitChildren(C, TU_visitor, &content);
			g_export_loc.clear();

		}

		clang_getInclusions(TU, visit_includes, 0);

		clang_disposeTranslationUnit(TU);

		auto _stopTime2 = clock.now();
		if (output_filename.empty() == false)
		{
			printf("file:%s processed use %lld %lld\n", v.c_str(),
				std::chrono::duration_cast<std::chrono::milliseconds>(_stopTime1 - _startTime).count(),
				std::chrono::duration_cast<std::chrono::milliseconds>(_stopTime2 - _stopTime1).count());
		}
	}


	clang_disposeIndex(Index);
	if (g_bDebug)
	{
		printf("press any key to start process content");
		getchar();
	}

	visit_contnet(&content, os);
	if (g_bDebug)
	{
		printf("global:func:%d, child:%d", content.m_vecFuncName.size(), content.m_setChild.size());
		printf("press any key to start output");
		getchar();
	}


	if (output_filename.empty())
		printf("%s", os.c_str());
	else
	{
		std::ofstream output_file(fopen(output_filename.c_str(), "w+"));
		output_file << os;
	}
	if (g_bDebug)
	{
		printf("press any key to exit");
		getchar();
	}
	return 0;
}