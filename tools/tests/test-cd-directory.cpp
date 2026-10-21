// Match player builds: the historical capacity assert was compiled out.
#define NDEBUG
#ifndef DIRECTORY_KEEP_DUPLICATES
#define FIX_BUGS
#endif
#include <cassert>
#include <cstdint>
#include <climits>
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
using int32 = std::int32_t;
using uint32 = std::uint32_t;
class CGeneral {
public:
	static int faststricmp(const char *a, const char *b) {
		while(*a && std::tolower(static_cast<unsigned char>(*a)) == std::tolower(static_cast<unsigned char>(*b))){ ++a; ++b; }
		return std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
	}
};
#include "cd-directory-types.inc"
#include "cd-directory-production.inc"

static void Check(bool value, const char *message)
{
	if(!value){ std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
static CDirectory::DirectoryInfo Item(int i)
{
	CDirectory::DirectoryInfo item{};
	item.offset = static_cast<uint32>(i * 7);
	item.size = static_cast<uint32>(i + 1);
	std::snprintf(item.name, sizeof(item.name), "extra_%04d", i);
	return item;
}

int main()
{
	const int capacities[] = {0, 1, 128};
	for(int initial : capacities){
		CDirectory directory(initial);
		for(int i = 0; i < 4096; ++i){
			const auto item = Item(i);
			directory.AddItem(item, 3);
			Check(directory.numEntries == i + 1, "entry count");
			Check(directory.maxEntries >= directory.numEntries, "129th and later entries fit allocated capacity");
		}
		for(int i = 0; i < 4096; ++i){
			auto item = Item(i);
			uint32 offset = 0, size = 0;
			Check(directory.FindItem(item.name, offset, size), "all entries survive repeated growth");
			Check(offset == (item.offset | (3u << 24)) && size == item.size, "image selector and sector range preserved");
		}
		uint32 offset = 0, size = 0;
		Check(directory.FindItem("EXTRA_0000", offset, size), "case insensitive lookup");
		Check(!directory.FindItem("missing", offset, size), "missing entry");
	}
	CDirectory full(1);
	full.AddItem(Item(1));
	// Growth must take a snapshot before releasing the aliased entry array.
	full.AddItem(full.entries[0]);
#ifdef FIX_BUGS
	Check(full.numEntries == 1 && full.maxEntries == 1, "duplicate at capacity preserves first archive entry");
#else
	Check(full.numEntries == 2 && full.maxEntries >= 2, "aliased entry survives growth");
	Check(full.entries[1].offset == Item(1).offset, "aliased record contents");
#endif
	std::puts("PASS: production directory stores 4096 entries from capacities 0/1/128 with NDEBUG; metadata, lookup and duplicates preserved");
}
