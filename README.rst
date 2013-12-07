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
more user-friendly output, use the struct_layout tool and
use the profile as input.

