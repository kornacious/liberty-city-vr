#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace QuestAstcCache
{
constexpr const char *FileName = "texture-cache-astc.bin";
constexpr size_t HeaderSize = 20;
constexpr size_t EntrySize = 78;
constexpr size_t MaxLevels = 13;
constexpr int MaxDimension = 4096;

inline uint64_t ReadLE(const uint8_t *data, size_t bytes)
{
	uint64_t value = 0;
	for(size_t i = 0; i < bytes; i++)
		value |= uint64_t(data[i]) << (i * 8);
	return value;
}

struct Entry
{
	uint64_t offset;
	uint16_t width, height;
	uint8_t levels, alpha;
	std::array<uint32_t, MaxLevels> levelSizes = {};
	uint32_t size = 0;

	bool CalculateSizes()
	{
		if(width == 0 || height == 0 || width > MaxDimension || height > MaxDimension ||
		   levels == 0 || levels > MaxLevels || alpha > 1)
			return false;
		uint32_t w = width, h = height;
		for(size_t level = 0; level < levels; level++){
			levelSizes[level] = ((w + 5) / 6) * ((h + 5) / 6) * 16;
			size += levelSizes[level];
			if(w == 1 && h == 1 && level + 1 < levels)
				return false;
			w = std::max(1u, w / 2);
			h = std::max(1u, h / 2);
		}
		return true;
	}
};

class Index
{
	using File = std::unique_ptr<FILE, decltype(&fclose)>;
	File payload{nullptr, fclose};
	mutable std::mutex fileMutex;
	std::unordered_map<std::string, Entry> entries;

public:
	bool Open(const std::string &source)
	{
		File file(fopen(source.c_str(), "rb"), fclose);
		if(!file || fseek(file.get(), 0, SEEK_END) != 0)
			return false;
		const long fileSize = ftell(file.get());
		if(fileSize < long(HeaderSize) || fseek(file.get(), 0, SEEK_SET) != 0)
			return false;
		uint8_t header[HeaderSize];
		if(fread(header, 1, sizeof(header), file.get()) != sizeof(header) ||
		   memcmp(header, "LCVASTC1", 8) != 0)
			return false;
		const uint64_t indexOffset = ReadLE(header + 8, 8);
		const uint64_t count = ReadLE(header + 16, 4);
		if(count == 0 || count > 65536 || indexOffset < HeaderSize ||
		   indexOffset > uint64_t(fileSize) || count * EntrySize != uint64_t(fileSize) - indexOffset ||
		   fseek(file.get(), long(indexOffset), SEEK_SET) != 0)
			return false;

		std::unordered_map<std::string, Entry> loaded;
		loaded.reserve(size_t(count));
		uint64_t expectedOffset = HeaderSize;
		for(uint64_t i = 0; i < count; i++){
			uint8_t record[EntrySize];
			if(fread(record, 1, sizeof(record), file.get()) != sizeof(record))
				return false;
			const std::string key(reinterpret_cast<const char *>(record), 64);
			Entry entry = {ReadLE(record + 64, 8), uint16_t(ReadLE(record + 72, 2)),
				uint16_t(ReadLE(record + 74, 2)), record[76], record[77]};
			if(key.find_first_not_of("0123456789ABCDEF") != std::string::npos ||
			   entry.offset != expectedOffset || !entry.CalculateSizes() ||
			   entry.size > indexOffset - expectedOffset || !loaded.emplace(key, entry).second)
				return false;
			expectedOffset += entry.size;
		}
		if(expectedOffset != indexOffset)
			return false;
		payload = std::move(file);
		entries = std::move(loaded);
		return true;
	}

	const Entry *Find(const std::string &key) const
	{
		auto it = entries.find(key);
		return it == entries.end() ? nullptr : &it->second;
	}

	bool Read(const Entry &entry, std::vector<uint8_t> &blocks) const
	{
		std::lock_guard<std::mutex> lock(fileMutex);
		if(!payload || fseek(payload.get(), long(entry.offset), SEEK_SET) != 0)
			return false;
		blocks.resize(entry.size);
		return fread(blocks.data(), 1, blocks.size(), payload.get()) == blocks.size();
	}
};
}
