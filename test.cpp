#include "access_profiler.hpp"
#include <stdio.h>

using namespace access_profiler;

namespace foo
{
	struct test : instrument_type<test>
	{
		test() : a(0), b(0) {}
		char array[50];
		int a;
		int b;
	};
}

namespace bar
{
	struct test2 : instrument_type<test2>
	{
		test2() : a(0), b(0) {}
		char array[5000];
		int a;
		int b;
	};
}

int main(int argc, char* argv[])
{
	foo::test* t1 = new foo::test;
	bar::test2* t2 = new bar::test2;
	bar::test2* t3 = new bar::test2;

	for (int i = 0; i < 10; ++i)
	{
		++t1->a;
		t2->a += t1->a;
		t3->b = t2->a;
	}


	delete t1;
	delete t2;
	delete t3;
}

