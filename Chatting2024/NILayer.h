#if !defined(AFX_NILAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_)
#define AFX_NILAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "BaseLayer.h"
#include <pcap.h>
#include <Packet32.h>
#include <iphlpapi.h>
#pragma comment (lib, "iphlpapi.lib")

class CNILayer : public CBaseLayer
{
protected:
    pcap_t* m_AdapterObject;  // Selected adapter instance
    pcap_if_t* m_pAdapterList[NI_COUNT_NIC];  // Adapter list
    int m_iNumAdapter;  // Selected adapter index

public:
    BOOL m_thrdSwitch;  // Thread activation status
    unsigned char* m_PayloadBuffer;  // Data buffer

    CNILayer(char* name, LPADAPTER* adaptInst = NULL, int adaptIndex = 0);  // Constructor
    virtual ~CNILayer();  // Destructor

    void PacketStartDriver();  // Prepare for packet reception
    BOOL Send(unsigned char* ppayload, int nlength);  // Packet transmission
    BOOL Receive(unsigned char* ppayload);  // Packet reception

    pcap_if_t* GetAdapterObject(int index);  // Return adapter
    void SetAdapterNumber(int num);  // Set adapter number
    void InitAdaptList(LPADAPTER* plist);  // Initialize adapter list

    static UINT PacketReceiverThread(LPVOID param);  // Packet reception thread
    static UINT ChattingThread(LPVOID param);  // Chatting thread (empty)

    CString GetNICardAddress(char* adaptName);  // Get MAC address of the network card
};

#endif // !defined(AFX_NILAYER_H__7857C9C2_B459_4DC8_B9B3_4E6C8B587B29__INCLUDED_)
