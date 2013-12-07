/*
    access_profiler instruments types and collects usage counts for class fields
    Copyright (C) 2013 Arvid Norberg

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ACCESS_PROFILER_HPP
#define ACCESS_PROFILER_HPP

#include <typeinfo>

namespace access_profiler
{

	namespace detail
	{
		void* allocate_instrumented_type(int size, int type);
		void free_instrumented_type(void* buf, int size, int type);
		int type_idx(std::type_info const* ti, int size);
	}

	template <class T>
	struct instrument_type
	{
		void* operator new(size_t s)
		{
			return detail::allocate_instrumented_type(s
				, detail::type_idx(&typeid(T), sizeof(T)));
		}

		void operator delete(void* buf)
		{
			detail::free_instrumented_type(buf, sizeof(T)
				, detail::type_idx(&typeid(T), sizeof(T)));
		}
	};

}

#endif

