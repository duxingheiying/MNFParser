#pragma once
#include <cstdint>
#include <fstream>
#include <string>

class BigEndianReader {
public:
	explicit BigEndianReader(const std::string& filename);
	bool open();
	bool good() const;
	uint64_t size() const;
	void seek(uint64_t position);
	uint64_t tell();

	uint8_t readU8();
	uint16_t readU16();
	uint32_t readU32();
	uint64_t readU64();
	int32_t readI32();
	float readFloat();
	double readDouble();
	void readBytes(void* dst, size_t size);

private:
	std::string filename_;
	std::ifstream file_;
	uint64_t size_ = 0;
};
