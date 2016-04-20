#pragma once
#include "class2.h"

export_lua int GlobalFunction(int) PURE_VIRTUAL_FUNCTION_0
export_lua int GlobalFunction(int,int,int c= 0);

class TestBase
{};

export_lua class TestA : public TestBase
{
public:
	export_lua TestA();
	export_lua TestA(int a);
	export_lua TestA(int a, int b, int c = 0);

	export_lua void TestVec(const std::vector& v);

	export_lua int TestFunc(int);
	export_lua int TestFunc(int,int,int c= 0);

	export_lua static void StaticFunc();
	export_lua int m_val;
	export_lua static int s_val;

public:
	export_lua struct Iterator
	{
		export_lua Iterator(int, int);
		export_lua void Move();
	};
};

namespace NM
{
	export_lua int GloabalFunction(int);
	export_lua int GloabalFunction(int, int, int c = 0);

	export_lua class TestBase
	{};

	export_lua class TestA : public TestBase
	{
	public:
		export_lua TestA();
		export_lua TestA(int a);
		export_lua TestA(int a, int b, int c = 0);

		export_lua int TestFunc(int);
		export_lua int TestFunc(int, int, int c = 0);

		export_lua static void StaticFunc();
		export_lua int m_val;
		export_lua static int s_val;

	public:
		export_lua struct Iterator
		{
			export_lua Iterator(int, int);
			export_lua void Move();
		};
	};

	namespace detail
	{
		export_lua int GloabalFunction(int);
		export_lua int GloabalFunction(int, int, int c = 0);

		export_lua class TestBase
		{};

		export_lua class TestA : public TestBase
		{
		public:
			export_lua TestA();
			export_lua TestA(int a);
			export_lua TestA(int a, int b, int c = 0);

			export_lua int TestFunc(int);
			export_lua int TestFunc(int, int, int c = 0);

			export_lua static void StaticFunc();
			export_lua int m_val;
			export_lua static int s_val;

		public:
			export_lua struct Iterator
			{
				export_lua Iterator(int, int);
				export_lua void Move();
			};
		};
	}
}







