#pragma once

#include "WP81ProjectionCommon.h"

class CWP81ProjectionClient
{
private:
	LPWSTR lpstrUsbVid = NULL;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	HANDLE hEvent = NULL;
	HANDLE hWrite = NULL;
	WINUSB_INTERFACE_HANDLE hUsb = NULL;
	PVOID pBufOfData = NULL;
	DWORD dwBufferSize = 0;
	BOOL bIoPending = FALSE;
	WP_SCRREN_TO_PC_PIPE usbPipe;
	OVERLAPPED ioOverlapped;
	OVERLAPPED ioWrite;
	CRITICAL_SECTION cs;
	DWORD dwPhoneWidth = 0,dwPhoneHeight = 0,dwPhoneStride = 0;
	DWORD send[48];
	DWORD last;
public:
	CWP81ProjectionClient(LPCWSTR lpszUsbVid);
	~CWP81ProjectionClient();
public: //My Methods (Error Query:GetLastError)
	/*
	��ʼ��WPͶӰ��(�ֻ�������Ƿ������Ӱ�Ի���E
	������
		dwMaxBufferSize ��������ݻ�������С
	*/
	virtual BOOL Initialize(DWORD dwMaxBufferSize = WP_SCREEN_TO_PC_ALIGN512_MAX_SIZE);

	virtual void Reset();
	/*
	��ȡ��ǰ����Ļͼ��ͼ��Ϊ16bit��BMP���ݣ�
	������
		dwBufferSize ��������С
		pBuffer ���ݽ���д���������ǁE		pWidth ͼ��߶�
		pHeight ͼ�����
		pdwBits ͼ��ɫɁE		pOrientation ��Ļ��ρE	*/
	virtual BOOL ReadImageAsync();
	/*
	�ȴ�ҁE�IO��ȡ́E�
	������
		dwTimeout ��ʱʱ�䣬Ĭ�����޵ȴ�
	*/
	virtual BOOL WaitReadImageComplete(PBYTE* ppBuffer,PUINT32 pWidth,PUINT32 pHeight,PDWORD pdwBits, PDWORD pdwStride,PUINT pOrientation,DWORD dwTimeout = INFINITE,BOOL bFastCall = FALSE);

	virtual BOOL SendTouchEvent(UINT uMsg, WPARAM wParam, LPARAM lPos, LPARAM lSize, UINT Orientation);
};