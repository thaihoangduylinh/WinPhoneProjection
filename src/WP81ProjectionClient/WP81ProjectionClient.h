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
	³õÊ¼»¯WPÍ¶Ó°ÒÇ(ÊÖ»ú»á³öÏÖÊÇ·ñÔÊĞúé¶Ó°¶Ô»°¿E
	²ÎÊı£º
		dwMaxBufferSize ×ûĞó¶ÁÊı¾İ»º³åÇø´óĞ¡
	*/
	virtual BOOL Initialize(DWORD dwMaxBufferSize = WP_SCREEN_TO_PC_ALIGN512_MAX_SIZE);

	virtual void Reset();
	/*
	¶ÁÈ¡µ±Ç°µÄÆÁÄ»Í¼Ïñ£¨Í¼ÏñÎª16bitµÄBMPÊı¾İ£©
	²ÎÊı£º
		dwBufferSize »º³åÇø´óĞ¡
		pBuffer Êı¾İ½«±»Ğ´µ½Õâ¸ö»º³åÇE		pWidth Í¼Ïñ¸ß¶È
		pHeight Í¼Ïñ¿úÒÈ
		pdwBits Í¼ÏñÉ«ÉE		pOrientation ÆÁÄ»·½ÏE	*/
	virtual BOOL ReadImageAsync();
	/*
	µÈ´ıÒE½IO¶ÁÈ¡ÍEÉ
	²ÎÊı£º
		dwTimeout ³¬Ê±Ê±¼ä£¬Ä¬ÈÏÎŞÏŞµÈ´ı
	*/
	virtual BOOL WaitReadImageComplete(PBYTE* ppBuffer,PUINT32 pWidth,PUINT32 pHeight,PDWORD pdwBits, PDWORD pdwStride,PUINT pOrientation,DWORD dwTimeout = INFINITE,BOOL bFastCall = FALSE);

	virtual BOOL SendTouchEvent(UINT uMsg, WPARAM wParam, LPARAM lPos, LPARAM lSize, UINT Orientation);
};