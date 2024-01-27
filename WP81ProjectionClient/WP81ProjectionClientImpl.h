#pragma once

#ifndef _INC_WINDOWS
#include <Windows.h>
#endif

typedef enum{
	WP_ProjectionScreenOrientation_Default = 0, //默认（竖）
	WP_ProjectionScreenOrientation_Normal = 1, //竖屏
	WP_ProjectionScreenOrientation_Hori_KeyBack = 2, //横屏（方向向着后退按钮）
	WP_ProjectionScreenOrientation_Hori_KeySearch = 8, //横屏（方向向着搜索按钮）
}WP81ProjectionScreenOrientation,*PWP81ProjectionScreenOrientation; 

EXTERN_C HANDLE WINAPI InitWinPhoneProjectionClient(LPCWSTR lpstrUsbVid);
EXTERN_C VOID WINAPI FreeWinPhoneProjectionClient(HANDLE p);
EXTERN_C VOID WINAPI ResetWinPhoneProjectionClient(HANDLE p);
EXTERN_C BOOL WINAPI ReadWinPhoneScreenImageAsync(HANDLE p);
EXTERN_C BOOL WINAPI WaitWinPhoneScreenImageComplete(HANDLE p,PBYTE* ppBuffer,PUINT32 pWidth,PUINT32 pHeight,PDWORD pdwBits,PDWORD pdwStride,PUINT pOrientation,DWORD dwTimeout = INFINITE,BOOL bFastCall = FALSE);
EXTERN_C BOOL WINAPI SendWinPhoneTouchEvent(HANDLE p, UINT uMsg, WPARAM wParam, LPARAM lPos, LPARAM lSize, UINT Orientation);

EXTERN_C HANDLE WINAPI FindFirstUsbBusDev();
EXTERN_C BOOL WINAPI FindNextUsbBusDev(HANDLE h);
EXTERN_C BOOL WINAPI FindUsbBusGetDevPath(HANDLE h,LPWSTR lpstrBuffer,DWORD cchBuffer);
EXTERN_C BOOL WINAPI FindUsbBusGetDisplayName(HANDLE h,LPWSTR lpstrBuffer,DWORD cchBuffer);
EXTERN_C VOID WINAPI FindUsbBusClose(HANDLE h);