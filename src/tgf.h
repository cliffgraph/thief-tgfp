#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

enum TGF_MARK : uint8_t
{
	TGF_M_NOP		= 0x00,
	TGF_M_SYSINFO	= 0x01,
	TGF_M_WAIT		= 0x02,
	TGF_M_TC		= 0x03,
	TGF_M_OPLL		= 0x04,
	TGF_M_PSG		= 0x05,
	TGF_M_SCC		= 0x06,

};

typedef uint32_t timecode_t;

#pragma pack(push,1)
struct TGF_ATOM
{
	TGF_MARK	mark;
	uint16_t	data1;
	uint16_t	data2;
	TGF_ATOM() : mark(TGF_M_NOP), data1(0), data2(0) {}
	TGF_ATOM(const TGF_MARK c) : mark(c), data1(0), data2(0) {}
};
#pragma pack(pop)

