
#include <iostream>
#include <array>
#include <clang-c/Index.h>
#include <string>
#include <set>
#include <vector>
#include <map>

#include<functional>

struct Visitor_Content
{
	Visitor_Content(std::string name = "", Visitor_Content* pParent = nullptr)
		:m_name(name), m_pParent(pParent)
	{
		if (m_pParent)
			m_pParent->m_setChild.insert(this);
	}
	~Visitor_Content()
	{
		destory();
	}
	void destory()
	{
		for (auto v : m_setChild)
		{
			delete v;
		}
		m_setChild.clear();
	}

	bool bClass = false;
	bool bNameSpace = false;
	std::string m_name;
	Visitor_Content* m_pParent = nullptr;
	std::set<Visitor_Content*> m_setChild;
	typedef std::vector<std::string> ParamsDefaultVal;
	struct OverLoadData
	{
		bool is_static;
		std::string func_type;
		std::string funcptr_type;
		ParamsDefaultVal default_val;
	};
	std::map<std::string, std::vector<OverLoadData> > m_vecFuncName;
	std::map<std::string, std::vector<OverLoadData> > m_vecConName;
	std::map<std::string, bool > m_vecValName;
	std::vector<std::string> m_vecInhName;
	std::vector<std::string> m_vecEnumName;
};


CXTranslationUnit TU;
typedef std::set<int> export_line_set;
std::map<std::string, export_line_set> g_export_loc;

std::string visit_param(CXCursor cursor, int params_idx)
{
	//std::string varname = clang_getCString(clang_getCursorSpelling(cursor));

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
			std::string p = clang_getCString(clang_getTokenSpelling(TU, token_i));
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
			std::string p = clang_getCString(clang_getTokenSpelling(TU, token_i));
			default_val += p;
		}
	}

	clang_disposeTokens(TU, tokens, numtokens);

	return default_val;
}

void visit_function(CXCursor cursor, Visitor_Content* pContent)
{
	std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
	std::string typestr = clang_getCString(clang_getTypeSpelling(clang_getCursorType(cursor)));
	auto& refSetOverLoad = pContent->m_vecFuncName[nsname];
	Visitor_Content::OverLoadData overload_data;
	{
		overload_data.funcptr_type += clang_getCString(clang_getTypeSpelling(clang_getResultType(clang_getCursorType(cursor))));

		if (pContent->bClass == true)
		{
			overload_data.funcptr_type += "(";
			overload_data.funcptr_type += pContent->m_name;
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

			overload_data.funcptr_type += clang_getCString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
			if(i < nArgs-1)
				overload_data.funcptr_type += ", ";

			std::string default_val = visit_param(args_cursor, i);
			if (default_val.empty() == false)
				overload_data.default_val.push_back(default_val);
		}
	}
	overload_data.funcptr_type += ")";
	refSetOverLoad.emplace_back(std::move(overload_data));


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
	std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
	std::string typestr = clang_getCString(clang_getTypeSpelling(clang_getCursorType(cursor)));
	auto& refSetOverLoad = pContent->m_vecConName[nsname];
	Visitor_Content::OverLoadData overload_data;
	//reg gloabl_func
	int nArgs = clang_Cursor_getNumArguments(cursor);
	{
		for (int i = 0; i < nArgs; i++)
		{
			CXCursor args_cursor = clang_Cursor_getArgument(cursor, i);
			overload_data.func_type += ", ";
			overload_data.func_type += clang_getCString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
			std::string default_val = visit_param(args_cursor, i);
			if (default_val.empty() == false)
				overload_data.default_val.push_back(default_val);
		}
	}

	refSetOverLoad.emplace_back(std::move(overload_data));
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
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
			//clang_getEnumConstantDeclValue(cursor);
			pContent->m_vecEnumName.push_back(nsname);
		}
		break;
	default:
		break;
	}

	return CXChildVisit_Continue;
}

enum CXChildVisitResult TU_visitor(CXCursor cursor,
	CXCursor parent,
	CXClientData client_data)
{
	Visitor_Content* pContent = (Visitor_Content*)client_data;

	auto source_loc = clang_getCursorLocation(cursor);
	if (clang_Location_isInSystemHeader(source_loc))
		return CXChildVisit_Continue;

	auto kind = clang_getCursorKind(cursor);
	switch (kind)
	{
	case CXCursor_MacroExpansion:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
			if (nsname == "export_lua")
			{
				CXFile file;
				unsigned line;
				unsigned column;
				unsigned offset;
				clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
				std::string filename = clang_getCString(clang_getFileName(file));
				auto& refSet = g_export_loc[filename];
				refSet.insert(line);
			}

		}
		break;
	case CXCursor_Namespace:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));

			//reg class
			Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent);
			clang_visitChildren(cursor, &TU_visitor, pNewContent);

		}
		break;
	case CXCursor_UnionDecl:
		break;
	case CXCursor_StructDecl:
	case CXCursor_ClassDecl:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));

			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				//reg class
				Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent);
				pNewContent->bClass = true;
				clang_visitChildren(cursor, &TU_visitor, pNewContent);
			}

		}
		break;
	case CXCursor_CXXMethod:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				visit_function(cursor, pContent);
			}
		}
		break;
	case CXCursor_Constructor:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				visit_constructor(cursor, pContent);
			}
		}
		break;
	case CXCursor_Destructor:
		{

		}
		break;
	case CXCursor_FieldDecl:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				pContent->m_vecValName[nsname] = false;;
			}
		}
		break;
	case CXCursor_CXXBaseSpecifier:
		{
			//reg inh
			std::string nsname = clang_getCString(clang_getCursorDisplayName(cursor));
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				pContent->m_vecInhName.push_back(nsname);
			}
		}
		break;
	case CXCursor_EnumDecl:
	case CXCursor_EnumConstantDecl:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));

			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				//reg global val;	
				clang_visitChildren(cursor, &visit_enum, pContent);
			}

		}
		break;
	case CXCursor_VarDecl:
		{
			std::string nsname = clang_getCString(clang_getCursorSpelling(cursor));

			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				//reg global val;
				pContent->m_vecValName[nsname] = true;

			}

		}
		break;
	case CXCursor_FunctionDecl:
		{
			CXFile file;
			unsigned line;
			unsigned column;
			unsigned offset;
			clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
			std::string filename = clang_getCString(clang_getFileName(file));
			auto& refSet = g_export_loc[filename];
			if (refSet.find(line) != refSet.end())
			{
				visit_function(cursor, pContent);
			}


		}
		break;
	default:
		{
			//std::string kindname = clang_getCString(clang_getCursorKindSpelling(kind));
			//printf("skip:%d %s\n", kind, kindname.c_str());
		}
		break;
	}
	return CXChildVisit_Continue;
}

void visit_contnet(Visitor_Content* pContent, std::string& os)
{
	char szBuf[4096];
	//class add
	if(pContent->bClass)
	{
		sprintf_s(szBuf, 4096, "luatinker::class_add<%s>(L, \"%s\",true);\n", pContent->m_name.c_str(), pContent->m_name.c_str());	//class_add
		os += szBuf;
	}
	//global_func
	if (pContent->m_name.empty())
	{
		if (pContent->bClass == false)
		{
			for (const auto& v : pContent->m_vecFuncName)
			{
				const auto& refDataSet = v.second;
				if (refDataSet.size() == 1) //no overload
				{
					const auto& refData = refDataSet[0];
					if (refData.default_val.empty())
					{
						sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\",&%s);\n", v.first.c_str(), v.first.c_str());	//normal
						os += szBuf;
					}
					else
					{
						std::string def_params;
						for (const auto& dv : refData.default_val)
						{
							if (def_params.empty() == false)
								def_params += ", ";
							def_params += dv;
						}
						sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\",&%s, %s);\n", v.first.c_str(), v.first.c_str(), def_params.c_str());
						os += szBuf;
					}

				}
				else
				{
					//overload
					std::string overload_params;
					for (const auto& refData : refDataSet)
					{
						std::string def_params;
						for (const auto& dv : refData.default_val)
						{
							if (def_params.empty() == false)
								def_params += ", ";
							def_params += dv;
						}
						if (overload_params.empty() == false)
							overload_params += " ,";
						sprintf_s(szBuf, 4096, "(%s)(&%s)", refData.funcptr_type.c_str(), v.first.c_str());
						overload_params += szBuf;
					}

					sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\", lua_tinker::args_type_overload_functor(%s));\n", v.first.c_str(), overload_params.c_str());
					os += szBuf;
				}


			}
		}
		else
		{
			for (const auto& v : pContent->m_vecFuncName)
			{
				const auto& refDataSet = v.second;
				if (refDataSet.size() == 1) //no overload
				{
					const auto& refData = refDataSet[0];
					if (refData.default_val.empty())
					{
						sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\",&%s::%s);\n", v.first.c_str(), pContent->m_name.c_str(), v.first.c_str());	//normal
						os += szBuf;
					}
					else
					{
						std::string def_params;
						for (const auto& dv : refData.default_val)
						{
							if (def_params.empty() == false)
								def_params += ", ";
							def_params += dv;
						}
						sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\",&%s::%s, %s);\n", v.first.c_str(), pContent->m_name.c_str(), v.first.c_str(), def_params.c_str());
						os += szBuf;
					}

				}
				else
				{
					//overload
					std::string overload_params;
					for (const auto& refData : refDataSet)
					{
						std::string def_params;
						for (const auto& dv : refData.default_val)
						{
							if (def_params.empty() == false)
								def_params += ", ";
							def_params += dv;
						}
						if (overload_params.empty() == false)
							overload_params += " ,";
						sprintf_s(szBuf, 4096, "(%s)(&%s::%s)", refData.funcptr_type.c_str(), pContent->m_name.c_str(),v.first.c_str());
						overload_params += szBuf;
					}

					sprintf_s(szBuf, 4096, "luatinker::def(L, \"%s\", lua_tinker::args_type_overload_functor(%s));\n", v.first.c_str(), overload_params.c_str());
					os += szBuf;
				}


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
				const auto& refData = refDataSet[0];
				if (refData.default_val.empty())
				{
					if(refData.is_static)
						sprintf_s(szBuf, 4096, "luatinker::class_def_static<%s>(L, \"%s\",&%s::%s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str());	//normal
					else
						sprintf_s(szBuf, 4096, "luatinker::class_def<%s>(L, \"%s\",&%s::%s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str());	//normal
					os += szBuf;
				}
				else
				{
					std::string def_params;
					for (const auto& dv : refData.default_val)
					{
						if (def_params.empty() == false)
							def_params += ", ";
						def_params += dv;
					}
					if (refData.is_static)
						sprintf_s(szBuf, 4096, "luatinker::class_def_static<%s>(L, \"%s\",&%s::%s, %s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str(), def_params.c_str());
					else
						sprintf_s(szBuf, 4096, "luatinker::class_def<%s>(L, \"%s\",&%s::%s, %s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str(), def_params.c_str());
					os += szBuf;
				}

			}
			else
			{
				//overload
				std::string overload_params;
				for (const auto& refData : refDataSet)
				{
					std::string def_params;
					for (const auto& dv : refData.default_val)
					{
						if (def_params.empty() == false)
							def_params += ", ";
						def_params += dv;
					}
					if (overload_params.empty() == false)
						overload_params += " ,";
					sprintf_s(szBuf, 4096, "(%s)(&%s::%s)", refData.funcptr_type.c_str(), pContent->m_name.c_str(), v.first.c_str());
					overload_params += szBuf;
				}
				
				sprintf_s(szBuf, 4096, "luatinker::class_def<%s>(L, \"%s\", lua_tinker::args_type_overload_member_functor(%s));\n",
						pContent->m_name.c_str(), v.first.c_str(), overload_params.c_str());
				

				os += szBuf;
			}


		}
	}
	
	for (const auto& v : pContent->m_vecConName)
	{
		const auto& refDataSet = v.second;
		if (refDataSet.size() == 1) //no overload
		{
			const auto& refData = refDataSet[0];
			if (refData.default_val.empty())
			{
				sprintf_s(szBuf, 4096, "luatinker::class_con<%s>(L, lua_tinker::constructor<%s%s>::invoke);\n", pContent->m_name.c_str(), pContent->m_name.c_str(), refData.func_type.c_str());	//normal
				os += szBuf;
			}
			else
			{
				std::string def_params;
				for (const auto& dv : refData.default_val)
				{
					if (def_params.empty() == false)
						def_params += ", ";
					def_params += dv;
				}
				sprintf_s(szBuf, 4096, "luatinker::class_con<%s>(L, lua_tinker::constructor<%s%s>::invoke, %s);\n", pContent->m_name.c_str(), pContent->m_name.c_str(), refData.func_type.c_str(), def_params.c_str());	//normal
				os += szBuf;
			}

		}
		else
		{
			//overload
			std::string overload_params;
			for (const auto& refData : refDataSet)
			{
				std::string def_params;
				for (const auto& dv : refData.default_val)
				{
					if (def_params.empty() == false)
						def_params += ", ";
					def_params += dv;
				}
				sprintf_s(szBuf, 4096, "lua_tinker::constructor<%s%s>()", pContent->m_name.c_str(), refData.func_type.c_str());	//normal
				if (overload_params.empty() == false)
					overload_params += ", ";
				overload_params += szBuf;
			}

			sprintf_s(szBuf, 4096, "luatinker::class_con<%s>(L,lua_tinker::args_type_overload_constructor(%s))\n", v.first.c_str(), overload_params.c_str());
			os += szBuf;
		}


	}

	if (pContent->m_name.empty())
	{
		for (const auto& v : pContent->m_vecValName)
		{
			sprintf_s(szBuf, 4096, "luatinker::set(L,\"%s\",%s);\n", v.first.c_str(), v.first.c_str());
			os += szBuf;
		}
	}
	else
	{
		for (const auto& v : pContent->m_vecValName)
		{
			if(v.second == true)
				sprintf_s(szBuf, 4096, "luatinker::class_mem_static<%s>(L,\"%s\",%s::%s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str());
			else
				sprintf_s(szBuf, 4096, "luatinker::class_mem<%s>(L,\"%s\",%s::%s);\n", pContent->m_name.c_str(), v.first.c_str(), pContent->m_name.c_str(), v.first.c_str());
			os += szBuf;
		}
	}

	for (const auto& v : pContent->m_vecInhName)
	{
		sprintf_s(szBuf, 4096, "luatinker::class_inh<%s,%s>(L);\n", pContent->m_name.c_str(), v.c_str());
		os += szBuf;
	}
	for (const auto& v : pContent->m_vecEnumName)
	{
		sprintf_s(szBuf, 4096, "luatinker::set(L, \"%s\",%s);\n", v.c_str(), v.c_str());
		os += szBuf;
	}

	for (auto v : pContent->m_setChild)
	{
		visit_contnet(v, os);
	}

}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::cout << "need source file name" << std::endl;
		return 0;
	}

	const char* szParam[] =
	{
		"-xc++",
	};
	std::string os;
	char szBuf[4096];
	os += "//this file was auto generated, plz don't modify it\n";

	Visitor_Content content;
	CXIndex Index = clang_createIndex(0, 0);
	int nIdx = 1;
	while (nIdx < argc)
	{
		TU = clang_parseTranslationUnit(Index, argv[nIdx], (const char**)&szParam, 1,
			0, 0, CXTranslationUnit_DetailedPreprocessingRecord | CXTranslationUnit_SkipFunctionBodies);


		CXCursor C = clang_getTranslationUnitCursor(TU);
		clang_visitChildren(C, TU_visitor, &content);

		clang_disposeTranslationUnit(TU);
		sprintf_s(szBuf, 4096, "#include \"%s\"\n", argv[nIdx]);	//normal
		os += szBuf;

		nIdx++;
	}
	clang_disposeIndex(Index);

	os += "void export_to_lua()\n";
	os += "{\n";

	visit_contnet(&content,os);
	os += "}\n";

	printf("%s", os.c_str());
	return 0;
}