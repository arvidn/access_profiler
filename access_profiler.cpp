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

#include <signal.h>
#include <stdlib.h> // for malloc/free
#include <sys/mman.h> // for mprotect
#include <cxxabi.h>

#include <unordered_map>
#include <cstdint>
#include <cinttypes> // for PRId64
#include <typeinfo>
#include <vector>
#include <mutex>

struct type_t
{
	// the type this page belongs to
	std::uint16_t type_idx;
	
	void* base_ptr;
	size_t size;
};

std::mutex pagemutex;

// maps page addresses to the type that page belongs to
std::unordered_map<uintptr_t, type_t> pagemap;

std::mutex typemutex;
std::unordered_map<std::type_info const*, std::uint8_t> typemap;
int next_typeidx = 0;

struct access_t
{
	int size;
	std::atomic<std::uint64_t>* counter;
};

enum { max_types = 300 };
std::vector<access_t> access;

// when the sigsegv handler unprotects a page and enters
// single step mode, it saves the page that was unprotected
// here, and protects it again in the single step handler
// (i.e. one instruction later)
pthread_key_t last_page = 0;

void segv_handler(int, siginfo_t *info, void* uap)
{
	ucontext_t* uc = (ucontext_t*)uap;

	uintptr_t page_addr = uintptr_t(info->si_addr);
	page_addr &= ~4095;

	void* base_ptr;
	int type_size;
	int type_idx;
	{
		std::lock_guard<std::mutex> l(pagemutex);
		auto i = pagemap.find(page_addr);
		if (i == pagemap.end()) return;
		type_t& t = i->second;
		base_ptr = t.base_ptr;
		type_size = t.size;
		type_idx = t.type_idx;
	}

	// the allocator itself apparently touches some memory
	// of unrelated pages when freeing
	// we must still allow the access, but don't record it
	if ((char*)info->si_addr < (char*)base_ptr + type_size)
	{
		// record the access
		int offset = uintptr_t(info->si_addr) - uintptr_t(base_ptr);
		++access[type_idx].counter[offset];
	}

	// unprotect the page
	mprotect((void*)page_addr, 4096, PROT_READ | PROT_WRITE);

	// save the page address so the single step handler
	// know which page to protect again
	pthread_setspecific(last_page, (void*)page_addr);

	// turn on single step
#ifdef __LP64__
	uc->uc_mcontext->__ss.__rflags |= 0x100;
#else
	uc->uc_mcontext->__ss.__eflags |= 0x100;
#endif
}

void single_step_handler(int signo, siginfo_t* info, void* uap)
{
	ucontext_t* uc = (ucontext_t*)uap;

	// turn off single step
#ifdef __LP64__
	uc->uc_mcontext->__ss.__rflags &= ~0x100;
#else
	uc->uc_mcontext->__ss.__eflags &= ~0x100;
#endif

	void* buf = pthread_getspecific(last_page);

	// and protect the page again
	mprotect(buf, 4096, PROT_NONE);
}

namespace access_profiler
{

namespace detail
{

int type_idx(std::type_info const* ti, int size)
{
	std::lock_guard<std::mutex> l(typemutex);
	auto i = typemap.find(ti);
	if (i != typemap.end()) return i->second;
	if (next_typeidx >= max_types) return -1;

	access[next_typeidx].counter = new std::atomic<std::uint64_t>[size];
	for (int i = 0; i < size; ++i) access[next_typeidx].counter[i] = 0;
	access[next_typeidx].size = size;
	typemap[ti] = next_typeidx++;
	return next_typeidx-1;
}

void* allocate_instrumented_type(int size, int type)
{
	if (type == -1) return malloc(size);

	// round up to even page
	int page_size = (size + 4095) & ~4095;

	void* buf = valloc(page_size);
	if (buf == nullptr) return buf;

	{
		std::lock_guard<std::mutex> l(pagemutex);
		for (uintptr_t ptr = (uintptr_t)buf; ptr < uintptr_t(buf) + page_size;
			ptr += 4096)
		{
			type_t& t = pagemap[ptr];
			t.base_ptr = buf;
			t.size = size;
			t.type_idx = type;
		}
	}

	mprotect(buf, page_size, PROT_NONE);

	return buf;
}

void free_instrumented_type(void* buf, int size, int type)
{
	if (type == -1)
	{
		free(buf);
		return;
	}

	// round up to even page
	size = (size + 4095) & ~4095;

	mprotect(buf, size, PROT_READ | PROT_WRITE);

	{
		std::lock_guard<std::mutex> l(pagemutex);
		for (uintptr_t ptr = (uintptr_t)buf; ptr < uintptr_t(buf) + size;
			ptr += 4096)
		{
			pagemap.erase(ptr);
		}
	}

	free(buf);
}

} // detail

void init_instrumentation()
{
	pthread_key_create(&last_page, NULL);

	struct sigaction sa;
	sa.sa_sigaction = &segv_handler;
	sa.sa_flags = SA_SIGINFO | SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGSEGV, &sa, NULL);
	sigaction(SIGBUS, &sa, NULL);

	sa.sa_sigaction = &single_step_handler;
	sigaction(SIGTRAP, &sa, NULL);

	access.resize(500);
}

void print_report()
{
	FILE* out = fopen("access_profile.out", "w+");
	if (out == nullptr)
	{
		fprintf(stderr, "failed to open \"access_profile.out\" "
			"for writing: (%d) %s\n"
			, errno, strerror(errno));
		return;
	}

	std::lock_guard<std::mutex> l(typemutex);
	for (auto i : typemap)
	{
		int dummy;
		fprintf(out, "\n%s\n", abi::__cxa_demangle(i.first->name(), NULL, NULL, &dummy));

		int type_idx = i.second;
		access_t& a = access[type_idx];

		// don't show fields with less than 1% of max
		// hit rate
		for (int i = 0; i < a.size; ++i)
		{
			std::uint64_t cnt = a.counter[i].load();
			if (cnt == 0) continue;
			fprintf(out, "   %4d: %" PRId64 "\n", i, cnt);
		}
	}

	fclose(out);
}

static struct static_init_t
{
	static_init_t()
	{
		init_instrumentation();
	}

	~static_init_t()
	{
		print_report();
	}

} initializer;

} // access_profiler

