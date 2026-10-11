#include "BigEndianReader.h"
#include <cstring>

BigEndianReader::BigEndianReader(const std::string& filename) : m_filename(filename) {}

bool BigEndianReader::open() {
	m_file.open(m_filename, std::ios::binary);
	if (!m_file) return false;
	m_file.seekg(0, std::ios::end);
	m_size = static_cast<uint64_t>(m_file.tellg());
	m_file.seekg(0, std::ios::beg);
	return true;
}
bool BigEndianReader::good() const { return m_file.good(); }
uint64_t BigEndianReader::size() const { return m_size; }
void BigEndianReader::seek(uint64_t p) { m_file.clear(); m_file.seekg(static_cast<std::streamoff>(p), std::ios::beg); }
uint64_t BigEndianReader::tell() { return static_cast<uint64_t>(m_file.tellg()); }

uint8_t BigEndianReader::readU8() {
	char c = 0; m_file.read(&c, 1);
	return static_cast<uint8_t>(static_cast<unsigned char>(c));
}
uint16_t BigEndianReader::readU16() {
	uint8_t b[2]; m_file.read(reinterpret_cast<char*>(b), 2);
	return (uint16_t(b[0]) << 8) | uint16_t(b[1]);
}
uint32_t BigEndianReader::readU32() {
	uint8_t b[4]; m_file.read(reinterpret_cast<char*>(b), 4);
	return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) |
		(uint32_t(b[2]) << 8) | uint32_t(b[3]);
}
uint64_t BigEndianReader::readU64() {
	uint64_t v = 0;
	for (int i = 0; i < 8; ++i) v = (v << 8) | readU8();
	return v;
}
int32_t BigEndianReader::readI32() { return static_cast<int32_t>(readU32()); }
float BigEndianReader::readFloat() {
	uint32_t v = readU32(); float r; std::memcpy(&r, &v, sizeof(r)); return r;
}
double BigEndianReader::readDouble() {
	uint64_t v = readU64(); double r; std::memcpy(&r, &v, sizeof(r)); return r;
}
void BigEndianReader::readBytes(void* dst, size_t size) {
	m_file.read(reinterpret_cast<char*>(dst), static_cast<std::streamsize>(size));
}
