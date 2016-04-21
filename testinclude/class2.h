#pragma once
#include "class1.h"
#define export_lua
export_lua int GlobalFunction(int a);
export_lua int GlobalFunction(int a, int b);
export_lua int GlobalFunction(int a, int b, int c);
export_lua int GlobalFunction(int a, int b, int c, int d);