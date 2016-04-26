#pragma once
#include "class2.h"
#include<vector>
#define export_lua
class A;
class B;
export_lua int GlobalFunction(int a);
export_lua int GlobalFunction(double a);
export_lua int GlobalFunction(A* a);
export_lua int GlobalFunction(B* a);

export_lua int GlobalFunction(int,int,int b = 11+5,int c = 10 );

export_lua class TestBase
{};

export_lua enum CT_TEST
{
	CT_TEST_1,
	CT_TEST_2,
	CT_TEST_3,
	CT_TEST_4,
};

export_lua class TestA : public TestBase
{
	export_lua TestA();
	export_lua TestA(int a);
	export_lua TestA(int a, int b, int c = 10);

	export_lua void TestVec(const std::vector& v);
	export_lua void TestString(int, const std::string& v = "");
	export_lua void TestString(const std::string& v = std::string(""));

	export_lua int TestFunc(int) PURE_VIRTUAL_FUNCTION_0
	export_lua int TestFunc(int,int,int c= 10);

	export_lua static void StaticFunc();
	export_lua int m_val;
	export_lua static int s_val;
	using int_ptr = int*;
	typedef int int_type;
	export_lua struct Iterator
	{
		export_lua Iterator(int, int);
		export_lua void Move();
	};

		
};

namespace NM
{
	export_lua int GloabalFunction(int);
	export_lua int GloabalFunction(int, int, int c = 10);

	export_lua class TestBase
	{};

	export_lua class TestA : public TestBase
	{
		export_lua TestA();
		export_lua TestA(int a);
		export_lua TestA(int a, int b, int c = 10);
		export_lua int TestFunc(int);
	public:

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

		export_lua enum CT_TEST
		{
			CT_TEST_1,
			CT_TEST_2,
			CT_TEST_3,
			CT_TEST_4,
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



export_lua class TestC : public NM::detail::TestBase
{
};

export_lua class TestD : public TestA
{
};

