# export2lua

* 使用libclang读取cxx头文件，自动生成符合luatinkerE语法的导出语句  
* 支持luatinkerE的所有新特性，包括函数重载及默认参数导出
* 需要定义一个空的export_lua,用来指示哪一行的定义需要导出到lua

***

* use libclang read cxx header file ，auto gen c++ code export to lua for luatinkerE  
* support all feature of luatinkerE， include overload function and default params output
* need define a empty macro named export_lua,will gen export code which line "export_lua" used 

***

usage： 
export2lua[cppfile][-Iheaderdir] ...[-Iheaderdir]
* [--cpps=filename]		use ';' to separate source files from file like a.cpp;b.cpp;test/c.cpp;
* [--include=filename]		use ';' to separate include dirs from file like -Itest;-I../game;
* [--output=filename]		output file name
* [--exportclass=filename]	use ';' to separate which class need export from file
* [--keyword=export_lua]		keyword default is export_lua, if set to empty will output all decl
* [--skip_default_params]		not output default_params
* [--skip_function]		not output function
* [--skip_class]			not output class
* [--skip_namespace]		not output namespace
* [--skip_var]			not output var
* [--skip_field]			not output class field
* [--skip_enum]			not output enum
* [--skip_method]			not output method
* [--skip_method_static]		not static method
* [--skip_con]			not output Constructor
* [--skip_overload]		not output overload function
* [-v]				will output DEBUG info)   
* 

***

example:
```
读取tests/test.h文件，增加include作为头文件目录，结果重定向到a.txt
export2lua tests/test.h -Iinclude > a.txt
```
```
读取tests/test.h文件，增加include作为头文件目录，结果输出到a.txt
export2lua tests/test.h -Iinclude --output=a.txt
```
```
读取cpps.txt中列出的所有文件，增加header.txt中列出的所有目录作为头文件目录，结果输出到a.txt
export2lua --cpps=cpps.txt --include=header.txt --output=a.txt
```
```
读取cpps.txt中列出的所有文件，增加header.txt中列出的所有目录作为头文件目录，取消关键字查找改为全部导出, 导出类名必须包含在export.txt中， 结果输出到a.txt
export2lua --cpps=cpps.txt --include=header.txt --output=a.txt --keyword= --exportclass=export.txt
```





***
in cxx：  
```
#define export_lua  
export_lua class test    
{   
export_lua test(){}  
export_lua void member_func(int);  
};  
export_lua int global_func();  
```
autogen:
```
//this file was auto generated, plz don't modify it
#include "lua_tinker.h"
#include "tests/class2.h"
void export_to_lua_auto(lua_State* L)
{ 
lua_tinker::def(L, "global_func",&global_func);
lua_tinker::class_add<test>(L, "test",true);
lua_tinker::class_def<test>(L, "member_func",&test::member_func); 
lua_tinker::class_con<test>(L, lua_tinker::constructor<test>::invoke);
}
```
