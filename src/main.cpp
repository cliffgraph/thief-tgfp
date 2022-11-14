
#include "pch.h"
#include "tools.h"
#include "constools.h"
#include "tgf.h"
#include "tgp.h"
#include "msxdef.h"
#include <memory.h>
#include "RmmChipMuse.h"
#include "CUTimeCount.h"
#include "CUdpSocket.h"
#include <queue>

inline uint32_t MAKE_U32(const uint16_t H, const uint16_t L)
{
	return static_cast<uint32_t>((H<<16)|L);
}

struct OPTS{
	tstring PlayFilePath;
	bool bHelp;
	bool bMute;
	bool bWithoutInitializeSCC;
	bool bRepeat;
	int	UdpPrortNo;
	OPTS() : bHelp(false), bMute(false), bWithoutInitializeSCC(false), bRepeat(false), UdpPrortNo(0) {}
};

static void printHelp()
{
	t_OC(
		_T(" -m            : mute\n")
		_T(" -f <file path>: TGF file path\n")
		_T(" -r            : Repeat play music\n")
		_T(" -ws           : Without initialize SCC\n")
		_T(" -h            : Help message\n")
		_T(" -udp <n>      : UDP port number\n")
	);
	return;
}

static bool options(int argc, char *argv[], OPTS *pOpts)
{
	if( argc <= 1 )
		return false;
	for( int t = 1; t < argc; ++t){
		tstring s;
		t_ToWiden(std::string(argv[t]), &s);

		if( s == _T("-f") ){
			if( argc <= (t+1) )
				return false;
			t_ToWiden(std::string(argv[++t]), &pOpts->PlayFilePath);
		}
		else if( s == _T("-h") ){
			pOpts->bHelp = true;
		}
		else if( s == _T("-m") ){
			pOpts->bMute = true;
		}
		else if( s == _T("-ws") ){
			pOpts->bWithoutInitializeSCC = true;
		}
		else if( s == _T("-r") ){
			pOpts->bRepeat = true;
		}
		else if( s == _T("-udp") ){
			if( argc <= (t+1) )
				return false;
			pOpts->UdpPrortNo = std::atoi(argv[++t]);
			if( pOpts->UdpPrortNo < 1 || 65535 < pOpts->UdpPrortNo )
				return false;
		}
		else {
			t_OC( _T("found unknown option \"%ls\"\n"), s.c_str() );
			return false;
		}
	}
	return true;
}

static void mute(RmmChipMuse *pOpll, RmmChipMuse *pPsg, RmmChipMuse *pScc)
{
	// 音量を0にする
	// それ以外のレジスタはいじらない

	// OPLL
	pOpll->SetRegister(0x30, 0x0F);	// Vol = 0
	pOpll->SetRegister(0x31, 0x0F);
	pOpll->SetRegister(0x32, 0x0F);
	pOpll->SetRegister(0x33, 0x0F);
	pOpll->SetRegister(0x34, 0x0F);
	pOpll->SetRegister(0x35, 0x0F);
	pOpll->SetRegister(0x36, 0x0F);
	pOpll->SetRegister(0x37, 0xFF);
	pOpll->SetRegister(0x38, 0xFF);
	// PSG
	pPsg->SetRegister(0x08, 0x00);
	pPsg->SetRegister(0x09, 0x00);
	pPsg->SetRegister(0x0A, 0x00);
	// SCC
	pScc->SetRegister(0x9000, 0x3f);
	pScc->SetRegister(0x988A, 0);
	pScc->SetRegister(0x988B, 0);
	pScc->SetRegister(0x988C, 0);
	pScc->SetRegister(0x988D, 0);
	pScc->SetRegister(0x988E, 0);

	return;
}

bool playerReceiveData(
	const OPTS &opts, RmmChipMuse &opll, RmmChipMuse &psg, RmmChipMuse &scc, bool *pbRequestStop)
{
	std::unique_ptr<CUdpSocket> pUdp(NNEW CUdpSocket());
	if( !pUdp->Open(opts.UdpPrortNo, false) ){
		return false;
	}else{
		t_OC(_T("UDP port : %d\n"), opts.UdpPrortNo);
	}

	TGPACKET sendPack;
	uint8_t *pRecvBuff;
	size_t recvSize;
	uint32_t srcIp, destIp = 0;
	uint16_t srcPort, destPort = 0;
	uint32_t plaingIndex = 0;

	CUTimeCount mtc;
	TGF_ATOM atom = {TGF_M_NOP};
	timecode_t padding = 0;
	bool bFirst = true;
	bool bPlayng = false;
	bool bReceivedAtoms = false;
	std::queue<TGF_ATOM> *pFifo = NNEW std::queue<TGF_ATOM>();

	while(!*pbRequestStop){
		if( pUdp->GetReceiveDataPtr( &pRecvBuff, &recvSize, &srcIp, &srcPort) ){
			if( sizeof(TGPACKET) <= recvSize){
				auto *p = reinterpret_cast<const TGPACKET*>(pRecvBuff);
				switch(p->cmd)
				{
					case TGPACKET_REQUEST_PLAY:		// TGFE -> TGFP, 再生を要求する	index=開始インデックス
					{
						destIp = srcIp, destPort = srcPort;
						plaingIndex = p->index;
						sendPack.cmd = TGPACKET_REQUEST_ATOMS;
						sendPack.index = plaingIndex;
						pUdp->SetDestinationAddress(destIp, destPort); 
						uint32_t ipaddr;
						int portNo;
						pUdp->GetDestinationAddress(&ipaddr, &portNo);
						pUdp->SendBinary(reinterpret_cast<const uint8_t*>(&sendPack), sizeof(sendPack));
						mtc.ResetBegin();
						atom.mark = TGF_M_NOP;
						bFirst = true;
						bPlayng = true;
						break;
					}
					case TGPACKET_REQUEST_STOP:		// TGFE -> TGFP, 再生停止を要求する
					{
						mute(&opll, &psg, &scc);
						atom.mark = TGF_M_NOP;
						destIp = 0, destPort = 0;
						plaingIndex = 0;
						bFirst = true;
						bPlayng = false;
						while(!pFifo->empty()) pFifo->pop();
						break;
					}
					case TGPACKET_TG_ATOMS:			// TGFE -> TGFP, 再生データ、inde=データのインデックス
					{
						auto *p = reinterpret_cast<const TGPACKET*>(pRecvBuff);
						for( int t = 0; t < static_cast<int>(p->num); ++t){
							pFifo->push(p->atoms[t]);
						}
						bReceivedAtoms = true;
						break;
					}
					case TGPACKET_REQUEST_ATOMS:
					case TGPACKET_NONE:				
					default:
						break;
				}
			}
		}
		if( atom.mark == TGF_M_NOP && !pFifo->empty() ){
			atom = pFifo->front();
			pFifo->pop();
		}
		if( bPlayng && bReceivedAtoms && pFifo->empty() ){
			sendPack.cmd = TGPACKET_REQUEST_ATOMS;
			sendPack.index = ++plaingIndex;
			pUdp->SendBinary(reinterpret_cast<const uint8_t*>(&sendPack), sizeof(sendPack));
			bReceivedAtoms = false;
		}
		switch(atom.mark)
		{
			case TGF_M_OPLL:
			{
				opll.SetRegister(atom.data1, atom.data2);
				atom.mark = TGF_M_NOP;
				break;
			}
			case TGF_M_PSG:		
			{
				psg.SetRegister(atom.data1, atom.data2);	
				atom.mark = TGF_M_NOP;
				break;
			}
			case TGF_M_SCC:
			{
				scc.SetRegister(atom.data1, atom.data2);	
				atom.mark = TGF_M_NOP;
				break;
			}
			case TGF_M_TC:
			{
				const auto base = static_cast<timecode_t>(MAKE_U32(atom.data1,atom.data2));
				if( bFirst ){
					bFirst = false;
					padding = base;
				}
				//t_OC(_T("TC=%d\n") CURSOR_UP, base);						// \rは使用しない
				const auto tc = static_cast<timecode_t>(mtc.GetTime()/16600);	// 16.6ms
				if( (base-padding) <= tc ){
					atom.mark = TGF_M_NOP;
				}
				break;
			}
			case TGF_M_SYSINFO:
			case TGF_M_WAIT:		
			default:
				atom.mark = TGF_M_NOP;
				break;
		}
	}
	NDELETE(pFifo);
	pUdp->Close();
	return true;
}

/**
 * 先頭のTCの値が1になるように、後続のTCの値も修正する
*/
static void addujster(std::vector<uint8_t> *pTgfData)
{
	auto *pAtoms = reinterpret_cast<TGF_ATOM*>(pTgfData->data());
	const int num = static_cast<int>(pTgfData->size()/sizeof(TGF_ATOM));
	bool bFirstFindTc = true;
	uint32_t startTc = 0;
	for(int t = 0; t < num; ++t ){
		auto &dt = pAtoms[t];
		if( dt.mark == TGF_M_TC ){
			uint32_t tc = MAKE_U32(dt.data1,dt.data2);
			if( bFirstFindTc ){
				bFirstFindTc = false;
				dt.data1 = 0;
				dt.data2 = 1;
				startTc = tc;
			}
			else{
				tc = tc - startTc + 1;
				dt.data1 = tc >> 16;
				dt.data2 = tc & 0xffff;
			}
		}
	}
	return;
}

/** 再生データファイル読み込み処理
 * @param filePath 読み込みファイルのパス
 * @return 読み込みデータへのvectorポインタを返す（使用しなくなったらdeleteすること）
*/
static std::vector<uint8_t> *loader(const tstring &filePath)
{
	// ファイル読み込む
	const uint64_t fsize = t_GetFileSize(filePath);
	std::vector<uint8_t> *pTgfData = nullptr;
	if( fsize == 0 || !t_ReadFile(filePath, &pTgfData) ){
		t_OC(_T("  Don't open file %ls\n"), filePath.c_str());
		return nullptr;
	}
	t_OC( _T("file: %ls, %d bytes.\n"), filePath.c_str(), pTgfData->size() );

	// サイズチェック（構造体サイズの倍数ではなかったら処理を終了する）
	if( static_cast<int>(pTgfData->size()%sizeof(TGF_ATOM)) != 0 )
	{
		NDELETE(pTgfData);
		t_OC( _T("  Incorrect file size.\n"), filePath.c_str(), pTgfData->size() );
		return nullptr;
	}

	return pTgfData;
}

/* 最終TCを得る
*/
static timecode_t getLastTimeCode(const TGF_ATOM atoms[], const int num)
{
	timecode_t tc = 0;
	for( int t = 0; t < num; ++t){
		auto &a = atoms[num-t-1];
		if( a.mark == TGF_M_TC ){
			tc = MAKE_U32(a.data1, a.data2);
			break;
		}
	}
	return tc;
}


/** 再生処理
 * @param opts オプション指定への参照
 * @param opll OPLL再生器オブジェクトへの参照
 * @param psg PSG再生器オブジェクトへの参照
 * @param scc SCC再生器オブジェクトへの参照
 * @param pTgfData 再生データへのポインタ（バイトバイナリデータ）
 * @param *pbRequestStop trueの場合再生を停止し処理を終了する（別スレッドで内容が変更される）
 * @return true
*/
static bool player(
	const OPTS &opts, RmmChipMuse &opll, RmmChipMuse &psg, RmmChipMuse &scc,
	const std::vector<uint8_t> *pTgfData, bool *pbRequestStop )
{
	auto *pAtoms = reinterpret_cast<const TGF_ATOM*>(pTgfData->data());
	const int num = static_cast<int>(pTgfData->size()/sizeof(TGF_ATOM));
	const auto lastTc = getLastTimeCode(pAtoms, num);
	CUTimeCount mtc;
	do{
		for( int t = 0; t < num && !*pbRequestStop; ++t){
			auto &dt = pAtoms[t];
			switch(dt.mark)
			{
				case TGF_M_OPLL:
					opll.SetRegister(dt.data1, dt.data2);	
					break;
				case TGF_M_PSG:		
					psg.SetRegister(dt.data1, dt.data2);	
					break;
				case TGF_M_SCC:
					scc.SetRegister(dt.data1, dt.data2);	
					break;
				case TGF_M_TC:
				{
					auto base = MAKE_U32(dt.data1,dt.data2);
					uint32_t tc = 0;
					while( tc < base && !*pbRequestStop){
						tc = static_cast<uint32_t>(mtc.GetTime()/16600);	// 16.6ms
					}
					t_OC(_T("count: %d/%d") CURSOR_DELTAIL _T("\n") CURSOR_UP, base, lastTc);	// \rは使用しない
					break;
				}
				case TGF_M_NOP:
				case TGF_M_SYSINFO:
				case TGF_M_WAIT:		
				default:
					// do nothing
					break;
			}
		}
		mtc.ResetBegin();
	}while(opts.bRepeat && !*pbRequestStop);
	return true;
}

/**
 * 
*/
#include <signal.h>
static bool g_bRequestStop = false;
static void ctrlc_handler(int signo)
{
	if( signo == SIGINT ){
		g_bRequestStop = true;
	}
	return;
}

int main(int argc, char *argv[])
{
	struct sigaction act;
    memset(&act, 0, sizeof act);
    act.sa_handler = ctrlc_handler;
    sigaction(SIGINT, &act, NULL);

	OPTS opts;
	t_OC(_T("\nthief-tgfp version 1.4 by @Harumakkin, 2022\n"));
	const bool bOpt = options(argc, argv, &opts);
	if( !bOpt || opts.bHelp ){
		printHelp();
		return 1;
	}

	// 音源の準備
	auto *pOpll = NNEW RmmChipMuse(RmmChipMuse::OPLL);
	auto *pPsg = NNEW RmmChipMuse(RmmChipMuse::PSG);
	auto *pScc = NNEW RmmChipMuse(RmmChipMuse::SCC);
	pOpll->Init();
	pPsg->Init();
	pScc->Init(opts.bWithoutInitializeSCC);

	t_OC(CURSOR_OFF);

	// 再生
	if( !opts.bMute ){
		if( 0 < opts.UdpPrortNo ){
			playerReceiveData(opts, *pOpll, *pPsg, *pScc, &g_bRequestStop);
		}
		else{
			if( !opts.PlayFilePath.empty() ){
				auto *pTgfData = loader(opts.PlayFilePath);
				if( pTgfData !=  nullptr ){
					addujster(pTgfData);
					player(opts, *pOpll, *pPsg, *pScc, pTgfData, &g_bRequestStop);
					NDELETE(pTgfData);
				}
			}
		}
	}

	// 後片付け
	mute(pOpll, pPsg, pScc);
	NDELETE(pOpll);
	NDELETE(pPsg);
	NDELETE(pScc);
	t_OC(CURSOR_ON _T("\n"));

	return 0;
}

