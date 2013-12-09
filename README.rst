access_profiler
===============

``access_profiler`` is a heavy-weight class field access
profiler, implemented as C++ library.

to use this profiler, include ``"access_profiler.hpp"``
and make the types you want to instrument derive from
``access_profiler::instrument_type< your-type >`` (i.e. you
need to specify your type as the template argument).

in you ``Jamfile``, add a dependency to the access_profiler
library.

When you terminate your program, the access counters
for your types fields will be printed to "access_profile.out"
in current working directory. This file lists all instrumented
types and the access counters for offsets into those types.

To combine this information with the debug information for
more user-friendly output, use the `struct_layout`_ tool and
use the profile as input.

.. _`struct_layout`: https://github.com/arvidn/struct_layout

.. note::
	``access_profiler`` is currently not compatible with
	``std::make_shared`` or similar functions, since those
	won't invoke the ``new`` operator. To profile such types,
	convert them to regular ``std::shared_ptr`` constructor
	which still allocates the object with ``new``.

output format
-------------

Each instrumented type has its fully qualified name printed
on a single line preceded by a blank line (even the first type).

After each instrumented type follows a list of offsets into that
type, colon, and the number of times that offset was accessed. The
counter does not distinguish between reads and writes. These
lines are indented by at least 3 spaces, but the offset is right
adjusted and may contain some leading spaces too.

The general outline looks like this:

.. parsed-literal::
	
	*<blank line>*
	*<fully qualified name of instrumented type>*
	   *<offset>*:*<hit count>*
	   *<offset>*:*<hit count>*

example usage
-------------

::
	
	#include "access_profiler.hpp"
	#include <stdio.h>
	
	struct test : access_profiler::instrument_type<test>
	{
		test() : a(0), b(0) {}
		char array[50];
		int a;
		int b;
	};
	
	int main(int argc, char* argv[])
	{
		test* t1 = new test;
	
		for (int i = 0; i < 10; ++i)
		{
			++t1->a;
			t1->b += t1->a;
		}
	
		printf("%d\n", t1->b);
	
		delete t1;
	}

example output
--------------

output from a debug build::

	test
	     52: 31
	     56: 22

output from a release build::

	test
	     52: 1

