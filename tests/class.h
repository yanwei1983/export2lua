#include <memory>
#include <functional>

#define export_lua

export_lua int g_c_int = 0;
export_lua double g_c_double = 0.0;
export_lua void gint_add1()
{
	g_c_int++;
}
export_lua void gint_addint(int n)
{
	g_c_int += n;
}
export_lua void gint_addintptr(int* p)
{
	g_c_int += *p;
}
export_lua void gint_addintref(const int& ref)
{
	g_c_int += ref;
}
export_lua void gint_add_intref(int& ref, int n)
{
	ref += n;
}

export_lua void g_addint_double(int n1, double n2)
{
	g_c_int += n1;
	g_c_double += n2;
}

export_lua int get_gint()
{
	return g_c_int;
}
export_lua int& get_gintref()
{
	return g_c_int;
}
export_lua int* get_gintptr()
{
	return &g_c_int;
}
export_lua double get_gdouble()
{
	return g_c_double;
}


export_lua class ff_base
{
public:
	ff_base() {}
	virtual ~ff_base() {}

	export_lua int test_base_callfn(int n)
	{
		return n;
	}
};

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
};
int ff::s_val;
int ff::s_ref;

ff g_ff;
export_lua ff* get_gff_ptr()
{
	return &g_ff;
}

export_lua const ff& get_gff_cref()
{
	return g_ff;
}

std::unordered_map<int, int> g_testhashmap = { { 1,1 },{ 3,2 },{ 5,3 },{ 7,4 } };
export_lua const std::unordered_map<int, int>& push_hashmap()
{
	return g_testhashmap;
}

std::map<int, int> g_testmap = { { 1,1 },{ 3,2 },{ 5,3 },{ 7,4 } };
export_lua const std::map<int, int>& push_map()
{
	return g_testmap;
}

std::set<int> g_testset = { 1,2,3,4,5 };
export_lua const std::set<int> & push_set()
{
	return g_testset;
}


std::vector<int> g_testvec = { 1,2,3,4,5 };
export_lua const std::vector<int>& push_vector()
{
	return g_testvec;
}

std::string g_teststring = "test";
export_lua std::string push_string()
{
	return g_teststring;
}

export_lua std::string connect_string(std::string str1, const std::string& str2, const std::string& str3)
{
	return str1 + str2 + str3;
}

export_lua const std::string& push_string_ref()
{
	return g_teststring;
}


std::shared_ptr<ff> g_ff_shared;
export_lua std::shared_ptr<ff> make_ff()
{
	if (!g_ff_shared)
	{
		g_ff_shared = std::shared_ptr<ff>(new ff);
	}
	return g_ff_shared;
}

export_lua std::shared_ptr<ff> make_ff_to_lua()
{
	return std::shared_ptr<ff>(new ff);
}

export_lua std::weak_ptr<ff> make_ff_weak()
{
	return std::weak_ptr<ff>(g_ff_shared);
}

export_lua bool visot_ff(ff* pFF)
{
	if (pFF)
	{
		return true;
	}
	return false;
}

export_lua void visot_ff_ref(ff& refFF)
{
	std::cout << "visot_ff(" << refFF.m_val << ")" << std::endl;
}
export_lua void visot_ff_const_ref(const ff& refFF)
{
	std::cout << "visot_ff(" << refFF.m_val << ")" << std::endl;
}

export_lua bool visot_ff_shared(std::shared_ptr<ff> pFF)
{
	if (pFF)
	{
		return true;
	}
	return false;
}

export_lua bool visot_ff_weak(std::weak_ptr<ff> pWeakFF)
{
	if (pWeakFF.expired() == false)
	{
		std::shared_ptr<ff> pFF = pWeakFF.lock();
		if (pFF)
		{
			return true;
		}
	}
	return false;
}

class ff_nodef
{
public:
	ff_nodef(int n = 0) :m_val(n) {}
	ff_nodef(const ff_nodef& rht) :m_val(rht.m_val) {}
	ff_nodef(ff_nodef&& rht) :m_val(rht.m_val) {}

	~ff_nodef()
	{
		std::cout << "ff_nodef::~ff_nodef()" << std::endl;
	}

	int m_val;
};

ff_nodef g_ff_nodef;
export_lua ff_nodef* make_ff_nodef()
{
	return &g_ff_nodef;
}

export_lua bool visot_ff_nodef(ff_nodef* pFF)
{
	if (pFF)
	{
		return true;
	}
	return false;
}

std::shared_ptr<ff_nodef> g_ff_nodef_shared;
export_lua std::shared_ptr<ff_nodef> make_ff_nodef_shared()
{
	if (!g_ff_nodef_shared)
	{
		g_ff_nodef_shared = std::shared_ptr<ff_nodef>(new ff_nodef);
	}
	return g_ff_nodef_shared;
}

export_lua bool visot_ff_nodef_shared(std::shared_ptr<ff_nodef> pFF)
{
	if (pFF)
	{
		return true;
	}
	return false;
}

export_lua unsigned long long addUL(unsigned long long a, unsigned long long b)
{
	return a + b;
}

export_lua long long Number2Interger(double v)
{
	return (long long)(v);
}


export_lua int test_overload(int n)
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

export_lua int test_lua_function(std::function<int(int)> func)
{
	return func(1);
}
export_lua int test_lua_function_ref(const std::function<int(int)>& func)
{
	return func(1);
}

export_lua std::function<int(int)> get_c_function()
{
	auto func = [](int v)->int
	{
		return v + 1;
	};
	return std::function<int(int)>(func);
}


//func must be release before lua close.....user_conctrl
std::function<int(int)> g_func_lua;
export_lua void store_lua_function(std::function<int(int)> func)
{
	g_func_lua = func;
}

export_lua int use_stored_lua_function()
{
	return g_func_lua(1);
}