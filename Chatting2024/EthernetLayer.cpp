// EthernetLayer.cpp: implementation of the CEthernetLayer class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "pch.h"
#include "EthernetLayer.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CEthernetLayer::CEthernetLayer(char* pName)
	: CBaseLayer(pName)
{
	ResetHeader();
}

CEthernetLayer::~CEthernetLayer()
{
}

void CEthernetLayer::ResetHeader()
{
	memset(m_sHeader.enet_dstaddr.addrs, 0, 6);
	memset(m_sHeader.enet_srcaddr.addrs, 0, 6);
	memset(m_sHeader.enet_data, 0, ETHER_MAX_DATA_SIZE);
	m_sHeader.enet_type = 0x3412; // 0x0800
}

void CEthernetLayer::SetEnetSrcAddress(unsigned char* pAddress)
{
	
	memcpy(m_sHeader.enet_srcaddr.addrs, pAddress, 6);
}

void CEthernetLayer::SetEnetDstAddress(unsigned char* pAddress)
{
	memcpy(m_sHeader.enet_dstaddr.addrs, pAddress, 6);
}

void CEthernetLayer::SetFrameType(unsigned short type) {
	m_sHeader.enet_type = type;
}

unsigned char* CEthernetLayer::GetEnetDstAddress() 
{
	return m_sHeader.enet_srcaddr.addrs;
}

unsigned char* CEthernetLayer::GetEnetSrcAddress() 
{
	return m_sHeader.enet_dstaddr.addrs;
}

BOOL CEthernetLayer::Send(unsigned char* ppayload, int nlength)
{
	
	memcpy(m_sHeader.enet_data, ppayload, nlength);

	BOOL bSuccess = FALSE;
	bSuccess = mp_UnderLayer->Send((unsigned char*)&m_sHeader, nlength + ETHER_HEADER_SIZE);


	return bSuccess;
}

BOOL CEthernetLayer::Receive(unsigned char* ppayload)
{
	// 하위 계층에서 받은 payload를 현재 계층의 header구조에 맞게 읽음.
	PETHERNET_HEADER pFrame = (PETHERNET_HEADER)ppayload;


	// 브로드캐스트 MAC 주소 ff:ff:ff:ff:ff:ff
	unsigned char broadcastAddr[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	BOOL bSuccess = FALSE;

	// 목적지 주소가 자신인 경우나
	if ((!memcmp((char*)pFrame->enet_dstaddr.addrs, (char*)m_sHeader.enet_srcaddr.addrs, 6) ||
		// 목적지 주소가 브로드 캐스트
		!memcmp(pFrame->enet_dstaddr.addrs, broadcastAddr, 6)) &&
		// 출발지 주소가 자신의 주소가 아닌지 확인
		memcmp((char*)pFrame->enet_srcaddr.addrs, (char*)m_sHeader.enet_srcaddr.addrs, 6))
	{
		if (ntohs(pFrame->enet_type) == 0x8020) { // Chat App Data 받음
			bSuccess = mp_aUpperLayer[0]->Receive((unsigned char*)pFrame->enet_data);
		}
	}


	return bSuccess;
}