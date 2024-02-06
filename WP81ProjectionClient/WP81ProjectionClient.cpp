#include "WP81ProjectionClient.h"
#include <mfapi.h>

__forceinline BOOL InitWP81UsbPipePolicy(WINUSB_INTERFACE_HANDLE hUsbInterface, PWP_SCRREN_TO_PC_PIPE p)
{
	BOOL bResult = FALSE;
	USB_INTERFACE_DESCRIPTOR usbInterfaceDesc = {};
	WinUsb_QueryInterfaceSettings(hUsbInterface, 0, &usbInterfaceDesc);
	if (usbInterfaceDesc.bNumEndpoints == 2) //Ò»¸ö¿ØÖÆ£¬Ò»¸ö¶ÁÆÁÄ»
	{
		BOOLEAN bOpenState = TRUE;
		WinUsb_QueryPipe(hUsbInterface, 0, 0, &p->PipeOfData);
		WinUsb_QueryPipe(hUsbInterface, 0, 1, &p->PipeOfControl);
		if (p->PipeOfControl.PipeType == UsbdPipeTypeBulk && p->PipeOfData.PipeType == UsbdPipeTypeBulk)
		{
			if (WinUsb_SetPipePolicy(hUsbInterface, p->PipeOfControl.PipeId, 7, 1, &bOpenState) && WinUsb_SetPipePolicy(hUsbInterface, p->PipeOfData.PipeId, 7, 1, &bOpenState))
				bResult = TRUE;
		}
	}
	return bResult;
}

__forceinline BOOL SendWP81UsbCtlCode(WINUSB_INTERFACE_HANDLE hUsbInterface, UINT code)
{
	WINUSB_SETUP_PACKET usbPacket = {};
	usbPacket.RequestType = 0x21; //magic
	usbPacket.Request = code;
	DWORD dwTemp = 0;
	return WinUsb_ControlTransfer(hUsbInterface, usbPacket, NULL, 0, &dwTemp, NULL);
}

//¹¹ÔE¯Êý
CWP81ProjectionClient::CWP81ProjectionClient(LPCWSTR lpszUsbVid)
{
	lpstrUsbVid = StrDupW(lpszUsbVid);
	hFile = CreateFileW(lpstrUsbVid,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
	hEvent = CreateEventExW(NULL,NULL,CREATE_EVENT_MANUAL_RESET,EVENT_ALL_ACCESS);
	hWrite = CreateEventExW(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	InitializeCriticalSectionEx(&cs,0,CRITICAL_SECTION_NO_DEBUG_INFO);
	RtlZeroMemory(&usbPipe,sizeof(usbPipe));
	RtlZeroMemory(&ioOverlapped,sizeof(ioOverlapped));
	ioOverlapped.hEvent = hEvent;
	RtlZeroMemory(&ioWrite, sizeof(ioWrite));
	ioWrite.hEvent = hWrite;
	EnableMouseInPointer(TRUE);
	send[0] = 0x44555608;
	send[1] = 0x00000003;
	send[2] = 0x000000c0;

	send[3] = 0x00000000;
	send[4] = 0x00000000;
	send[5] = 0x00000000;

	send[6] = 0x00000000;
	send[7] = 0x00000000;
	last = 0;
}

//Îö¹¹º¯Êý
CWP81ProjectionClient::~CWP81ProjectionClient()
{
	SendWP81UsbCtlCode(hUsb, 1);
	if (hUsb)
		WinUsb_Free(hUsb);
	if (pBufOfData)
		GlobalFree(pBufOfData);
	if (lpstrUsbVid)
		CoTaskMemFree(lpstrUsbVid);
	DeleteCriticalSection(&cs);
	SetEvent(hWrite);
	CloseHandle(hWrite);
	if (hEvent) {
		SetEvent(hEvent);
		CloseHandle(hEvent);
		hEvent = NULL;
	}
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
}

BOOL CWP81ProjectionClient::Initialize(DWORD dwMaxBufferSize)
{
	if (pBufOfData)
		return FALSE;
	if (dwMaxBufferSize == 0)
		return FALSE;
	if (!WinUsb_Initialize(hFile,&hUsb))
		return FALSE;
	if (!InitWP81UsbPipePolicy(hUsb,&usbPipe))
		return FALSE;
	if (!SendWP81UsbCtlCode(hUsb,WP_SCREEN_TO_PC_CTL_CODE_NOTIFY)) //WP8.1ÊÖ»ú»áÏÔÊ¾Í¶Ó°Í¨Öª
		return FALSE;
	DWORD dwTemp;
	WinUsb_ResetPipe(hUsb,usbPipe.PipeOfControl.PipeId);
	WinUsb_ResetPipe(hUsb,usbPipe.PipeOfData.PipeId);
	WinUsb_GetOverlappedResult(hUsb,&ioOverlapped,&dwTemp,TRUE);
	ResetEvent(hEvent);
	pBufOfData = GlobalAlloc(GPTR,dwMaxBufferSize);
	dwBufferSize = dwMaxBufferSize;
	return TRUE;
}

void CWP81ProjectionClient::Reset()
{
	WinUsb_ResetPipe(hUsb, usbPipe.PipeOfControl.PipeId);
	WinUsb_ResetPipe(hUsb, usbPipe.PipeOfData.PipeId);
	SetEvent(hEvent);
	CloseHandle(hEvent);
	hEvent = NULL;
}

BOOL CWP81ProjectionClient::ReadImageAsync()
{
	if (hUsb == NULL)
		return FALSE;

	BOOL bResult = FALSE;
	EnterCriticalSection(&cs);
	if (bIoPending) //ÕýÔÚReadPipeÖÐ¡£¡£¡£
	{
		if (WinUsb_AbortPipe(hUsb,usbPipe.PipeOfData.PipeId))
		{
			DWORD dwTemp;
			WinUsb_GetOverlappedResult(hUsb,&ioOverlapped,&dwTemp,TRUE);
			ResetEvent(hEvent);
		}
	}
	if (SendWP81UsbCtlCode(hUsb,WP_SCREEN_TO_PC_CTL_CODE_READY))
	{
		if (!WinUsb_ReadPipe(hUsb, usbPipe.PipeOfData.PipeId, (PUCHAR)pBufOfData, dwBufferSize, NULL, &ioOverlapped))
		{
			if (GetLastError() == ERROR_IO_PENDING)
				bIoPending = bResult = TRUE;
		}
	}
	LeaveCriticalSection(&cs);
	return bResult;
}

BOOL CWP81ProjectionClient::WaitReadImageComplete(PBYTE* ppBuffer,PUINT32 pWidth,PUINT32 pHeight,PDWORD pdwBits, PDWORD pdwStride,PUINT pOrientation,DWORD dwTimeout,BOOL bFastCall)
{
	if (!bFastCall)
	{
		if (ppBuffer == NULL)
			return FALSE;
		if (hUsb == NULL)
			return FALSE;
	}

	BOOL bResult = FALSE;
	if (!bFastCall)
		EnterCriticalSection(&cs);
	if (WaitForSingleObjectEx(hEvent,dwTimeout,FALSE) == WAIT_OBJECT_0)
	{
		DWORD dwTemp;
		WinUsb_GetOverlappedResult(hUsb, &ioOverlapped, &dwTemp, TRUE);
		ResetEvent(hEvent);
		bIoPending = FALSE;
		PWP_SCRREN_TO_PC_DATA pData = (PWP_SCRREN_TO_PC_DATA)pBufOfData;
		if (pData->dwFourcc == WP_SCREEN_TO_PC_FLAG_FOURCC && pData->dwFlags == 2 && pData->nImageDataLength > 0)
		{
			if (pWidth)
				*pWidth = pData->wDisplayWidth;
			if (pHeight)
				*pHeight = pData->wDisplayHeight;
			if (pOrientation) {
				*pOrientation = pData->wScrOrientation;
				if (*pOrientation == 0) {
					*pOrientation = 1;
				}
			}
			if (pdwBits)
				*pdwBits = pData->dwImageBits;

			LONG width = pData->dwUnk3;
			if (width == NULL || width == 0) {
				width = (pData->nImageDataLength - WP_SCREEN_TO_PC_IMAGE_DATA_OFFSET) / pData->wDisplayHeight;
			}

			if (dwPhoneStride == 0)
			{
				dwPhoneWidth = pData->wRawWidth;
				if (dwPhoneWidth == NULL || dwPhoneWidth == 0) {
					dwPhoneWidth = (pData->nImageDataLength - WP_SCREEN_TO_PC_IMAGE_DATA_OFFSET) / pData->wDisplayHeight;
				}
				dwPhoneHeight = pData->wRawHeight;
				dwPhoneStride = dwPhoneWidth * 4;
			}

			DWORD dwBitSize = 1;
			if (pData->dwImageBits == 16)
				dwBitSize = 2;
			else if (pData->dwImageBits == 24)
				dwBitSize = 3;
			else if (pData->dwImageBits == 32)
				dwBitSize = 4;
			
			

			if (pdwStride)
				*pdwStride = width;
			if (ppBuffer)
				*ppBuffer = ((PBYTE)pData) + (pData->nImageDataLength - width * pData->wDisplayHeight);
			bResult = TRUE;
		}
	}
	if (!bFastCall)
		LeaveCriticalSection(&cs);
	return bResult;
}

long GetIndexBuf(DWORD* send, UINT uMsg, long id)
{
	long idx=0;
	while (idx < send[7])
	{
		if (send[8 + idx * 4] == id)
		{
			return idx;
		}
		idx++;
	}
	if (uMsg == WM_POINTERDOWN)
	{
		if (send[7] < 10) {
			send[8 + send[7] * 4] = id;
			send[7]++;
			return send[7] - 1;
		}
	}
	return -1;
}
void ReleaseIndexBuf(DWORD* send, UINT uMsg, long idx)
{
	if (uMsg == WM_POINTERUP) {
		if (idx < (send[7] - 1))
		{
			memmove(&(send[8 + idx * 4]), &(send[8 + (idx + 1) * 4]), 16);
		}
		send[7]--;
	}
}
BOOL CWP81ProjectionClient::SendTouchEvent(UINT uMsg, WPARAM wParam, LPARAM lPos, LPARAM lSize, UINT Orientation)
{
	long idx = 0;
	if ((uMsg == WM_KEYDOWN)||(uMsg == WM_KEYUP))
	{
		send[6] = 0x00000001;
		//send[7] = (wParam == VK_BACK) ? 0 : 1;

		switch (wParam) {
			case VK_BACK:
				send[7] = 0;
				break;
			case 0x48:
				send[7] = 1;
				break;
			case VK_SPACE:
				send[7] = 2;
				break;
		}
		send[8] = (uMsg == WM_KEYDOWN) ? 1 : 0;
	}
	else {
		idx = GetIndexBuf(send, uMsg, GET_POINTERID_WPARAM(wParam));
		if (idx < 0) {
			return TRUE;
		}

		POINT pos;
		SIZE sz;
		sz.cx = LOWORD(lSize);
		sz.cy = HIWORD(lSize);
		switch (Orientation)
		{
		case 2:
			pos.x = (sz.cy - HIWORD(lPos))* sz.cx / sz.cy;
			pos.y = LOWORD(lPos)*sz.cy / sz.cx;
			break;
		case 4:
			pos.x = sz.cx - LOWORD(lPos);
			pos.y = sz.cy - HIWORD(lPos);
			break;
		case 8:
			pos.x = HIWORD(lPos)* sz.cx / sz.cy;
			pos.y = (sz.cx - LOWORD(lPos))*sz.cy / sz.cx;
			break;
		default:
			pos.x = LOWORD(lPos);
			pos.y = HIWORD(lPos);
		}
		DWORD newpos = MAKELONG(dwPhoneWidth * pos.x / sz.cx, dwPhoneHeight * pos.y / sz.cy);

		send[6] = 0x00000000;
		switch (uMsg) {
		case WM_POINTERUPDATE:
			if (send[11 + idx * 4] == newpos)
			{
				return TRUE;
			}
			send[9 + idx * 4] = 0x00000002;
			send[10 + idx * 4] = 0x00000002;
			break;
		case WM_POINTERDOWN:
			send[9 + idx * 4] = 0x00000002;
			send[10 + idx * 4] = 0x00000000;
			break;
		case WM_POINTERUP:
			send[9 + idx * 4] = 0x00000000;
			send[10 + idx * 4] = 0x00000002;
			break;
		}
		send[11 + idx * 4] = newpos;
	}

	DWORD crnt = GetTickCount();
	if ((last + 12) > crnt)
	{
		Sleep(last + 12 - crnt);
	}
	DWORD dwErr = ERROR_SUCCESS;
	EnterCriticalSection(&cs);
	if(!WinUsb_WritePipe(hUsb, usbPipe.PipeOfControl.PipeId, (PUCHAR)send, 192, NULL, &ioWrite))
	{
		dwErr = GetLastError();
	}
	LeaveCriticalSection(&cs);
	if (dwErr == ERROR_IO_PENDING)
	{
		if (WaitForSingleObjectEx(hWrite, INFINITE, FALSE) == WAIT_OBJECT_0)
		{
			DWORD dwTemp;
			WinUsb_GetOverlappedResult(hUsb, &ioWrite, &dwTemp, TRUE);
			ResetEvent(hWrite);
		}
	}
	last = crnt;

	ReleaseIndexBuf(send, uMsg, idx);
	return TRUE;
}
