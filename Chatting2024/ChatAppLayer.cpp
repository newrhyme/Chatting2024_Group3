#define _CRT_SECURE_NO_WARNINGS
// ChatAppLayer.cpp: implementation of the CChatAppLayer class.
//
//////////////////////////////////////////////////////////////////////


#include "stdafx.h"
#include "pch.h"
#include "ChatAppLayer.h"
#include "EthernetLayer.h"
#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CChatAppLayer::CChatAppLayer(char* pName)
	: CBaseLayer(pName),
	mp_Dlg(NULL)
{
	ResetHeader();
}

CChatAppLayer::~CChatAppLayer()
{
}

void CChatAppLayer::ResetHeader() // ChatApp 헤더 초기화
{
	m_sHeader.capp_totlen = 0x0000;
	m_sHeader.capp_type = 0x00;

	memset(m_sHeader.capp_data, 0, APP_DATA_SIZE);
}

BOOL CChatAppLayer::Send(unsigned char* ppayload, int nlength)
{
	// ppayload : 입력한 문자열 , nlength : 문자의 길이
	m_ppayload = ppayload;
	m_length = nlength;

	if (nlength <= APP_DATA_SIZE) {
		((CEthernetLayer*)GetUnderLayer())->SetFrameType(0x2080);
		m_sHeader.capp_totlen = nlength;
		m_sHeader.capp_type = DATA_TYPE_END;
		memcpy(m_sHeader.capp_data, ppayload, nlength); // GetBuff에 data 복사
		mp_UnderLayer->Send((unsigned char*)&m_sHeader, nlength + APP_HEADER_SIZE);
	}
	else {
		int sentData = 0;
		int remainLength = nlength;
		unsigned char* pppayload = ppayload;

		// 처음
		m_sHeader.capp_totlen = nlength;
		m_sHeader.capp_type = DATA_TYPE_BEGIN;
		memcpy(m_sHeader.capp_data, pppayload, APP_DATA_SIZE);
		((CEthernetLayer*)GetUnderLayer())->SetFrameType(0x2080);
		mp_UnderLayer->Send((unsigned char*)&m_sHeader, (APP_DATA_SIZE + APP_HEADER_SIZE));
		sentData += APP_DATA_SIZE;
		pppayload += APP_DATA_SIZE;
		remainLength -= APP_DATA_SIZE;

		// 중간
		while (remainLength > APP_DATA_SIZE) {
			m_sHeader.capp_type = DATA_TYPE_CONT;
			memcpy(m_sHeader.capp_data, pppayload, APP_DATA_SIZE);
			((CEthernetLayer*)GetUnderLayer())->SetFrameType(0x2080);
			mp_UnderLayer->Send((unsigned char*)&m_sHeader, APP_DATA_SIZE + APP_HEADER_SIZE);
			sentData += APP_DATA_SIZE;
			pppayload += APP_DATA_SIZE;
			remainLength -= APP_DATA_SIZE;
		}

		if (remainLength > 0) {
			m_sHeader.capp_type = DATA_TYPE_END;
			memcpy(m_sHeader.capp_data, pppayload, remainLength);
			((CEthernetLayer*)GetUnderLayer())->SetFrameType(0x2080);
			mp_UnderLayer->Send((unsigned char*)&m_sHeader, remainLength + APP_HEADER_SIZE);
		}
	}
	return TRUE;

}

BOOL CChatAppLayer::Receive(unsigned char* ppayload)
{
	// ppayload를 ChatApp 헤더 구조체로 넣는다.
	PCHAT_APP_HEADER capp_hdr = (PCHAT_APP_HEADER)ppayload;
	static unsigned char* GetBuff; // 데이터를 쌓을 GetBuff를 선언한다.

	if (capp_hdr->capp_totlen <= APP_DATA_SIZE) {
		GetBuff = (unsigned char*)malloc(capp_hdr->capp_totlen + 1);
		memset(GetBuff, 0, capp_hdr->capp_totlen + 1);
		memcpy(GetBuff, capp_hdr->capp_data, capp_hdr->capp_totlen);
		GetBuff[capp_hdr->capp_totlen] = '\0';

		mp_aUpperLayer[0]->Receive((unsigned char*)GetBuff); // 상위 계층으로 데이터 올림
		free(GetBuff);
		return TRUE;
	}

	// 밑 계층에서 넘겨받은 ppayload를 분석하여 ChatDlg 계층으로 넘겨준다.
	if (capp_hdr->capp_type == DATA_TYPE_BEGIN) // 데이터 첫 부분
	{
		// 첫 부분 일 경우 그 크기만큼 버퍼 할당		
		GetBuff = (unsigned char*)malloc(capp_hdr->capp_totlen + 1);
		memset(GetBuff, 0, capp_hdr->capp_totlen + 1);  // GetBuff를 초기화해준다.
		memcpy(GetBuff, capp_hdr->capp_data, APP_DATA_SIZE);
	}
	else if (capp_hdr->capp_type == DATA_TYPE_CONT) // 데이터 중간 부분
	{
		// 계속 버퍼에 쌓는다.
		strncat((char*)GetBuff, (char*)capp_hdr->capp_data, APP_DATA_SIZE);
	}
	else if (capp_hdr->capp_type == DATA_TYPE_END) // 데이터 끝 부분
	{
		// 남은 데이터를 버퍼에 추가
		strncat((char*)GetBuff, (char*)capp_hdr->capp_data, capp_hdr->capp_totlen % APP_DATA_SIZE);

		// 위에서 만들어진 메시지 포맷을 ChatDlg로 넘겨준다.
		mp_aUpperLayer[0]->Receive((unsigned char*)GetBuff);
		free(GetBuff);
	}
	else
		return FALSE;

	return TRUE;
}

UINT CChatAppLayer::ChatThread(LPVOID pParam)
{
	// Thread 함수가 static으로 사용되기에, 인자로 넘겨받은 pParam에 들어있는 Chatapp의 클래스를 이용하여 간접 접근한다.
	BOOL bSuccess = FALSE;
	CChatAppLayer* pChat = (CChatAppLayer*)pParam;
	((CEthernetLayer*)(pChat->GetUnderLayer()))->SetFrameType(0x2080);
	int data_length = APP_DATA_SIZE; // 보낼 data의 길이
	int seq_tot_num; // Sequential
	int data_index;	 // data를 나눠 보낼 때, 현재 보낸 data의 길이의 끝 index를 저장
	// 다음 data를 보낼 때의 첫 index가 된다.
	int temp = 0;

	// sequential number의 총 개수를 정함
	if (pChat->m_length < APP_DATA_SIZE) // APP_DATA_SIZE 보다 작으면 1. (한 번만 전송하면 된다.)
		seq_tot_num = 1;
	else // 그렇지 않으면, 데이터 길이로 APP_DATA_SIZE를 나눈 몫에 1을 더한 값으로 seq_tot_num을 결정한다.
		seq_tot_num = (pChat->m_length / APP_DATA_SIZE) + 1;

	for (int i = 0; i <= seq_tot_num + 1; i++)
	{
		// 보낼 data의 길이를 결정
		if (seq_tot_num == 1) { // 보낼 횟수가 한 번이면, 데이터 길이 만큼 보내면 된다.
			data_length = pChat->m_length;
		}
		else { // 보낼 횟수가 두 번 이상이고,
			if (i == seq_tot_num) // 보낼 횟수의 가장 마지막일 때는, 남은 데이터의 길이만큼 보낸다.
				data_length = pChat->m_length % APP_DATA_SIZE;
			else // 처음, 중간 데이터 일 때, APP_DATA_SIZE만큼 보낸다.
				data_length = APP_DATA_SIZE;
		}

		memset(pChat->m_sHeader.capp_data, 0, data_length);

		if (i == 0) // 처음부분 : 타입은 0x00, 데이터의 총 길이를 전송한다.
		{
			pChat->m_sHeader.capp_totlen = pChat->m_length;
			pChat->m_sHeader.capp_type = DATA_TYPE_BEGIN;
			data_length = 0;
		}
		else if (i != 0 && i <= seq_tot_num) // 중간 부분 : 타입은 0x01, seq_num는 순서대로,
		{
			data_index = data_length * 2;
			pChat->m_sHeader.capp_type = DATA_TYPE_CONT;
			pChat->m_sHeader.capp_seq_num = i - 1;

			memcpy(pChat->m_sHeader.capp_data, pChat->m_ppayload + temp, data_length);
			temp += data_length;
		}
		else // 마지막 부분 : 타입은 0x02
		{
			pChat->m_sHeader.capp_type = DATA_TYPE_END;
			data_length = 0;
		}
		bSuccess = pChat->mp_UnderLayer->Send((unsigned char*)&pChat->m_sHeader, data_length + APP_HEADER_SIZE);
	}

	return bSuccess;
}