# export2lua

使用libclang读取cxx头文件，自动生成符合luatinkerE语法的导出语句  
需要定义一个空的export_lua,用来指示哪一行的定义需要导出到lua

use libclang read cxx header file ，auto gen c++ code export to lua for luatinkerE  
need define a empty macro named export_lua,will gen export code which line "export_lua" used 

in cxx：  
`#define export_lua`  
`export_lua class test
{
export_lua test(){}
export_lua void member_func(int);
};
export_lua int global_func();`

