
#define export_lua

namespace TT
{
	export_lua int GlobalFunction(int a);
	export_lua int GlobalFunction(int a, int b);
	export_lua int GlobalFunction(int a, int b, int c);
	export_lua int GlobalFunction(int a, int b, int c, int d);

	export_lua class ff : public ff_base
	{
	public:
		export_lua ff()
		{
			s_ref++;
		}
		export_lua ff(int a) :m_val(a)
		{
			s_ref++;
		}
		ff(double a)
		{
			s_ref++;
		}
		export_lua ff(double, unsigned char, int a) :m_val(a)
		{
			s_ref++;
		}
		ff(const ff& rht) :m_val(rht.m_val)
		{
			s_ref++;
		}
		ff(ff&& rht) = default;

		ff& operator=(const ff& rht)
		{
			if (&rht != this)
			{
				m_val = rht.m_val;
			}
			std::cout << "ff::operator =" << std::endl;

			return *this;
		}

		~ff()
		{
			s_ref--;
		}
		export_lua bool test_memfn()
		{
			return true;
		}
		export_lua bool test_const() const
		{
			return true;
		}

		export_lua int add(int n)
		{
			return m_val += n;
		}

		export_lua int add_ffptr(ff* pff)
		{
			return m_val + pff->m_val;
		}

		export_lua int add_ffcref(const ff& rht)
		{
			return m_val + rht.m_val;
		}

		export_lua int test_overload(int n) const
		{
			return n;
		}

		export_lua int test_overload(int n, double d)
		{
			return n + (int)d;
		}

		export_lua int test_overload(int n1, int n2, double d)
		{
			return n1 + n2 + (int)d;
		}

		export_lua static int get_static_val(int v)
		{
			return s_val + v;
		}

		export_lua int getVal()const { return m_val; }
		export_lua void setVal(int v) { m_val = v; }

		export_lua int m_val = 0;
		export_lua static int s_val;
		export_lua static int s_ref;


	public:
		export_lua class Iterator
		{
		public:
			Iterator();
			~Iterator();
		public:
			export_lua void Move();
			export_lua void Test();
		};
	};

}
