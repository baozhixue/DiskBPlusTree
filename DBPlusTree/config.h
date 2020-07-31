#pragma once

#include <array>
#include <iostream>
#include <string.h>
#include <cstdint>

constexpr uint8_t DATA_FIELD_NUM = 64;
struct Data {
	uint64_t key;
	// If we applied the verion of the delta, we say the data has this version
	uint64_t version;
	uint64_t field[DATA_FIELD_NUM];
};

struct DeltaItem {
	uint64_t key;
	// the change of the data compare to previous version.
	uint32_t delta[DATA_FIELD_NUM];
};

// all the deltas in one DeltaPacketket have the same version
// DeltaItem can have same or different keys.
struct DeltaPacket {
	// the global sequence number of the packet
	uint64_t version;
	uint16_t delta_count;
	DeltaItem deltas[0];
};


/*
	DISKIO,控制是否读写类型
*/
enum class IOTYPE
{
	O_READ,	// 仅读,默认
	O_REWR  // 读写
};


/*
	文件描述头
*/
struct FileHeader
{
	char fileName[32];
};
constexpr size_t FileHeaderOffset = sizeof(FileHeader);


using FPTR = uint64_t;	// file offset
using KEY = uint64_t;	// 
constexpr size_t MaxM = 253;
constexpr uint64_t FNUL = 0;	// file nullptr

struct Data2V2 {
	Data2V2() {
		memset(data, 0, 512);
	}
	uint64_t data[DATA_FIELD_NUM];
	uint64_t pre_data = FNUL;
	uint64_t next_data = FNUL;
};

struct Append {
	Append() {
		memset(data, 0, 512);
	}
	uint64_t data[DATA_FIELD_NUM];
};

enum class PointerLevel
{
	PL_FIX,	// 0,固定的
	PL_FRE,	// 1,未被使用的或被使用过的，但是可以再次使用的 
	PL_USE,	// 2,使用中
	PL_NUL	// 3,空指针
};

