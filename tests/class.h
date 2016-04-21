#pragma once
//#include "class2.h"
#define export2lua
#include<vector>
export2lua int GlobalFunction(int a);
export2lua int GlobalFunction(int,int,int b = 11+5,int c = 10 );

export2lua class TestBase
{};

export2lua class TestA : public TestBase
{
public:
	export2lua TestA();
	export2lua TestA(int a);
	export2lua TestA(int a, int b, int c = 10);

	export2lua void TestVec(const std::vector& v);
	export2lua void TestString(int, const std::string& v = "");
	export2lua void TestString(const std::string& v = std::string(""));

	export2lua int TestFunc(int) PURE_VIRTUAL_FUNCTION_0
	export2lua int TestFunc(int,int,int c= 10);

	export2lua static void StaticFunc();
	export2lua int m_val;
	export2lua static int s_val;
	using int_ptr = int*;
	typedef int int_type;
public:
	export2lua struct Iterator
	{
		export2lua Iterator(int, int);
		export2lua void Move();
	};
};

namespace NM
{
	export2lua int GloabalFunction(int);
	export2lua int GloabalFunction(int, int, int c = 10);

	export2lua class TestBase
	{};

	export2lua class TestA : public TestBase
	{
	public:
		export2lua TestA();
		export2lua TestA(int a);
		export2lua TestA(int a, int b, int c = 10);

		export2lua int TestFunc(int);
		export2lua int TestFunc(int, int, int c = 0);

		export2lua static void StaticFunc();
		export2lua int m_val;
		export2lua static int s_val;

	public:
		export2lua struct Iterator
		{
			export2lua Iterator(int, int);
			export2lua void Move();
		};
	};

	namespace detail
	{
		export2lua int GloabalFunction(int);
		export2lua int GloabalFunction(int, int, int c = 0);

		export2lua class TestBase
		{};

		export2lua class TestA : public TestBase
		{
		public:
			export2lua TestA();
			export2lua TestA(int a);
			export2lua TestA(int a, int b, int c = 0);

			export2lua int TestFunc(int);
			export2lua int TestFunc(int, int, int c = 0);

			export2lua static void StaticFunc();
			export2lua int m_val;
			export2lua static int s_val;

		public:
			export2lua struct Iterator
			{
				export2lua Iterator(int, int);
				export2lua void Move();
			};
		};
	}
}



export2lua class TestC : public NM::detail::TestBase
{
};

export2lua class TestD : public TestA
{
};

