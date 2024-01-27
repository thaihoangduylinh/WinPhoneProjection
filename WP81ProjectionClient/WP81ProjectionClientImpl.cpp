#include "WP81ProjectionClient.h"

//Export C-Style.
EXTERN_C HANDLE WINAPI InitWinPhoneProjectionClient(LPCWSTR lpstrUsbVid)
{
	if (lpstrUsbVid == NULL)
		return NULL;
	CWP81ProjectionClient* p = new CWP81ProjectionClient(lpstrUsbVid);
	if (p->Initialize())
		return p;
	else
	{
		delete p;
		return NULL;
	}
}

EXTERN_C VOID WINAPI FreeWinPhoneProjectionClient(HANDLE p)
{
	if (p)
		delete (CWP81ProjectionClient*)p;
}

EXTERN_C VOID WINAPI ResetWinPhoneProjectionClient(HANDLE p)
{
	if (p == NULL)
		return;
	((CWP81ProjectionClient*)p)->Reset();
}

EXTERN_C BOOL WINAPI ReadWinPhoneScreenImageAsync(HANDLE p)
{
	if (p == NULL)
		return FALSE;
	return ((CWP81ProjectionClient*)p)->ReadImageAsync();
}

EXTERN_C BOOL WINAPI WaitWinPhoneScreenImageComplete(HANDLE p,PBYTE* ppBuffer,PUINT32 pWidth,PUINT32 pHeight,PDWORD pdwBits,PDWORD pdwStride,PUINT pOrientation,DWORD dwTimeout,BOOL bFastCall)
{
	if (p == NULL)
		return FALSE;
	return ((CWP81ProjectionClient*)p)->WaitReadImageComplete(ppBuffer,pWidth,pHeight,pdwBits, pdwStride, pOrientation,dwTimeout,bFastCall);
}
EXTERN_C BOOL WINAPI SendWinPhoneTouchEvent(HANDLE p, UINT uMsg, WPARAM wParam, LPARAM lPos, LPARAM lSize, UINT Orientation)
{
	if (p == NULL)
		return FALSE;
	return ((CWP81ProjectionClient*)p)->SendTouchEvent(uMsg, wParam, lPos, lSize, Orientation);
}
