# export2lua

* 使用libclang读取cxx头文件，自动生成符合luatinkerE语法的导出语句  
* 支持luatinkerE的所有新特性，包括函数重载及默认参数导出
* 需要定义一个空的export_lua,用来指示哪一行的定义需要导出到lua

***

* use libclang read cxx header file ，auto gen c++ code export to lua for luatinkerE  
* support all feature of luatinkerE， include overload function and default params output
* need define a empty macro named export_lua,will gen export code which line "export_lua" used 

***

usage： export2lua [cppfile] [-Iheaderdir] ... [-Iheaderdir]
[--cpps=cpps_list_file]  cpps_list_file use ';' to separate cpp_file from file like a.cpp;b.cpp;test/c.cpp;  
[--include=header_dirs_file] header_dirs_file use ';' to separate header_dirs from file like -Itest;-I../game;  
[--output=output_file] 
[--exportclass=export_class_name_file]
[--keyword=export_lua]
[--skip_default_params] did not output default_params  
[--skip_function]  did not output function  
[--skip_function]  did not output function  
[--skip_function]  did not output function  [--skip_function]  did not output function  [--skip_function]  did not output function  [--skip_function]  did not output function  [--skip_function]  did not output function  
[-v]   will output DEBUG info  
***




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
