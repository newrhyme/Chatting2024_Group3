#include "stdafx.h"
#include "pch.h"
#include "NILayer.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////


// name, adapterInstance, adapterIndex를 받아 초기화
// 어댑터 인덱스를 저장하고, 스레드 스위치를 켜고, 어댑터 객체는 NULL로 설정
CNILayer::CNILayer(char* name, LPADAPTER* adaptInst, int adaptIndex)
    : CBaseLayer(name), m_iNumAdapter(adaptIndex), m_thrdSwitch(TRUE), m_AdapterObject(NULL)
{
    InitAdaptList(NULL); // 네트워크 어댑터(NIC) 리스트 초기화
}

CNILayer::~CNILayer()
{
}

void CNILayer::PacketStartDriver() // 패킷 수신 준비
{
    if (m_iNumAdapter < 0 || m_iNumAdapter >= NI_COUNT_NIC) { // 어댑터가 선택되지 않았거나 유효하지 않으면
        AfxMessageBox(_T("유효하지 않은 NIC"));
        return;
    }

    char errBuf[PCAP_ERRBUF_SIZE]; // 오류 메시지를 저장할 버퍼

    // 선택된 네트워크 어댑터를 열고 패킷을 수신하기 위한 설정
    m_AdapterObject = pcap_open_live(
        m_pAdapterList[m_iNumAdapter]->name,     // 어댑터 이름
        BUFSIZ,                                  // 최대 패킷 크기
        PCAP_OPENFLAG_PROMISCUOUS,               // 프로미스큐어스 모드 활성화
        2000,                                    // 타임아웃(밀리초)
        errBuf                                   // 오류 메시지를 저장할 버퍼
    );

    // 어댑터 열기에 실패하면 오류 메시지 표시
    if (!m_AdapterObject) {
        AfxMessageBox(errBuf);
        return;
    }

    m_thrdSwitch = TRUE; // 스레드 스위치를 활성화

    AfxBeginThread(PacketReceiverThread, this); // 패킷 수신 스레드 실행
}

// 어댑터 리스트에서 전달된 값에 해당하는 네트워크 어댑터 반환
pcap_if_t* CNILayer::GetAdapterObject(int index)
{
    return m_pAdapterList[index]; 
}

// 어느 어댑터를 사용할 것인지 결정
void CNILayer::SetAdapterNumber(int num)
{
    m_iNumAdapter = num; 
}

// 네트워크 어댑터 리스트 초기화
void CNILayer::InitAdaptList(LPADAPTER* plist) 
{
    pcap_if_t* allDevices;                              // 네트워크 장치 목록 저장
    char errorBuffer[PCAP_ERRBUF_SIZE];                 // 오류 저장 버퍼

    memset(m_pAdapterList, 0, sizeof(m_pAdapterList));  // 어댑터 리스트 초기화

    // pcap_findalldevs를 호출해 사용할 수 있는 네트워크 장치 검색. 실패하면 오류
    if (pcap_findalldevs(&allDevices, errorBuffer) == -1) {
        AfxMessageBox(_T("네트워크 장치를 찾을 수 없습니다."));
        return;
    }
    // 검색된 장치가 없으면 오류
    if (!allDevices) {
        AfxMessageBox(_T("사용 가능한 네트워크 장치가 없습니다."));
        return;
    }

    // 모든 네트워크 장치를 순회하면서 m_pAdapterList에 저장
    int deviceCount = 0;
    for (pcap_if_t* currentDevice = allDevices; currentDevice != NULL && deviceCount < NI_COUNT_NIC; currentDevice = currentDevice->next) {
        m_pAdapterList[deviceCount++] = currentDevice;
    }
}

BOOL CNILayer::Send(unsigned char* ppayload, int nlength) 
{
    if (nlength <= 0) {
        AfxMessageBox(_T("전송할 데이터가 없습니다."));
        return FALSE;
    }

    // pcap_sendpacket 함수를 사용하여 네트워크 어댑터를 통해 데이터 전송
    if (pcap_sendpacket(m_AdapterObject, ppayload, nlength) != 0) {
        AfxMessageBox(_T("패킷 전송 실패"));
        return FALSE;
    }
    return TRUE; // 데이터 전송 결과
}

BOOL CNILayer::Receive(unsigned char* ppayload)
{
    if (ppayload == nullptr) {
        AfxMessageBox(_T("수신할 데이터가 유효하지 않습니다."));
        return FALSE;
    }
    BOOL success = FALSE;
    success = mp_aUpperLayer[0]->Receive(ppayload); // 수신된 ppayload를 상위 계층으로 전달
    return success; // 패킷 전달 결과
}

// Thread가 실행되는 동안 패킷 수신
UINT CNILayer::PacketReceiverThread(LPVOID param) 
{
    CNILayer* layer = static_cast<CNILayer*>(param); // 형변환
    struct pcap_pkthdr* header;                      // 패킷 헤더
    const u_char* data;                              // 패킷 데이터

    // pcap_next_ex를 통해 스레드가 실행되는 동안 계속 패킷 수신
    while (layer->m_thrdSwitch) { 
        if (pcap_next_ex(layer->m_AdapterObject, &header, &data) == 1)
            layer->Receive((u_char*)data); // Receive 함수로 전달하여 패킷 수신 처리
    }
    return 0;
}

// 네트워크 어댑터의 MAC 주소를 가져오기
CString CNILayer::GetNICardAddress(char* adaptName) 
{
    CString Mac; // MAC 주소
    PPACKET_OID_DATA pOidData = (PPACKET_OID_DATA)malloc(sizeof(PACKET_OID_DATA) + 6); // MAC 주소 저장할 6바이트 할당

    // 메모리 할당되면
    if (pOidData) { 
        pOidData->Oid = OID_802_3_CURRENT_ADDRESS;     // MAC 주소 가져올 OID
        pOidData->Length = 6;                          // MAC 주소는 6바이트
        ZeroMemory(pOidData->Data, 6);                 // MAC 주소 저장할 데이터 초기화

        LPADAPTER pAdapter = PacketOpenAdapter(adaptName); // 네트워크 어댑터 객체 생성
        if (PacketRequest(pAdapter, FALSE, pOidData)) {    // OID 보내 MAC 주소 가져오기
            Mac.Format("%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                pOidData->Data[0], pOidData->Data[1],
                pOidData->Data[2], pOidData->Data[3],
                pOidData->Data[4], pOidData->Data[5]);
        }
        PacketCloseAdapter(pAdapter); // 어댑터 닫고 리소스 해제
        free(pOidData); // MAC 주소 메모리 해제
    }
    else // 메모리 할당 실패
        return "None";

    return Mac; // MAC 주소 반환
}