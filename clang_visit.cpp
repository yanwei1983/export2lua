
#include <array>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include <clang-c/Index.h>

#include "AttempUtil.h"
#include "StringAlgo.h"
#include "get_opt.h"

std::set<std::string> g_export_class_name;

std::string output_filename;
std::string temp_filename;
bool        g_bDebug               = false;
bool        g_bSkip_default_params = false;
bool        g_bSkip_function       = false;
bool        g_bSkip_class          = false;
bool        g_bSkip_namespace      = false;
bool        g_bSkip_var            = false;
bool        g_bSkip_field          = false;
bool        g_bSkip_enum           = false;
bool        g_bSkip_method         = false;
bool        g_bSkip_method_static  = false;
bool        g_bSkip_con            = false;
bool        g_bSkip_overload       = false;

std::set<std::string> g_strExportNamespaceName;

std::set<std::string> g_strExportClassName;
bool                  g_bJustDisplay = false;
std::string           g_strKeyword   = "export_lua";
unsigned              g_extTUFlag    = 0;
std::string           getClangString(CXString str)
{
    const char* pStr = clang_getCString(str);
    std::string strString;
    if(pStr)
    {
        strString.assign(pStr);
    }
    clang_disposeString(str);
    return strString;
}

template<class... ARGS>
void debug_printf(ARGS&&... args)
{
    if(g_bDebug)
    {
        printf(std::forward<ARGS>(args)...);
    }
}
template<class... ARGS>
void debug_display_debug_cursor(ARGS&&... args)
{
    if(g_bDebug)
    {
        display_debug_cursor(std::forward<ARGS>(args)...);
    }
}

enum ContentType
{
    CT_GLOBAL,
    CT_CLASS,
    CT_NAMESPACE,
};
struct Visitor_Content
{
    Visitor_Content(const std::string& name = "", Visitor_Content* pParent = nullptr, const std::string& accessname = "", ContentType ct = CT_GLOBAL)
        : m_name(name)
        , m_pParent(pParent)
        , m_accessname(accessname)
        , m_ContentType(ct)
    {
        if(m_pParent)
        {
            if(m_ContentType == CT_CLASS)
                m_pParent->m_setChildClass[name] = this;
            else
                m_pParent->m_setChildNameSpace[name] = this;
        }
    }
    ~Visitor_Content() { destory(); }
    void destory()
    {
        for(auto& v: m_setChildClass)
        {
            delete v.second;
        }
        m_setChildClass.clear();
        for(auto& v: m_setChildNameSpace)
        {
            delete v.second;
        }
        m_setChildNameSpace.clear();
    }

    std::string getAccessName() { return m_accessname; }
    std::string getAccessPrifix()
    {
        if(m_accessname.empty())
            return "";
        return m_accessname + "::";
    }

    bool hasChildClass(const std::string& name) const { return m_setChildClass.find(name) != m_setChildClass.end(); }
    bool hasChildNameSpace(const std::string& name) const { return m_setChildNameSpace.find(name) != m_setChildNameSpace.end(); }

    ContentType                             m_ContentType;
    std::string                             m_name;
    std::string                             m_accessname;
    Visitor_Content*                        m_pParent = nullptr;
    std::vector<std::string>                m_aliasname_list;
    std::map<std::string, Visitor_Content*> m_setChildClass;
    std::map<std::string, Visitor_Content*> m_setChildNameSpace;
    typedef std::vector<std::string>        ParamsDefaultVal;
    struct OverLoadData
    {
        bool             is_static;
        std::string      func_type;
        std::string      funcptr_type;
        ParamsDefaultVal default_val;
    };
    std::map<std::string, std::map<std::string, OverLoadData>> m_vecFuncName;
    std::map<std::string, std::map<std::string, OverLoadData>> m_vecConName;
    struct member_prop
    {
        bool bIsStatic;
        bool bIsConst;
    };
    std::map<std::string, member_prop> m_vecValName;
    std::set<std::string>              m_vecInhName;
    std::set<std::string>              m_vecEnumName;
};

CXTranslationUnit                         TU;
typedef std::set<int32_t>                 export_line_set;
std::map<CXFileUniqueID, export_line_set> g_export_loc;

void display_debug_cursor(CXCursor& cursor, CXCursorKind& kind, CXSourceLocation& source_loc)
{

    std::string kindname = getClangString(clang_getCursorKindSpelling(kind));

    CXFile   file;
    unsigned line;
    unsigned column;
    unsigned offset;
    clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

    std::string filename = getClangString(clang_getFileName(file));
    std::string nsname   = getClangString(clang_getCursorSpelling(cursor));
    printf("find:%d %s name:%s in file:%s line:%d \n", kind, kindname.c_str(), nsname.c_str(), filename.c_str(), line);
}

std::string get_default_params(CXCursor cursor, int32_t params_idx)
{
    std::string varname = getClangString(clang_getCursorSpelling(cursor));

    CXSourceRange range = clang_getCursorExtent(cursor);
    unsigned      numtokens;
    CXToken*      tokens;
    clang_tokenize(TU, range, &tokens, &numtokens);

    int32_t defaultTokenStart = 0;
    int32_t defaultTokenEnd   = 0;
    // This tokensequence needs some cleanup. There may be a default value
    // included, and sometimes there are extra tokens.
    std::string default_val;
    bool        bStartInsert = false;
    for(uint32_t i = 0; i < numtokens; ++i)
    {
        auto token_i = tokens[i];
        if(bStartInsert)
        {
            std::string p = getClangString(clang_getTokenSpelling(TU, token_i));
            default_val += p;
            continue;
        }
        if(clang_getTokenKind(token_i) == CXToken_Punctuation)
        {
            std::string p = getClangString(clang_getTokenSpelling(TU, token_i));
            if(p == "=")
            {
                bStartInsert = true;
            }
        }
    }

    clang_disposeTokens(TU, tokens, numtokens);

    return default_val;
}

std::string function_name_conver(const std::string& name)
{
    static std::map<std::string, std::string> map_name = {
        {"operator+", "__add"},
        {"operator-", "__sub"},
        {"operator*", "__mul"},
        {"operator/", "__div"},
        {"operator%", "__mod"},
        {"operator^", "__pow"},
        //{"operator//", "__idiv"},     //浮点触发
        //{"operator-", "__unm"},      //1元运算符
        //{"operator..", "__concat"},  //连接符
        {"operator==", "__eq"},
        {"operator<=", "__le"},
        {"operator<", "__lt"},

        {"operator&", "__band"},
        {"operator|", "__bor"},
        {"operator^", "__bxor"},
        {"operator!", "__bnot"},

        {"operator<<", "__shl"},
        {"operator>>", "__shr"},
    };

    std::string remove_space_name = name;
    remove_space_name.erase(std::remove_if(remove_space_name.begin(), remove_space_name.end(), [](unsigned char x) { return std::isspace(x); }),
                            remove_space_name.end());

    auto itfind = map_name.find(remove_space_name);
    if(itfind != map_name.end())
    {
        return itfind->second;
    }
    else
        return name;
}

void visit_function(CXCursor cursor, Visitor_Content* pContent)
{
    // CXCursor cursor = clang_getCursorDefinition(func_cursor);
    // if (clang_Cursor_isNull(cursor))
    //	cursor = func_cursor;
    std::string nsname  = getClangString(clang_getCursorSpelling(cursor));
    std::string typestr = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
    auto&       refMap  = pContent->m_vecFuncName[nsname];
    if(g_bSkip_overload && refMap.empty() == false)
        return;

    if(refMap.find(typestr) != refMap.end())
        return;
    auto& overload_data     = refMap[typestr];
    overload_data.is_static = clang_CXXMethod_isStatic(cursor) != 0;

    {
        overload_data.funcptr_type = getClangString(clang_getTypeSpelling(clang_getResultType(clang_getCursorType(cursor))));

        if(pContent->m_ContentType == CT_CLASS && overload_data.is_static == false)
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

    // reg gloabl_func
    int32_t nArgs = clang_Cursor_getNumArguments(cursor);
    {
        for(int32_t i = 0; i < nArgs; i++)
        {
            CXCursor args_cursor = clang_Cursor_getArgument(cursor, i);

            overload_data.funcptr_type += getClangString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
            if(i < nArgs - 1)
                overload_data.funcptr_type += ", ";

            if(g_bSkip_default_params == false)
            {
                std::string default_val = get_default_params(args_cursor, i);
                if(default_val.empty() == false)
                    overload_data.default_val.push_back(default_val);
            }
        }
    }
    overload_data.funcptr_type += ")";
    if(clang_CXXMethod_isConst(cursor))
    {
        overload_data.funcptr_type += "const";
    }

    CXCursor* overridden_cursors;
    unsigned  num_overridden = 0;
    clang_getOverriddenCursors(cursor, &overridden_cursors, &num_overridden);
    for(uint32_t i = 0; i < num_overridden; i++)
    {
        CXCursor overridden = overridden_cursors[i];
        visit_function(overridden, pContent);
    }
    if(num_overridden > 0)
    {
        clang_disposeOverriddenCursors(overridden_cursors);
    }
}

void visit_constructor(CXCursor cursor, Visitor_Content* pContent)
{
    std::string nsname  = getClangString(clang_getCursorSpelling(cursor));
    std::string typestr = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
    auto&       refMap  = pContent->m_vecConName[nsname];
    if(g_bSkip_overload && refMap.empty() == false)
        return;
    if(refMap.find(typestr) != refMap.end())
        return;
    auto& overload_data = refMap[typestr];
    // reg gloabl_func
    int32_t nArgs = clang_Cursor_getNumArguments(cursor);
    {
        for(int32_t i = 0; i < nArgs; i++)
        {
            CXCursor args_cursor = clang_Cursor_getArgument(cursor, i);
            overload_data.func_type += ", ";
            overload_data.func_type += getClangString(clang_getTypeSpelling(clang_getCursorType(args_cursor)));
            if(g_bSkip_default_params == false)
            {
                std::string default_val = get_default_params(args_cursor, i);
                if(default_val.empty() == false)
                    overload_data.default_val.push_back(default_val);
            }
        }
    }
}

enum CXChildVisitResult visit_enum(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    Visitor_Content* pContent = (Visitor_Content*)client_data;

    auto kind = clang_getCursorKind(cursor);
    switch(kind)
    {
        case CXCursor_EnumDecl:
        case CXCursor_EnumConstantDecl:
        {
            std::string nsname = getClangString(clang_getCursorSpelling(cursor));
            // clang_getEnumConstantDeclValue(cursor);
            pContent->m_vecEnumName.insert(nsname);
        }
        break;
        default:
            break;
    }

    return CXChildVisit_Continue;
}

CXFileUniqueID           g_lastfile = {0, 0, 0};
std::set<CXFileUniqueID> g_finish_file;
bool                     operator<(const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
    if(lft.data[0] == rht.data[0])
    {
        if(lft.data[1] == rht.data[1])
        {
            return lft.data[2] < rht.data[2];
        }
        else
            return lft.data[1] < rht.data[1];
    }
    else
        return lft.data[0] < rht.data[0];
}

bool operator>(const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
    if(lft.data[0] == rht.data[0])
    {
        if(lft.data[1] == rht.data[1])
        {
            return lft.data[2] > rht.data[2];
        }
        else
            return lft.data[1] > rht.data[1];
    }
    else
        return lft.data[0] > rht.data[0];
}

bool operator==(const CXFileUniqueID& lft, const CXFileUniqueID& rht)
{
    return (lft < rht) == false && (lft > rht) == false;
}
bool operator!=(const CXFileUniqueID& lft, const CXFileUniqueID& rht)
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
    if(itFind != g_finish_file.end())
    {
        debug_printf("skip:%llx %llx %llx \n", id.data[0], id.data[1], id.data[2]);
        return true;
    }

    return false;
}

static void printString(const char* name, CXString string)
{
    const char* cstr = clang_getCString(string);
    if(cstr && *cstr)
    {
        printf("%s: %s ", name, cstr);
    }
    clang_disposeString(string);
}

static void printCursor(CXCursor cursor)
{
    CXFile           file;
    uint32_t         off, line, col;
    CXSourceLocation location = clang_getCursorLocation(cursor);
    if(clang_Location_isInSystemHeader(location))
        return;
    clang_getSpellingLocation(location, &file, &line, &col, &off);
    CXString    fileName     = clang_getFileName(file);
    const char* fileNameCStr = clang_getCString(fileName);
    if(fileNameCStr)
    {
        CXSourceRange range = clang_getCursorExtent(cursor);
        uint32_t      start, end;
        clang_getSpellingLocation(clang_getRangeStart(range), 0, 0, 0, &start);
        clang_getSpellingLocation(clang_getRangeEnd(range), 0, 0, 0, &end);
        printf("%s:%d:%d (%d, %d-%d) ", fileNameCStr, line, col, off, start, end);
    }
    clang_disposeString(fileName);
    printString("kind", clang_getCursorKindSpelling(clang_getCursorKind(cursor)));
    printString("display name", clang_getCursorDisplayName(cursor));
    printString("usr", clang_getCursorUSR(cursor));
    if(clang_isCursorDefinition(cursor))
        printf("definition ");
    printf("\n");
}

static enum CXChildVisitResult visit_display(CXCursor cursor, CXCursor parent, CXClientData userData)
{
    (void)parent;
    int32_t indent = *(int32_t*)userData;
    int32_t i;
    for(i = 0; i < indent; ++i)
    {
        printf("  ");
    }
    printCursor(cursor);
    CXCursor ref = clang_getCursorReferenced(cursor);
    if(!clang_isInvalid(clang_getCursorKind(ref)) && !clang_equalCursors(ref, cursor))
    {
        for(i = 0; i < indent; ++i)
        {
            printf("  ");
        }
        printf("-> ");
        printCursor(ref);
    }
    ++indent;
    return CXChildVisit_Recurse;
    // clang_visitChildren(cursor, visit_display, &indent);
    // return CXChildVisit_Continue;
}

bool                    g_bPreProcessing = true;
enum CXChildVisitResult TU_visitor(CXCursor cursor, CXCursor parent, CXClientData client_data)
{
    Visitor_Content* pContent = (Visitor_Content*)client_data;

    auto kind = clang_getCursorKind(cursor);

    if(g_bPreProcessing && clang_isPreprocessing(kind) == 0)
    {
        // first out of preprcessing check need export
        g_bPreProcessing = false;
        if(g_strKeyword.empty())
            return CXChildVisit_Continue;
        if(g_export_loc.empty())
            return CXChildVisit_Break;
    }

    switch(kind)
    {
        case CXCursor_MacroExpansion:
        {
            if(g_strKeyword.empty())
                return CXChildVisit_Continue;
            std::string nsname = getClangString(clang_getCursorSpelling(cursor));
            if(nsname == g_strKeyword)
            {
                auto source_loc = clang_getCursorLocation(cursor);

                debug_display_debug_cursor(cursor, kind, source_loc);

                CXFile   file;
                unsigned line;
                unsigned column;
                unsigned offset;
                clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

                CXFileUniqueID id;
                clang_getFileUniqueID(file, &id);

                if(NeedSkipByFile(id) == true)
                    return CXChildVisit_Continue;

                auto& refSet = g_export_loc[id];
                refSet.insert(line);
            }
        }
        break;
        case CXCursor_Namespace:
        {
            if(g_bSkip_namespace)
                return CXChildVisit_Continue;
            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    return CXChildVisit_Continue;
                }
            }

            std::string nsname = getClangString(clang_getCursorSpelling(cursor));

            if(g_strExportNamespaceName.empty() == false && g_strExportNamespaceName.find(nsname) == g_strExportNamespaceName.end())
                return CXChildVisit_Continue;
            // reg class
            if(pContent->hasChildNameSpace(nsname) == false)
            {
                Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent, pContent->getAccessPrifix() + nsname, CT_NAMESPACE);

                clang_visitChildren(cursor, &TU_visitor, pNewContent);
            }
            else
            {
                clang_visitChildren(cursor, &TU_visitor, pContent->m_setChildNameSpace[nsname]);
            }
        }
        break;
        case CXCursor_UnionDecl:
            break;
        case CXCursor_StructDecl:
        case CXCursor_ClassDecl:
        {
            if(g_bSkip_class)
                return CXChildVisit_Continue;
            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:ClassDecl no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));

            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;
            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    return CXChildVisit_Continue;
                }
            }

            std::string nsname = getClangString(clang_getCursorSpelling(cursor));
            std::string tname  = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));

            // reg class
            if(pContent->hasChildClass(nsname) == false)
            {
                debug_printf("do:class\n");

                Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent, tname, CT_CLASS);
                g_export_class_name.insert(nsname);
                clang_visitChildren(cursor, &TU_visitor, pNewContent);
            }
            else
            {
                debug_printf("skip:class\n");
            }
        }
        break;
        case CXCursor_CXXMethod:
        {
            if(g_bSkip_method)
                return CXChildVisit_Continue;
            if(g_bSkip_method_static && clang_CXXMethod_isStatic(cursor))
                return CXChildVisit_Continue;

            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:CXXMethod no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));

            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    debug_printf("skip:CXXMethod\n");

                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:CXXMethod\n");

                    return CXChildVisit_Continue;
                }
            }
            debug_printf("do:CXXMethod\n");

            visit_function(cursor, pContent);
        }
        break;
        case CXCursor_Constructor:
        {
            if(g_bSkip_con)
                return CXChildVisit_Continue;
            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:Constructor no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:Constructor\n");

                    return CXChildVisit_Continue;
                }
            }

            debug_printf("do:Constructor\n");

            visit_constructor(cursor, pContent);
        }
        break;
        case CXCursor_Destructor:
        {
        }
        break;
        case CXCursor_FieldDecl:
        {
            if(g_bSkip_field)
                return CXChildVisit_Continue;
            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:FieldDecl no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:FieldDecl\n");

                    return CXChildVisit_Continue;
                }
            }
            std::string nsname                       = getClangString(clang_getCursorSpelling(cursor));
            pContent->m_vecValName[nsname].bIsStatic = false;
            pContent->m_vecValName[nsname].bIsConst  = clang_isConstQualifiedType(clang_getCursorType(cursor)) != 0;

            debug_printf("do:FieldDecl\n");
        }
        break;
        case CXCursor_CXXBaseSpecifier:
        {
            if(g_bSkip_class)
                return CXChildVisit_Continue;

            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:CXXBaseSpecifier no public\n");

                return CXChildVisit_Continue;
            }

            debug_display_debug_cursor(cursor, kind, source_loc);

            // reg inh
            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);

            // std::string filename = getClangString(clang_getFileName(file));
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:CXXBase\n");

                    return CXChildVisit_Continue;
                }
            }
            // std::string nsname = getClangString(clang_getCursorSpelling(cursor));
            std::string tname = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
            pContent->m_vecInhName.insert(tname);
            debug_printf("do:CXXBase\n");
        }
        break;
        case CXCursor_EnumDecl:
        case CXCursor_EnumConstantDecl:
        {
            if(g_bSkip_enum)
                return CXChildVisit_Continue;

            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:EnumDecl no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:Enum\n");

                    return CXChildVisit_Continue;
                }
            }
            else
            {
            }

            debug_printf("do:Enum\n");

            CXSourceRange range = clang_getCursorExtent(cursor);
            unsigned      numtokens;
            CXToken*      tokens;
            clang_tokenize(TU, range, &tokens, &numtokens);

            bool bScopedEnum = false;
            // This tokensequence needs some cleanup.
            for(uint32_t i = 0; i < numtokens; ++i)
            {
                auto token_i = tokens[i];
                if(clang_getTokenKind(token_i) == CXToken_Keyword)
                {
                    std::string p = getClangString(clang_getTokenSpelling(TU, token_i));
                    if(p == "class")
                    {
                        bScopedEnum = true;
                        break;
                    }
                }
            }
            clang_disposeTokens(TU, tokens, numtokens);

            // reg global val;
            if(bScopedEnum)
            {
                std::string nsname = getClangString(clang_getCursorSpelling(cursor));

                Visitor_Content* pNewContent = new Visitor_Content(nsname, pContent, pContent->getAccessPrifix() + nsname, CT_NAMESPACE);

                clang_visitChildren(cursor, &visit_enum, pNewContent);
            }
            else
            {
                clang_visitChildren(cursor, &visit_enum, pContent);
            }
        }
        break;
        case CXCursor_VarDecl:
        {
            if(g_bSkip_var)
                return CXChildVisit_Continue;

            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CX_CXXAccessSpecifier access_kind = clang_getCXXAccessSpecifier(cursor);
            if(access_kind != CX_CXXPublic && access_kind != CX_CXXInvalidAccessSpecifier)
            {
                debug_printf("skip:VarDecl no public\n");

                return CXChildVisit_Continue;
            }

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:VarDecl\n");

                    return CXChildVisit_Continue;
                }
            }
            debug_printf("do:VarDecl\n");

            // reg global val;
            std::string nsname = getClangString(clang_getCursorSpelling(cursor));
            // std::string tname = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));

            pContent->m_vecValName[nsname].bIsStatic = true;
            pContent->m_vecValName[nsname].bIsConst  = clang_isConstQualifiedType(clang_getCursorType(cursor)) != 0;
        }
        break;
        case CXCursor_FunctionDecl:
        {
            if(g_bSkip_function)
                return CXChildVisit_Continue;

            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;

            debug_display_debug_cursor(cursor, kind, source_loc);

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;

            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    debug_printf("skip:funciton\n");

                    return CXChildVisit_Continue;
                }
            }

            debug_printf("do:funciton\n");

            visit_function(cursor, pContent);
        }
        break;
        case CXCursor_TypedefDecl:
        case CXCursor_TypeAliasDecl:
        {
            if(g_bSkip_class)
                return CXChildVisit_Continue;
            auto source_loc = clang_getCursorLocation(cursor);
            if(clang_Location_isInSystemHeader(source_loc))
                return CXChildVisit_Continue;
            debug_display_debug_cursor(cursor, kind, source_loc);

            CXFile   file;
            unsigned line;
            unsigned column;
            unsigned offset;
            clang_getExpansionLocation(source_loc, &file, &line, &column, &offset);
            // std::string filename = getClangString(clang_getFileName(file));

            CXFileUniqueID id;
            clang_getFileUniqueID(file, &id);
            if(NeedSkipByFile(id) == true)
                return CXChildVisit_Continue;
            if(g_strKeyword.empty() == false)
            {
                auto itFind = g_export_loc.find(id);
                if(itFind == g_export_loc.end())
                {
                    return CXChildVisit_Continue;
                }

                auto& refSet = itFind->second;
                if(refSet.find(line) == refSet.end())
                {
                    return CXChildVisit_Continue;
                }
            }

            std::string nsname     = getClangString(clang_getCursorSpelling(cursor));
            std::string tname      = getClangString(clang_getTypeSpelling(clang_getCursorType(cursor)));
            std::string canon_name = getClangString(clang_getTypeSpelling(clang_getCanonicalType(clang_getCursorType(cursor))));

            debug_printf("do:class_alias\n");
            if(pContent)
            {
                auto pRealType = pContent->m_setChildClass[canon_name];
                if(pRealType)
                {
                    pRealType->m_aliasname_list.push_back(nsname);
                }
            }
        }
        break;
        default:
        {
        }
        break;
    }
    return CXChildVisit_Continue;
}

void visit_contnet(Visitor_Content* pContent, std::string& os, std::string& os_second)
{
    char szBuf[4096];
    if(pContent->m_ContentType == CT_CLASS)
    {
        // should export this?
        if(g_strExportClassName.empty() == false && g_strExportClassName.find(pContent->m_name) == g_strExportClassName.end())
            return;
    }
    else if(pContent->m_ContentType == CT_NAMESPACE)
    {
        // should export this
        if(g_strExportNamespaceName.empty() == false && g_strExportNamespaceName.find(pContent->m_name) == g_strExportNamespaceName.end())
            return;
    }
    else if(pContent->m_pParent == NULL)
    {
        // root
        if(g_strExportClassName.empty() == false || g_strExportNamespaceName.empty() == false)
        {
            for(auto& v: pContent->m_setChildClass)
            {
                if(g_strExportNamespaceName.empty() == false)
                {
                    // only visit namespace
                    if(v.second->m_ContentType == CT_NAMESPACE)
                        visit_contnet(v.second, os, os_second);
                }
                else
                {
                    visit_contnet(v.second, os, os_second);
                }
            }
            for(auto& v: pContent->m_setChildNameSpace)
            {
                if(g_strExportNamespaceName.empty() == false)
                {
                    // only visit namespace
                    if(v.second->m_ContentType == CT_NAMESPACE)
                        visit_contnet(v.second, os, os_second);
                }
                else
                {
                    visit_contnet(v.second, os, os_second);
                }
            }
            return; // just visit child,skip global scope
        }
        else
        {
            // vist child first;
            for(auto& v: pContent->m_setChildClass)
            {
                visit_contnet(v.second, os, os_second);
                if(pContent->m_ContentType != CT_GLOBAL)
                {
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::scope_inner(L, \"%s\", \"%s\", \"%s\");\n",
                             pContent->getAccessName().c_str(),
                             v.second->m_name.c_str(),
                             v.second->getAccessName().c_str());
                    os += szBuf;
                }
            }
            for(auto& v: pContent->m_setChildNameSpace)
            {
                visit_contnet(v.second, os, os_second);
                if(pContent->m_ContentType != CT_GLOBAL)
                {
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::scope_inner(L, \"%s\", \"%s\", \"%s\");\n",
                             pContent->getAccessName().c_str(),
                             v.second->m_name.c_str(),
                             v.second->getAccessName().c_str());
                    os += szBuf;
                }
            }
        }
    }

    // class add
    if(pContent->m_ContentType == CT_CLASS)
    {
        snprintf(szBuf,
                 4096,
                 "lua_tinker::class_add<%s>(L, \"%s\",false);\n",
                 pContent->getAccessName().c_str(),
                 pContent->getAccessName().c_str()); // class_add
        os += szBuf;
    }
    else if(pContent->m_ContentType == CT_NAMESPACE)
    {
        snprintf(szBuf, 4096, "lua_tinker::namespace_add(L, \"%s\");\n",
                 pContent->getAccessName().c_str()); // namespace_add
        os += szBuf;
    }

    for(const auto& v: pContent->m_vecInhName)
    {
        if(g_export_class_name.find(v) == g_export_class_name.end())
            continue;

        snprintf(szBuf, 4096, "lua_tinker::class_inh<%s,%s>(L);\n", pContent->getAccessName().c_str(), v.c_str());
        os_second += szBuf;
    }

    for(const auto& v: pContent->m_aliasname_list)
    {
        snprintf(szBuf, 4096, "lua_tinker::class_alias<%s,%s>(L, \"%s\");\n", pContent->getAccessName().c_str(), v.c_str(), v.c_str());
        os_second += szBuf;
    }

    // global_func

    if(pContent->m_ContentType == CT_CLASS)
    {
        // class CXXMethod
        for(const auto& v: pContent->m_vecFuncName)
        {

            const auto& refDataSet = v.second;
            if(refDataSet.size() == 1) // no overload
            {
                const auto& refData = refDataSet.begin()->second;
                std::string def_params;
                for(const auto& dv: refData.default_val)
                {
                    def_params += ", ";
                    def_params += dv;
                }

                if(refData.is_static)
                {
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_def_static<%s>(L, \"%s\",&%s%s);\n",
                             pContent->getAccessName().c_str(),
                             v.first.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str(),
                             def_params.c_str());
                }
                else
                {
                    std::string new_func_name = function_name_conver(v.first);
                    if(new_func_name == v.first)
                    {
                        snprintf(szBuf,
                                 4096,
                                 "lua_tinker::class_def<%s>(L, \"%s\",&%s%s);\n",
                                 pContent->getAccessName().c_str(),
                                 new_func_name.c_str(),
                                 (pContent->getAccessPrifix() + v.first).c_str(),
                                 def_params.c_str());
                    }
                    else
                    {
                        snprintf(szBuf,
                                 4096,
                                 "lua_tinker::class_def<%s>(L, \"%s\",(%s)&%s%s);\n",
                                 pContent->getAccessName().c_str(),
                                 new_func_name.c_str(),
                                 refData.funcptr_type.c_str(),
                                 (pContent->getAccessPrifix() + v.first).c_str(),
                                 def_params.c_str());
                    }
                }
                os += szBuf;
            }
            else
            {
                // overload
                std::string overload_params;
                size_t      nDefaultParamsStart = 1;
                std::string def_params;
                bool        bStatic = false;
                for(const auto& it_refData: refDataSet)
                {
                    const auto& refData = it_refData.second;
                    bStatic |= refData.is_static;
                    std::string def_params_decl;
                    for(const auto& dv: refData.default_val)
                    {
                        def_params_decl += ", ";
                        def_params_decl += dv;
                    }

                    if(overload_params.empty() == false)
                        overload_params += ", ";
                    if(bStatic)
                    {
                        snprintf(szBuf,
                                 4096,
                                 "\n\tlua_tinker::make_functor_ptr((%s)(&%s)%s)",
                                 refData.funcptr_type.c_str(),
                                 (pContent->getAccessPrifix() + v.first).c_str(),
                                 def_params_decl.c_str());
                    }
                    else
                    {
                        snprintf(szBuf,
                                 4096,
                                 "\n\tlua_tinker::make_member_functor_ptr((%s)(&%s)%s)",
                                 refData.funcptr_type.c_str(),
                                 (pContent->getAccessPrifix() + v.first).c_str(),
                                 def_params_decl.c_str());
                    }
                    overload_params += szBuf;
                }

                if(bStatic)
                {
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_def_static<%s>(L, \"%s\", lua_tinker::args_type_overload_functor(%s)%s);\n",
                             pContent->getAccessName().c_str(),
                             function_name_conver(v.first).c_str(),
                             overload_params.c_str(),
                             def_params.c_str());
                }
                else
                {
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_def<%s>(L, \"%s\", lua_tinker::args_type_overload_member_functor(%s)%s);\n",
                             pContent->getAccessName().c_str(),
                             function_name_conver(v.first).c_str(),
                             overload_params.c_str(),
                             def_params.c_str());
                }

                os += szBuf;
            }
        }
    }
    else if(pContent->m_ContentType == CT_NAMESPACE)
    {
        for(const auto& v: pContent->m_vecFuncName)
        {
            const auto& refDataSet = v.second;
            if(refDataSet.size() == 1) // no overload
            {
                const auto& refData = refDataSet.begin()->second;
                std::string def_params;
                for(const auto& dv: refData.default_val)
                {
                    def_params += ", ";
                    def_params += dv;
                }

                snprintf(szBuf,
                         4096,
                         "lua_tinker::namespace_def(L, \"%s\", \"%s\",&%s%s);\n",
                         pContent->getAccessName().c_str(),
                         v.first.c_str(),
                         (pContent->getAccessPrifix() + v.first).c_str(),
                         def_params.c_str());
                os += szBuf;
            }
            else
            {
                // overload
                std::string overload_params;
                std::string def_params;
                size_t      nDefaultParamsStart = 1;
                for(const auto& it_refData: refDataSet)
                {
                    const auto& refData = it_refData.second;
                    std::string def_params_decl;
                    for(const auto& dv: refData.default_val)
                    {
                        def_params_decl += ", ";
                        def_params_decl += dv;
                    }

                    if(overload_params.empty() == false)
                        overload_params += ", ";
                    snprintf(szBuf,
                             4096,
                             "\n\tlua_tinker::make_functor_ptr((%s)(&%s)%s)",
                             refData.funcptr_type.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str(),
                             def_params_decl.c_str());
                    overload_params += szBuf;
                }

                snprintf(szBuf,
                         4096,
                         "lua_tinker::namespace_def(L, \"%s\", \"%s\", lua_tinker::args_type_overload_functor(%s)%s);\n",
                         pContent->getAccessName().c_str(),
                         v.first.c_str(),
                         overload_params.c_str(),
                         def_params.c_str());
                os += szBuf;
            }
        }
    }
    else if(pContent->m_pParent == NULL)
    {
        for(const auto& v: pContent->m_vecFuncName)
        {
            const auto& refDataSet = v.second;
            if(refDataSet.size() == 1) // no overload
            {
                const auto& refData = refDataSet.begin()->second;
                std::string def_params;
                for(const auto& dv: refData.default_val)
                {
                    def_params += ", ";
                    def_params += dv;
                }

                snprintf(szBuf,
                         4096,
                         "lua_tinker::def(L, \"%s\",&%s%s);\n",
                         v.first.c_str(),
                         (pContent->getAccessPrifix() + v.first).c_str(),
                         def_params.c_str());
                os += szBuf;
            }
            else
            {
                // overload
                std::string overload_params;
                std::string def_params;
                size_t      nDefaultParamsStart = 1;
                for(const auto& it_refData: refDataSet)
                {
                    const auto& refData = it_refData.second;
                    std::string def_params_decl;
                    for(const auto& dv: refData.default_val)
                    {
                        def_params_decl += ", ";
                        def_params_decl += dv;
                    }

                    if(overload_params.empty() == false)
                        overload_params += ", ";
                    snprintf(szBuf,
                             4096,
                             "\n\tlua_tinker::make_functor_ptr((%s)(&%s)%s)",
                             refData.funcptr_type.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str(),
                             def_params_decl.c_str());
                    overload_params += szBuf;
                }

                snprintf(szBuf,
                         4096,
                         "lua_tinker::def(L, \"%s\", lua_tinker::args_type_overload_functor(%s)%s);\n",
                         v.first.c_str(),
                         overload_params.c_str(),
                         def_params.c_str());
                os += szBuf;
            }
        }
    }

    for(const auto& v: pContent->m_vecConName)
    {
        const auto& refDataSet = v.second;
        if(refDataSet.size() == 1) // no overload
        {
            const auto& refData = refDataSet.begin()->second;

            std::string def_params;
            for(const auto& dv: refData.default_val)
            {
                def_params += ", ";
                def_params += dv;
            }
            snprintf(szBuf,
                     4096,
                     "lua_tinker::class_con<%s%s>(L%s);\n",
                     pContent->getAccessName().c_str(),
                     refData.func_type.c_str(),
                     def_params.c_str()); // normal
            os += szBuf;
        }
        else
        {
            // overload
            std::string overload_params;
            size_t      nDefaultParamsStart = 1;
            std::string def_params;
            for(const auto& it_refData: refDataSet)
            {
                const auto& refData = it_refData.second;
                std::string def_params_decl;
                for(const auto& dv: refData.default_val)
                {
                    if(def_params_decl.empty() == false)
                        def_params_decl += ", ";
                    def_params_decl += dv;
                }
                snprintf(szBuf,
                         4096,
                         "\nlua_tinker::make_constructor_ptr<%s%s>(%s)",
                         pContent->getAccessName().c_str(),
                         refData.func_type.c_str(),
                         def_params_decl.c_str()); // normal
                if(overload_params.empty() == false)
                    overload_params += ", ";
                overload_params += szBuf;
            }

            snprintf(szBuf,
                     4096,
                     "lua_tinker::class_con_fun<%s>(L,lua_tinker::args_type_overload_constructor(%s)%s);\n",
                     pContent->getAccessName().c_str(),
                     overload_params.c_str(),
                     def_params.c_str());
            os += szBuf;
        }
    }

    if(pContent->m_ContentType == CT_NAMESPACE)
    {
        for(const auto& v: pContent->m_vecValName)
        {
            snprintf(szBuf,
                     4096,
                     "lua_tinker::namespace_set(L,\"%s\",\"%s\",%s);\n",
                     pContent->getAccessName().c_str(),
                     v.first.c_str(),
                     (pContent->getAccessPrifix() + v.first).c_str());
            os += szBuf;
        }
    }
    else if(pContent->m_ContentType == CT_CLASS)
    {
        for(const auto& v: pContent->m_vecValName)
        {
            if(v.second.bIsStatic == true)
            {
                if(v.second.bIsConst == true)
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_mem_static_readonly<%s>(L,\"%s\",&%s);\n",
                             pContent->getAccessName().c_str(),
                             v.first.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str());
                else
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_mem_static<%s>(L,\"%s\",&%s);\n",
                             pContent->getAccessName().c_str(),
                             v.first.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str());
            }
            else
            {
                if(v.second.bIsConst == true)
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_mem_readonly<%s>(L,\"%s\",&%s);\n",
                             pContent->getAccessName().c_str(),
                             v.first.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str());
                else
                    snprintf(szBuf,
                             4096,
                             "lua_tinker::class_mem<%s>(L,\"%s\",&%s);\n",
                             pContent->getAccessName().c_str(),
                             v.first.c_str(),
                             (pContent->getAccessPrifix() + v.first).c_str());
            }
            os += szBuf;
        }
    }
    else if(pContent->m_pParent == NULL)
    {

        for(const auto& v: pContent->m_vecValName)
        {
            snprintf(szBuf, 4096, "lua_tinker::set(L,\"%s\",%s);\n", v.first.c_str(), (pContent->getAccessPrifix() + v.first).c_str());
            os += szBuf;
        }
    }

    if(pContent->m_ContentType == CT_NAMESPACE)
    {
        for(const auto& v: pContent->m_vecEnumName)
        {
            snprintf(szBuf,
                     4096,
                     "lua_tinker::namespace_set(L, \"%s\",\"%s\",%s);\n",
                     pContent->getAccessName().c_str(),
                     v.c_str(),
                     (pContent->getAccessPrifix() + v).c_str());
            os += szBuf;
        }
    }
    else if(pContent->m_ContentType == CT_CLASS)
    {
        for(const auto& v: pContent->m_vecEnumName)
        {
            snprintf(szBuf,
                     4096,
                     "lua_tinker::class_var_static<%s>(L, \"%s\",%s);\n",
                     pContent->getAccessName().c_str(),
                     v.c_str(),
                     (pContent->getAccessPrifix() + v).c_str());
            os += szBuf;
        }
    }
    else if(pContent->m_pParent == NULL)
    {
        for(const auto& v: pContent->m_vecEnumName)
        {
            snprintf(szBuf, 4096, "lua_tinker::set(L, \"%s\",%s);\n", v.c_str(), (pContent->getAccessPrifix() + v).c_str());
            os += szBuf;
        }
    }

    // root vist child first, other vist child end;
    if(pContent->m_pParent != NULL)
    {
        for(auto& v: pContent->m_setChildClass)
        {
            visit_contnet(v.second, os, os_second);
            if(pContent->m_ContentType != CT_GLOBAL)
            {
                snprintf(szBuf,
                         4096,
                         "lua_tinker::scope_inner(L, \"%s\", \"%s\", \"%s\");\n",
                         pContent->getAccessName().c_str(),
                         v.second->m_name.c_str(),
                         v.second->getAccessName().c_str());
                os += szBuf;
            }
        }
        for(auto& v: pContent->m_setChildNameSpace)
        {
            visit_contnet(v.second, os, os_second);
            if(pContent->m_ContentType != CT_GLOBAL)
            {
                snprintf(szBuf,
                         4096,
                         "lua_tinker::scope_inner(L, \"%s\", \"%s\", \"%s\");\n",
                         pContent->getAccessName().c_str(),
                         v.second->m_name.c_str(),
                         v.second->getAccessName().c_str());
                os += szBuf;
            }
        }
    }
}

void Tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters)
{
    // Skip delimiters at beginning.
    std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
    // Find first "non-delimiter".
    std::string::size_type pos = str.find_first_of(delimiters, lastPos);
    while(std::string::npos != pos || std::string::npos != lastPos)
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

template<typename T, int32_t N>
inline size_t sizeofArray(T (&_array)[N])
{
    return N;
}

void print_help()
{
    printf(
        R"(export2lua[cppfile][-Iheaderdir] ...[-Iheaderdir]
[--cpps=filename]			use ' ' to separate source files from file like a.cpp b.cpp test/c.cpp 
[--include=filename]		use ' ' to separate include dirs from file like -Itest -I../game;
[--output=filename]			output file name
[--exportclass=filename]	use ' ' to separate which class need export from file
[--keyword=export_lua]		keyword default is export_lua, if set to empty will output all decl
[--output_functionname=expot2lua] function name warp
[--skip_default_params]		not output default_params
[--skip_function]		not output function
[--skip_class]			not output class
[--skip_namespace]		not output namespace
[--skip_var]			not output var
[--skip_field]			not output class field
[--skip_enum]			not output enum
[--skip_method]			not output method
[--skip_method_static]		not static method
[--skip_con]			not output Constructor
[--skip_overload]		not output overload function
[-v]				will output DEBUG info)");
}
int main(int argc, char** argv)
{
    if(argc < 2)
    {
        print_help();
        return 0;
    }
    get_opt                  opt(argc, (const char**)argv);
    std::vector<std::string> szHeaderDirs;
    std::vector<const char*> szParams;
    std::vector<std::string> szCppFiles;
    std::string              output_functionname = "export2lua";

    if(opt.has("-I"))
        szHeaderDirs.push_back(opt["-I"]);

    if(opt.has("-v"))
        g_bDebug = true;

    if(opt.has("--output_functionname"))
        output_functionname = opt["--output_functionname"];

    bool bSkipCpp = false;
    if(opt.has("--skip_cpp"))
    {
        bSkipCpp = true;
    }

    if(opt.has("--cpps"))
    {
        std::ifstream infile(opt["--cpps"]);
        std::string   cpp;
        while(infile >> cpp)
        {
            if(bSkipCpp)
            {
                if(cpp.substr(cpp.size() - 4, 4) == ".cpp" || cpp.substr(cpp.size() - 3, 3) == ".cc")
                    continue;
            }

            szCppFiles.push_back(cpp);
        }
    }

    if(opt.has("--include"))
    {
        std::ifstream infile(opt["--include"]);
        std::string   include;
        while(infile >> include)
        {
            szHeaderDirs.push_back("-I" + include);
        }
    }

    if(opt.has("--output"))
    {
        output_filename = opt["--output"];
        temp_filename   = output_filename + "._tmp";
    }
    else
    {
        print_help();
        return 0;
    }

    if(opt.has("--keyword"))
    {
        g_strKeyword = opt["--keyword"];
    }

    if(opt.has("--justdisplay"))
    {
        g_bJustDisplay = true;
    }
    if(opt.has("--skip_default_params"))
    {
        g_bSkip_default_params = true;
    }
    if(opt.has("--skip_function"))
    {
        g_bSkip_function = true;
    }
    if(opt.has("--skip_class"))
    {
        g_bSkip_class = true;
    }
    if(opt.has("--skip_var"))
    {
        g_bSkip_var = true;
    }
    if(opt.has("--skip_field"))
    {
        g_bSkip_field = true;
    }
    if(opt.has("--skip_namespace"))
    {
        g_bSkip_namespace = true;
    }

    if(opt.has("--skip_enum"))
    {
        g_bSkip_enum = true;
    }
    if(opt.has("--skip_method"))
    {
        g_bSkip_method = true;
    }
    if(opt.has("--skip_method_static"))
    {
        g_bSkip_method_static = true;
    }
    if(opt.has("--skip_con"))
    {
        g_bSkip_con = true;
    }
    if(opt.has("--skip_overload"))
    {
        g_bSkip_overload = true;
    }
    if(opt.has("--ext_tuflag"))
    {
        g_extTUFlag = atoi(opt["--ext_tuflag"].c_str());
    }

    if(opt.has("--old_style"))
    {
        g_bSkip_default_params = true;
        g_bSkip_overload       = true;
        g_bSkip_var            = true;
        g_bSkip_field          = true;
        g_bSkip_method_static  = true;
    }

    const char* ext_cxx_flag[] = {
        "-xc++",
        "-std=c++17",
        "-w",
        "-fno-spell-checking",
        "-fsyntax-only",
        //"-Dexport_lua="
    };
    szParams.reserve(szHeaderDirs.size() + sizeofArray(ext_cxx_flag));
    for(auto& v: szHeaderDirs)
    {
        szParams.push_back(v.c_str());
    }
    for(size_t i = 0; i < sizeofArray(ext_cxx_flag); i++)
    {
        szParams.push_back(ext_cxx_flag[i]);
    }

    Visitor_Content content;
    CXIndex         Index = clang_createIndex(0, 0);
    clang_CXIndex_setGlobalOptions(Index, clang_CXIndex_getGlobalOptions(Index));
    std::fstream temp_file(temp_filename.c_str(), std::ios_base::in | std::ios_base::out | std::ios_base::trunc);
    for(const auto& cpp: szCppFiles)
    {
        temp_file << "#include \"" << get_filename_from_fullpath(cpp) << "\"\n";
        temp_file.flush();
    }

    {
        std::chrono::high_resolution_clock clock;

        auto     _startTime = clock.now();
        unsigned flag       = CXTranslationUnit_SkipFunctionBodies | CXTranslationUnit_KeepGoing | CXTranslationUnit_Incomplete |
                        CXTranslationUnit_IgnoreNonErrorsFromIncludedFiles | g_extTUFlag;
        if(g_strKeyword.empty() == false)
        {
            flag |= CXTranslationUnit_DetailedPreprocessingRecord;
        }
        TU = clang_parseTranslationUnit(Index, temp_filename.c_str(), szParams.data(), szParams.size(), 0, 0, flag);

        auto _stopTime1 = clock.now();

        CXCursor C = clang_getTranslationUnitCursor(TU);
        if(g_bJustDisplay)
        {
            int32_t indent = 0;
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
        if(temp_filename.empty() == false)
        {
            std::chrono::duration<double> parse_ms = _stopTime1 - _startTime;
            std::chrono::duration<double> visit_ms = _stopTime2 - _stopTime1;
            printf("file:%s parse hpp: %f vist hpp: %f\n", temp_filename.c_str(), parse_ms.count(), visit_ms.count());
        }
    }

    clang_disposeIndex(Index);
    if(g_bDebug)
    {
        printf("press any key to start process content");
        getchar();
    }

    std::string os;
    std::string os_second;

    visit_contnet(&content, os, os_second);
    if(g_bDebug)
    {
        printf("global:func:%lu, child:%lu\n", content.m_vecFuncName.size(), content.m_setChildClass.size() + content.m_setChildNameSpace.size());
        printf("press any key to start output\n");
        getchar();
    }

    if(output_filename.empty())
    {
        if(os.empty() == false)
            printf("%s", os.c_str());
        if(os_second.empty() == false)
            printf("%s", os_second.c_str());
    }
    else
    {
        printf("start gen export_code,plz waiting\n");

        temp_file << "#include \"lua_tinker.h\"\n";
        temp_file << "void " << output_functionname << "(lua_State* L)\n{\n";

        if(os.empty() == false)
            temp_file << os;
        if(os_second.empty() == false)
            temp_file << os_second;
        temp_file << "\n}";
        temp_file.flush();
        temp_file.seekg(0);
        temp_file.close();

        if(opt.has("--format"))
        {
            system(attempt_format("{} -i {}", opt["--format"], temp_filename).c_str());
        }

        std::fstream output_file_read(output_filename.c_str(), std::ios_base::in | std::ios_base::out);
        std::fstream temp_file_read(temp_filename.c_str(), std::ios_base::in | std::ios_base::out);
        std::string  old_txt((std::istreambuf_iterator<char>(output_file_read)), std::istreambuf_iterator<char>());
        std::string  new_txt((std::istreambuf_iterator<char>(temp_file_read)), std::istreambuf_iterator<char>());
        output_file_read.close();
        if(old_txt != new_txt)
        {
            std::fstream output_file;
            output_file.open(output_filename.c_str(), std::ios_base::out | std::ios_base::trunc);
            if(output_file.is_open())
            {
                output_file << new_txt;
                output_file.close();
                printf("write %s succ.\n", output_filename.c_str());
            }
        }

        temp_file_read.close();
        remove(temp_filename.c_str());
    }
    if(g_bDebug)
    {
        printf("press any key to exit\n");
        getchar();
    }
    return 0;
}