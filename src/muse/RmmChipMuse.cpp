﻿#include "pch.h"
#include "tools.h"
#include "constools.h"
#include "RmmChipMuse.h"
#include <chrono>
#include <thread>	// for sleep_for

#ifdef USE_RAMSXMUSE
#include <wiringPi.h>
#include <wiringPiI2C.h>
#endif

const static int GPIO_D0			= 0;
const static int GPIO_D1			= 1;
const static int GPIO_D2			= 2;
const static int GPIO_D3			= 3;
const static int GPIO_D4			= 4;
const static int GPIO_D5			= 5;
const static int GPIO_D6			= 6;
const static int GPIO_D7			= 7;

const static int GPIO_D8			= 15;
const static int GPIO_D9			= 16;
const static int GPIO_D10			= 10;
const static int GPIO_D11			= 11;
const static int GPIO_D12			= 26;
const static int GPIO_D13			= 27;
const static int GPIO_D14			= 28;
const static int GPIO_D15			= 29;

const static int GPIO_RESET			= 14;	// 0=RESET
const static int GPIO_AEX1			= 12;	// bit0,    Use in "HraSCC"
const static int GPIO_AEX0			= 13;	// bit1,	AEX=0-3 = {90xxh,98xxh,B8xxh,BFxxh}
const static int GPIO_A0			= 21;	// 0=register addr, 1=resister data
const static int GPIO_CSWR_YM2413	= 23;	// 0=Chip select "YM2413"
const static int GPIO_CSWR_YMZ294	= 24;	// 0=Chip select "YMZ294"
const static int GPIO_CSWR_HraSCC	= 25;	// 0=Chip select "HraSCC"

int RmmChipMuse::s_AlphaCount = 0;

struct CHIP_REG_VAL
{
	uint32_t addr, val;
}; 
static const CHIP_REG_VAL g_YM2413_INIT_REGISTER[] =
{
	{ 0x0010, 0x00 },
	{ 0x0011, 0x00 },
	{ 0x0012, 0x00 },
	{ 0x0013, 0x00 },
	{ 0x0014, 0x00 },
	{ 0x0015, 0x00 },
	{ 0x0016, 0x00 },
	{ 0x0017, 0x00 },
	{ 0x0018, 0x00 },
	{ 0x0020, 0x00 },
	{ 0x0021, 0x00 },
	{ 0x0022, 0x00 },
	{ 0x0023, 0x00 },
	{ 0x0024, 0x00 },
	{ 0x0025, 0x00 },
	{ 0x0026, 0x00 },
	{ 0x0027, 0x00 },
	{ 0x0028, 0x00 },
	{ 0x0030, 0x00 },
	{ 0x0031, 0x00 },
	{ 0x0032, 0x00 },
	{ 0x0033, 0x00 },
	{ 0x0034, 0x00 },
	{ 0x0035, 0x00 },
	{ 0x0036, 0x00 },
	{ 0x0037, 0x00 },
	{ 0x0038, 0x00 },
};

static const CHIP_REG_VAL g_HraSCC_INIT_REGISTER[] =
{

	{ 0xbffe, 0x30 },		// SCC+ mode
	{ 0xb000, 0x80 },
	{ 0xb8aa, 0x00 },		// CH.A volume
	{ 0xb8ab, 0x00 },		// CH.B volume
	{ 0xb8ac, 0x00 },		// CH.C volume
	{ 0xb8ad, 0x00 },		// CH.D volume
	{ 0xb8ae, 0x00 },		// CH.E volume
	{ 0xb88f, 0x00 },		// disable
	{ 0xb880, 0x00 },
	{ 0xb881, 0x00 },
	{ 0xb882, 0x00 },
	{ 0xb883, 0x00 },
	{ 0xb884, 0x00 },
	{ 0xb885, 0x00 }, 
	{ 0xb886, 0x00 },
	{ 0xb887, 0x00 },
	{ 0xb888, 0x00 },
	{ 0xb889, 0x00 },

	{ 0xBFFE, 0x20 },		// SCC mode
	{ 0x9000, 0x3f },
	{ 0x988a, 0x00 },		// CH.A volume
	{ 0x988b, 0x00 },		// CH.B volume
	{ 0x988c, 0x00 },		// CH.C volume
	{ 0x988d, 0x00 },		// CH.D Dvolume
	{ 0x988e, 0x00 },		// CH.E volume
	{ 0x988f, 0x00 },		// disable
	{ 0x9880, 0x00 },
	{ 0x9881, 0x00 },
	{ 0x9882, 0x00 },
	{ 0x9883, 0x00 },
	{ 0x9884, 0x00 },
	{ 0x9885, 0x00 }, 
	{ 0x9886, 0x00 },
	{ 0x9887, 0x00 },
	{ 0x9888, 0x00 },
	{ 0x9889, 0x00 }
};

static const float OPLLHZ = 3579545.f;
static const int OPLLwait12 = static_cast<int>(((16.0f * 1000000 / OPLLHZ) + 0.99f) + 1);
static const int OPLLwait84 = static_cast<int>(((90.0f * 1000000 / OPLLHZ) + 0.99f) + 1);

/** コンストラクタ
**/
RmmChipMuse::RmmChipMuse(const TARGETCHIP target) :
	m_TergetChip(target)
{
	// 最初に生成されたインスタンスのみがHWのセットアップを行う
	if( s_AlphaCount == 0 ) {
		// RaSCCのクロックを設定する
		initClockfoHraSCC();
		// Setup GPIO
		initGpio();
		setupGpio();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		// CHIP DEVICE RESET
		resetDevice();
	}
	++s_AlphaCount;
	return;	
}

/** デストラクタ
**/
RmmChipMuse::~RmmChipMuse()
{
	--s_AlphaCount;
	if (s_AlphaCount == 0) {
		initRegs();
		initGpio();
	}
	return;	
}

bool RmmChipMuse::Init(const bool bWithoutSCC)
{
	// レジスタの初期化をする
	initRegs(bWithoutSCC);
	return true;
}

bool RmmChipMuse::ResetChip()
{
	resetDevice();
	return true;
}

bool  RmmChipMuse::SetRegister(const uint32_t addr, const uint32_t data)
{
	switch(m_TergetChip) {
		case OPLL:	setOPLL(addr, data);	break;
		case PSG:	setPSG(addr, data);		break;
		case SCC:	setSCC(addr, data);		break;
		default:	break;
	}
	return true;
}

bool  RmmChipMuse::SetRegisterAddr(const uint32_t addr)
{
#ifdef __WIRING_PI_H__
	switch(m_TergetChip) {
		case OPLL:
		{
			setRegAddress(addr);
			digitalWrite(GPIO_CSWR_YM2413, 0);
			std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
			digitalWrite(GPIO_CSWR_YM2413, 1);
			usleep(OPLLwait12);	// 12us < wait for ADDR WR
			break;
		}
		case PSG:
		{
			setRegAddress(addr);
			digitalWrite(GPIO_CSWR_YMZ294, 0);
			std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
			digitalWrite(GPIO_CSWR_YMZ294, 1);
			usleep(OPLLwait12);	// 12us < wait for ADDR WR
			break;
		}
		case SCC:
		default:
			break;
	}
#endif
	return true;
}

bool  RmmChipMuse::SetRegisterData(const uint32_t data)
{
#ifdef __WIRING_PI_H__
	switch(m_TergetChip) {
		case OPLL:
		{
			setRegData(data);
			digitalWrite(GPIO_CSWR_YM2413, 0);
			std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
			digitalWrite(GPIO_CSWR_YM2413, 1);
			usleep(OPLLwait84);	// 84us < wait for DATA WR
			break;
		}
		case PSG:
		{
			setRegData(data);
			digitalWrite(GPIO_CSWR_YMZ294, 0);
			std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
			digitalWrite(GPIO_CSWR_YMZ294, 1);
			usleep(OPLLwait84);	// 84us < wait for DATA WR
			break;
		}
		case SCC:
		default:
			break;
	}
#endif
	return true;
}

void  RmmChipMuse::setPSG(const uint32_t addr, const uint32_t data)
{
#ifdef __WIRING_PI_H__
	setRegAddress(addr);
	digitalWrite(GPIO_CSWR_YMZ294, 0);
	std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
	digitalWrite(GPIO_CSWR_YMZ294, 1);
	std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS

	setRegData(data);
	digitalWrite(GPIO_CSWR_YMZ294, 0);
	std::this_thread::sleep_for(std::chrono::nanoseconds(30));// 30ns < wait for WR,CS
	digitalWrite(GPIO_CSWR_YMZ294, 1);
#endif
	return;
}

void  RmmChipMuse::setOPLL(const uint32_t addr, const uint32_t data)
{
#ifdef __WIRING_PI_H__
	setRegAddress(addr);
	digitalWrite(GPIO_CSWR_YM2413, 0);
	std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
	digitalWrite(GPIO_CSWR_YM2413, 1);
	usleep(OPLLwait12);	// 12us < wait for ADDR WR

	setRegData(data);
	digitalWrite(GPIO_CSWR_YM2413, 0);
	std::this_thread::sleep_for(std::chrono::nanoseconds(30));	// 30ns < wait for WR,CS
	digitalWrite(GPIO_CSWR_YM2413, 1);
	usleep(OPLLwait84);	// 84us < wait for DATA WR
#endif
	return;
}

void  RmmChipMuse::setSCC(const uint32_t addr, const uint32_t data)
{
#ifdef __WIRING_PI_H__
	uint32_t ad;
	// バンク4のバンクセレクタへのアクセスは、addr<-0xBFFD に変換する
	//	(RaSCC側は0xBFFDへのアクセスは0xB000へのアクセスと読み替えるようにしている）
	ad = (0xB000 <= addr && addr <= 0xB7FF) ? 0xBFFD : addr;

	uint8_t aex = 0;
	switch(ad & 0xFF00)
	{
		case 0x9000: aex = 0x00;	break;
		case 0x9800: aex = 0x01;	break;
		case 0xB800: aex = 0x02;	break;
		case 0xBF00: aex = 0x03;	break;
	}

	digitalWrite(GPIO_AEX0, (aex>>0) & 0x01);
	digitalWrite(GPIO_AEX1, (aex>>1) & 0x01);

	uint16_t addt = static_cast<uint16_t>(((ad&0xff)<<8)|(data&0xff));
	sendPinDW(addt);

	digitalWrite(GPIO_CSWR_HraSCC, 0);
	std::this_thread::sleep_for(std::chrono::nanoseconds(10));
	digitalWrite(GPIO_CSWR_HraSCC, 1);
#endif
	return;
}

void  RmmChipMuse::setRegAddress(const uint32_t addr)
{
#ifdef __WIRING_PI_H__
	digitalWrite(GPIO_A0, 0);
//	std::this_thread::sleep_for(std::chrono::nanoseconds(5));
	sendPinD(static_cast<uint8_t>(addr));
#endif
	return;
}

void  RmmChipMuse::setRegData(const uint32_t data)
{
#ifdef __WIRING_PI_H__
	digitalWrite(GPIO_A0, 1);
//	std::this_thread::sleep_for(std::chrono::nanoseconds(5));
	sendPinD(static_cast<uint8_t>(data));
#endif
	return;
}

void  RmmChipMuse::setRegAddressEX(const uint32_t addr)
{
#ifdef __WIRING_PI_H__
	uint8_t aex = 0;
	switch(addr & 0xFF00)
	{
		case 0x9000: aex = 0x00;	break;
		case 0x9800: aex = 0x01;	break;
		case 0xB800: aex = 0x02;	break;
		case 0xBF00: aex = 0x03;	break;
	}
	digitalWrite(GPIO_AEX0, (aex>>0) & 0x01);
	digitalWrite(GPIO_AEX1, (aex>>1) & 0x01);
#endif
	return;
}

void  RmmChipMuse::sendPinD(const uint8_t data)
{
#ifdef __WIRING_PI_H__
	const static int PINS[] = {GPIO_D0,GPIO_D1,GPIO_D2,GPIO_D3,GPIO_D4,GPIO_D5,GPIO_D6,GPIO_D7};
	const static int LEN = sizeof(PINS)/sizeof(PINS[0]);
	for( int t = 0; t < LEN; ++t){
		digitalWrite(PINS[t], (data>>t) & 0x01);
	}
#endif
	return;
}

void  RmmChipMuse::sendPinDW(const uint16_t data)
{
#ifdef __WIRING_PI_H__
	const static int PINS[] = {
		GPIO_D0, GPIO_D1, GPIO_D2, GPIO_D3, GPIO_D4, GPIO_D5, GPIO_D6, GPIO_D7,
		GPIO_D8, GPIO_D9, GPIO_D10,GPIO_D11,GPIO_D12,GPIO_D13,GPIO_D14,GPIO_D15,
	};
	const static int LEN = sizeof(PINS)/sizeof(PINS[0]);
	for( int t = 0; t < LEN; ++t){
		digitalWrite(PINS[t], (data>>t) & 0x01);
	}
#endif
	return;
}


void  RmmChipMuse::resetDevice()
{
#ifdef __WIRING_PI_H__
	// CHIP DEVICE RESET
	digitalWrite(GPIO_RESET, 0);
	usleep(1*1000);
	digitalWrite(GPIO_RESET, 1);
#endif
	return;
}

void RmmChipMuse::initRegs(const bool bWithoutSCC)
{
	switch(m_TergetChip) {
	case OPLL:
	{
		const int numOpll = sizeof(g_YM2413_INIT_REGISTER) / sizeof(CHIP_REG_VAL);
		for (int t = 0; t < numOpll; ++t) {
			auto &item = g_YM2413_INIT_REGISTER[t];
			setOPLL(item.addr, item.val);
		}
		break;
	}
	case SCC:
	{
		if( bWithoutSCC )
			break;
		const int numScc = sizeof(g_HraSCC_INIT_REGISTER) / sizeof(CHIP_REG_VAL);
		for (int t = 0; t < numScc; ++t) {
			auto &item = g_HraSCC_INIT_REGISTER[t];
			setSCC(item.addr, item.val);
		}
		break;
	}
	case PSG:
	default:
		// do nothing
		break;
	}
	return;
}

void RmmChipMuse::initClockfoHraSCC()
{
#ifdef __WIRING_PI_H__

// クロックジェネレーターSi5351Aの出力周波数 fout を設定する方法。
// ●まずは原理を理解する。
// Si5351Aは、水晶振動子とPLL回路と分周器から構成される。各回路のつながりは下記の通り
//		[水晶振動子] --> [PLL回路] ---> [分周器] ---> 出力信号　　 ･･･(a)
// 水晶振動子は決められた周波数の出力信号を出力する素子である。
// PLL回路は、入力信号の周波数を、 n倍にして出力する回路である。
// 分周器は、入力信号の周波数を、1/m倍にして出力する回路である。
// PLL回路と分周器は、与えるパラメータによって、n、m を調整できる回路である。
// よって、水晶振動子の周波数を、PLL回路、分周器を使って、n倍したり、1/m倍したりして目的の
// 周波数の信号を得るようにする。そのためのn、mを決定する。
//		(水晶振動子 * ｎ) / ｍ => 出力周波数
//
// ◆PLL回路の n は、Pa、Pb、Pcの３つのパラメータで決定する。
// この３つの値を決定してI2C通信でSi5351Aに渡してあげればPLL回路の動作が決定する。
// PLL回路の入力周波数xtalと、出力周波数fvcoとの関係は、
//	fvco = xtal × (Pa + (Pb / Pc))  ･･･(b)
//  ｎ = (Pa + (Pb / Pc)             ･･･(c)
// ※Pa、Pb、Pcは、採りえることができる値の範囲が決められていることに注意する
//		15 <= Pa <= 90
//		 0 <= Pb <= 1048575
//		 1 <= Pc <= 1048575
// ◆分周器の m は、Da、Db、Dcの３つのパラメータで決定する。
// PLL同様、この３つの値を決定してI2C通信でSi5351Aに渡してあげれば分周器の動作が決定する。
// 分周器の入力周波数fvcoと、出力周波数foutとの関係は、
//	fout = fvco ÷ (Da + (Db / Dc))  ･･･(d)
//  ｎ = (Da + (Db / Dc)             ･･･(e)
// ※ｎは、6 <= ｎ <= 1800 の範囲であることが決められていることに注意する
// 
// ◆つまり、出力信号 fout を出力するようにSi5351Aをセッティングするには、
// Pa、Pb、Pc、Da、Db、Dcの６個のパラメータを決定する必要がある。
//
// ●求めたい周波数foutが、水晶振動子の周波数xtal より小さい場合、、、
//			 PLL回路の出力周波数fvcoを決め打ちし、分周器のmをfoutから算出する、を試みる。
// fout=21477270[Hz]、xtal=25[MHz]。この時、適当にfvco=400[MHz]と決める。
// PLL回路のパラメータを求める
// 	fvco = xtal × ｎ
//  ｎ = fvco / xtal
//	ｎ = (Pa + (Pb / Pc))なので、fvco / xtalの整数部をPa、少数部を(Pb / Pc)としてｎを３つのパラメータに分解する。
// Pa = (fvco / xtal)の整数部 = Fix(400000000 / 25000000) = 16  ※Paはこれで決定
// (Pb / Pc) = (fvco / xtal)の小数部 = (fvco%xtal)/xtal = (400000000 % 25000000) / 25000000 = 0 / 21477270 = 0
// (Pb / Pc) = 0になってしまうので、Pa = 16から15にして、(Pb / Pc) = 1にする。
// ゆえに、
// Pa = 15, Pb = 1, Pc = 1
// 分周器のパラメータを求める
// 	fout = fvco / ｍ
//  ｍ = fvco / fout;
//	ｍ = (Da + (Db / Dc))なので、fvco / foutの整数部をDa、少数部を(Db / Dc)としてｍを３つのパラメータに分解する。
// Da = (fvco / fout)の整数部 = Fix(400000000/21477270) = 18  ※Daはこれで決定
// (Db / Dc) = (fvco / fout)の小数部 = (fvco%fout)/fout = (400000000%21477270) / 21477270 = 13409140 / 21477270 = 約0.624
// ただ(fvco / fout)の小数部の小数部が解っても、Db、Dcのそれぞれがわからなければ意味がない。
// ここで、Dcの値を、PLLの場合と同じように1048575に決め打ちする。
// Db = (fvco % fout) / fout * Dc
//    = (400000000%21477270) / 21477270 * 1048575 = 654668(小数点以下切り捨て）
// ゆえに、
// Da = 18, Db = 654668, Dc = 1048575
//
// この値を、I2Cを使って、Si5351Aに伝える。の前に、Pa、Pb、Pc、Da、Db、Dcを、Si5351Aのレジスタ形式に変換する。
// CLK0を使用することとする。
// MSNA_P1[17:0] = Pa * 128 + Floor( ( Pb / Pc ) * 128 ) - 0x200
// MSNA_P2[19:0] = Pb * 128 – Pc * Floor( ( Pb / Pc ) * 128 )
// MSNA_P3[19:0] = Pc
// MS0_P1[17:0] = Da * 128 + Floor( ( Db / Dc ) * 128 ) - 0x200
// MS0_P2[19:0] = Db * 128 – Pc * Floor( ( Db / Dc ) * 128 )
// MS0_P3[19:0] = Dc

 // PLLA_SRC = 0	// PLL-A に水晶振動子を選択
 // XTAL_CL = 0x2	// 8pFを選択（理由は知らない勉強してない）、
	
	static const uint8_t CLKGEN_ADDR = 0x60;	// 0b1100 000
	const int fd = wiringPiI2CSetup(CLKGEN_ADDR);
	if (fd == -1) {
		fprintf(stderr, "Failed to initialize I2C. errno = %d\n", errno);
		return;
	}

	uint32_t FOUT = 21477270;		// 21.47727[MHz]　出力したい周波数
	uint32_t XTAL = 25000000;		// 25[MHz] 水晶発振子の周波数
	uint32_t FVCO = 900000000;		// 400[MHz] PLL回路の出力周波数（決め打ち値）

	uint32_t Pc = 1048575;			// 決め打ち
	uint32_t Pb = static_cast<uint32_t>(((FVCO % XTAL) / static_cast<float>(XTAL)) * Pc);
	uint32_t Pa = FVCO / XTAL;
	uint32_t MSNA_P1 = Pa * 128 + ((Pb / Pc) * 128) - 0x200;
	uint32_t MSNA_P2 = Pb * 128 - Pc * ((Pb / Pc) * 128);
	uint32_t MSNA_P3 = Pc;
	
	uint32_t Dc = 1048575;			// 決め打ち
	uint32_t Db = static_cast<uint32_t>(((FVCO % FOUT) / static_cast<float>(FOUT)) * Dc);
	uint32_t Da = FVCO / FOUT;
	uint32_t MS0_P1 = Da * 128 + ((Db / Dc) * 128) - 0x200;
	uint32_t MS0_P2 = Db * 128 - Pc * (( Db / Dc) * 128);
	uint32_t MS0_P3 = Dc;

//	t_OC(FOREGROUND_GREEN | FOREGROUND_RED, _T("MSNA_P1=%d, P2=%d, P3=%d\n"), MSNA_P1, MSNA_P2, MSNA_P3);
//	t_OC(FOREGROUND_GREEN | FOREGROUND_RED, _T("MS0_P1=%d, P2=%d, P3=%d\n"), MS0_P1, MS0_P2, MS0_P3);

	uint32_t WAITV = 500;

	// (24). CLK3–0 Disable State(LOW)
	wiringPiI2CWriteReg8(fd, 24, 0x00);
	usleep(WAITV);
	// (25). CLK7–4 Disable State(LOW)
	wiringPiI2CWriteReg8(fd, 25, 0x00);
	usleep(WAITV);
	// (3). まず全CLKをdisable
	wiringPiI2CWriteReg8(fd, 3, 0xff);
	usleep(WAITV);


	// (2). interrupt mask
	wiringPiI2CWriteReg8(fd, 2, 0x00);
	usleep(WAITV);

	// PLLA_SRC = 0
	wiringPiI2CWriteReg8(fd, 15, 0x00);
	usleep(WAITV);
	// Power Down
	for( int t = 0; t < 8; ++t){
		wiringPiI2CWriteReg8(fd, 16+t, 0x80);
		usleep(WAITV);
	}

	// XTAL_CL = 0x2	// 8pFを選択
	wiringPiI2CWriteReg8(fd, 183, 0x92);		// xx010010b, xx = 10=8pF -> 10010010b
	usleep(WAITV);

	// MSNA_P1-3
	wiringPiI2CWriteReg8(fd, 26, static_cast<uint8_t>(MSNA_P3>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 37, static_cast<uint8_t>(MSNA_P3>>0) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 28, static_cast<uint8_t>(MSNA_P1>>16) & 0x03);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 29, static_cast<uint8_t>(MSNA_P1>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 30, static_cast<uint8_t>(MSNA_P1>>0) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 31,
		(static_cast<uint8_t>(MSNA_P3>>(16-4)) & 0xf0) |
		(static_cast<uint8_t>(MSNA_P2>>(16+0)) & 0x0f) );
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 32, static_cast<uint8_t>(MSNA_P2>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 33, static_cast<uint8_t>(MSNA_P2>>0) & 0xff);
	usleep(WAITV);

	// MS0_P1-3
	wiringPiI2CWriteReg8(fd, 42, static_cast<uint8_t>(MS0_P3>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 43, static_cast<uint8_t>(MS0_P3>>0) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 44, 
		(static_cast<uint8_t>(MS0_P1>>16) & 0x03) | 
		0x00 ); // R0_DIV = 1
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 45, static_cast<uint8_t>(MS0_P1>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 46, static_cast<uint8_t>(MS0_P1>>0) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 47,
		(static_cast<uint8_t>(MS0_P3>>(16-4)) & 0xf0) | 
		(static_cast<uint8_t>(MS0_P2>>(16+0)) & 0x0f) );
	wiringPiI2CWriteReg8(fd, 48, static_cast<uint8_t>(MS0_P2>>8) & 0xff);
	usleep(WAITV);
	wiringPiI2CWriteReg8(fd, 49, static_cast<uint8_t>(MS0_P2>>0) & 0xff);
	usleep(WAITV);

	// (177)Reset PLL
	wiringPiI2CWriteReg8(fd, 177, 0xa0);
	usleep(WAITV);

	// (16)CLK0 Control
	// 	MS0 = Integer mode.
	//	MS0の入力は、PLLA
	//	CLK0の入力は、MS0
	//	CLK0出力、8mA
	wiringPiI2CWriteReg8(fd, 16, 0x4f);
	usleep(WAITV);

	// (3)CLK0のみの出力をEnbale
	wiringPiI2CWriteReg8(fd, 3, 0xfe);
	usleep(WAITV);

	// 終了
  	close(fd);

#endif /*__WIRING_PI_H__*/
	return;
}

void RmmChipMuse::initGpio()
{
#ifdef __WIRING_PI_H__
	// wiringPi 初期化
	wiringPiSetup();
#endif
	return;
}

void RmmChipMuse::setupGpio()
{
#ifdef __WIRING_PI_H__
	// GPIOピンのモード設定
	pinMode(GPIO_D0, OUTPUT);
	pinMode(GPIO_D1, OUTPUT);
	pinMode(GPIO_D2, OUTPUT);
	pinMode(GPIO_D3, OUTPUT);
	pinMode(GPIO_D4, OUTPUT);
	pinMode(GPIO_D5, OUTPUT);
	pinMode(GPIO_D6, OUTPUT);
	pinMode(GPIO_D7, OUTPUT);
	pinMode(GPIO_D8, OUTPUT);
	pinMode(GPIO_D9, OUTPUT);
	pinMode(GPIO_D10, OUTPUT);
	pinMode(GPIO_D11, OUTPUT);
	pinMode(GPIO_D12, OUTPUT);
	pinMode(GPIO_D13, OUTPUT);
	pinMode(GPIO_D14, OUTPUT);
	pinMode(GPIO_D15, OUTPUT);

	pinMode(GPIO_A0, OUTPUT);
	pinMode(GPIO_RESET, OUTPUT);
	pinMode(GPIO_AEX1, OUTPUT);
	pinMode(GPIO_AEX0, OUTPUT);
	pinMode(GPIO_CSWR_YMZ294, OUTPUT);
	pinMode(GPIO_CSWR_YM2413, OUTPUT);
	pinMode(GPIO_CSWR_HraSCC, OUTPUT);

	digitalWrite(GPIO_RESET, 1);
	digitalWrite(GPIO_A0, 0);
	digitalWrite(GPIO_CSWR_YMZ294, 1);
	digitalWrite(GPIO_CSWR_YM2413, 1);
	digitalWrite(GPIO_CSWR_HraSCC, 1);
#endif
	return;
}

