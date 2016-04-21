#pragma once
#include "class2.h"
#define export2lua


export2lua int GlobalFunction(int) PURE_VIRTUAL_FUNCTION_0
export2lua int GlobalFunction(int,int,int c= 0);

class TestBase
{};

export2lua class TestA : public TestBase
{
public:
	export2lua TestA();
	export2lua TestA(int a);
	export2lua TestA(int a, int b, int c = 0);

	export2lua void TestVec(const std::vector& v);

	export2lua int TestFunc(int);
	export2lua int TestFunc(int,int,int c= 0);

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

namespace NM
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







