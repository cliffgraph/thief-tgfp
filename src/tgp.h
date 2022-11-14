#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型
#include "tgf.h"

enum TGPACKET_CMD : uint16_t
{
	TGPACKET_NONE			= 0x0000,
	//
	TGPACKET_REQUEST_PLAY	= 0x0001,	// TGFE -> TGFP, 再生を要求する	index=開始インデックス
	TGPACKET_REQUEST_STOP	= 0x0002,	// TGFE -> TGFP, 再生停止を要求する
	TGPACKET_TG_ATOMS		= 0x0003,	// TGFE -> TGFP, 再生データ、inde=データのインデックス
	//
	TGPACKET_REQUEST_ATOMS	= 0x0010,	// TGFP -> TGFE, 再生データの返信を要求する	index=要求データのインデックス
};

#pragma pack(push,1)
struct TGPACKET
{
	TGPACKET_CMD	cmd;
	uint32_t		index;
	uint32_t		maxIndex;
	timecode_t		tc;
	uint16_t		num;		// number of atoms[]
	TGF_ATOM		atoms[1];
};
#pragma pack(pop)

