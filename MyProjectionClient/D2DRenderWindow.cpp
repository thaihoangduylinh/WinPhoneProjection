#include "stdafx.h"
#include "D2DRenderWindow.h"

CD2DRenderWindow::CD2DRenderWindow(HWND hWnd)
{
	hRender = CreateWindowW(L"#32770",NULL,WS_VISIBLE|WS_CHILD,0,0,0,0,hWnd,0,GetModuleHandle(NULL),NULL);
	SetWindowLongPtrW(hRender,GWLP_USERDATA,(LONG_PTR)this);
	SetWindowLongPtrW(hRender,GWLP_WNDPROC,(LONG_PTR)&CD2DRenderWindow::_StaticWndProc);
	InitializeCriticalSection(&cs);
}

CD2DRenderWindow::~CD2DRenderWindow()
{
	DeleteCriticalSection(&cs);
	ResetWinPhoneProjectionClient(hProjectionClient);
	if (hThreadReader)
	{
		//TerminateThread(hThreadReader,-1);
		WaitForSingleObject(hThreadReader, INFINITE);
		CloseHandle(hThreadReader);
		hThreadReader = NULL;
	}
	if (hSyncEvent)
		CloseHandle(hSyncEvent);
	if (hThreadRender)
	{
		//TerminateThread(hThreadRender,-1);
		WaitForSingleObject(hThreadRender, INFINITE);
		CloseHandle(hThreadRender);
		hThreadRender = NULL;
	}
	if (hProjectionClient)
		FreeWinPhoneProjectionClient(hProjectionClient);
	if (pBufOfConverted)
		free(pBufOfConverted);
	SAFE_RELEASE(pRawBitmap);
	SAFE_RELEASE(pBltBitmap);
	SAFE_RELEASE(pRender);
	SAFE_RELEASE(pD2DFactory);
	SAFE_RELEASE(pWICFactory);
	SAFE_RELEASE(pNotifySharedQueue);
	DestroyWindow(hRender);
}

CD2DRenderWindow::operator HWND() const
{
	return hRender;
}

CD2DRenderWindow::operator HRESULT() const
{
	return lastHR;
}

BOOL CD2DRenderWindow::Initialize(LPWSTR lpstrUsbVid)
{
	if (lpstrUsbVid == NULL)
	{
		lastHR = E_POINTER;
		return FALSE;
	}
	if (pWICFactory == NULL)
	{
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory1, NULL, CLSCTX_ALL, IID_PPV_ARGS(&pWICFactory))))
		{
			lastHR = E_FAIL;
			return FALSE;
		}
	}
	if (pD2DFactory == NULL)
	{
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &pD2DFactory)))
		{
			lastHR = E_ABORT;
			return FALSE;
		}
		pD2DFactory->CreateHwndRenderTarget(&D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE)),
			&D2D1::HwndRenderTargetProperties(hRender, D2D1::SizeU()),
			&pRender);
		if (hRender == NULL)
		{
			lastHR = E_DRAW;
			return FALSE;
		}
		pRender->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
	}
	if (hProjectionClient)
		FreeWinPhoneProjectionClient(hProjectionClient);
	hProjectionClient = InitWinPhoneProjectionClient(lpstrUsbVid);
	if (hProjectionClient == NULL)
	{
		lastHR = E_UNEXPECTED;
		return FALSE;
	}
	if (hSyncEvent == NULL)
	{
		hSyncEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	}
	if (hThreadReader == NULL)
	{
		hThreadReader = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CD2DRenderWindow::_StaticReaderThread, this, 0, NULL);
		SetThreadPriority(hThreadReader, THREAD_PRIORITY_HIGHEST);
	}
	if (hThreadRender == NULL)
	{
		hThreadRender = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&CD2DRenderWindow::_StaticRenderThread, this, 0, NULL);
		SetThreadPriority(hThreadRender, THREAD_PRIORITY_TIME_CRITICAL);
	}
	return TRUE;
}

BOOL CD2DRenderWindow::Pause()
{
	if (hSyncEvent == NULL)
		return FALSE;
	if (bPaused)
		return FALSE;

	EnterCriticalSection(&cs);
	SuspendThread(hThreadReader);
	SuspendThread(hThreadRender);
	bPaused = TRUE;
	LeaveCriticalSection(&cs);
	return TRUE;
}

BOOL CD2DRenderWindow::Resume()
{
	if (hSyncEvent == NULL)
		return FALSE;
	if (!bPaused)
		return FALSE;

	EnterCriticalSection(&cs);
	ResumeThread(hThreadReader);
	ResumeThread(hThreadRender);
	bPaused = FALSE;
	LeaveCriticalSection(&cs);
	return TRUE;
}

BOOL CD2DRenderWindow::MoveWindow(int x,int y,DWORD dwWidth,DWORD dwHeight, BOOL bRecalc)
{
	if (bRecalc) {
		UINT rWidth, rHeight;
		if (ForceOrientation == WP_ProjectionScreenOrientation_Hori_KeyBack || ForceOrientation == WP_ProjectionScreenOrientation_Hori_KeySearch) {
			rWidth = nHeight;
			rHeight = nWidth;
		}
		else {
			rWidth = nWidth;
			rHeight = nHeight;
		}
		if (dwWidth * rHeight > rWidth * dwHeight) {
			dwWidth = dwHeight * rWidth / rHeight;
			InvalidateRect(GetParent(hRender), NULL, TRUE);
		}
		else if (dwWidth * rHeight < rWidth * dwHeight) {
			dwHeight = dwWidth * rHeight / rWidth;
			InvalidateRect(GetParent(hRender), NULL, TRUE);
		}
	}
	return ::MoveWindow(hRender,x,y,dwWidth,dwHeight,TRUE);
}

BOOL CD2DRenderWindow::ForceChangeOrientation(WP81ProjectionScreenOrientation ori)
{
	if (pBltBitmap == NULL)
		return FALSE;

	EnterCriticalSection(&cs);
	if (ori == WP_ProjectionScreenOrientation_Hori_KeyBack || ori == WP_ProjectionScreenOrientation_Hori_KeySearch)
	{
		pRender->Resize(D2D1::SizeU(nHeight,nWidth));
		if (ori == WP_ProjectionScreenOrientation_Hori_KeyBack)
			pRender->SetTransform(D2D1::Matrix3x2F::Rotation(-90,D2D1::Point2F(nWidth / 2,nWidth / 2)));
		else
			pRender->SetTransform(D2D1::Matrix3x2F::Rotation(90,D2D1::Point2F(nHeight / 2,nHeight / 2)));
	}else{
		pRender->Resize(D2D1::SizeU(nWidth,nHeight));
		pRender->SetTransform(D2D1::Matrix3x2F::Rotation(0));
	}
	ForceOrientation = ori;
	if (ForceOrientation == WP_ProjectionScreenOrientation_Default) {
		ForceOrientation = WP_ProjectionScreenOrientation_Normal;
	}
	LeaveCriticalSection(&cs);
	return TRUE;
}

UINT CD2DRenderWindow::ReaderThread()
{
	_FOREVER_LOOP{
		if (!SyncReadWP16BitScreenImageWithWICBitmap(hProjectionClient,&Orientation,pWICFactory,&pRawBitmap,TRUE))
		{
			if (pRawBitmap)
			{
				pRawBitmap->Release();
				pRawBitmap = NULL;
			}
			CloseHandle(hSyncEvent);
			hSyncEvent = NULL;
			hThreadReader = NULL;
			PostMessageW(GetParent(hRender),WM_WP81_PC_HANGUP,NULL,NULL);
			break;
		}
		if (pRawBitmap) {
			if (nWidth == 0)
				pRawBitmap->GetSize(&nWidth, &nHeight);
			SetEvent(hSyncEvent);
		}
	}
	return NULL;
}

WNDPROC CD2DRenderWindow::SetNewWndProc(WNDPROC newproc)
{
	return (WNDPROC)SetWindowLongPtrW(hRender,GWLP_WNDPROC,(LONG_PTR)newproc);
}

UINT32 CD2DRenderWindow::GetFps()
{
	UINT32 nResult;
	DWORD dwNewTime = timeGetTime();
	if (dwNewTime - dwDrawTime > 1000)
	{
		dwDrawTime = dwNewTime;
		nResult = nDrawFps = nDrawFrames;
		nDrawFrames = 0;
	}else
		nResult = nDrawFps;
	return nResult;
}

VOID CD2DRenderWindow::SetInformationRecorder(DWORD dwFps,HANDLE hEvent,IComInterfaceQueue<IWICBitmapSource>* pSharedQueue)
{
	this->dwFps = dwFps;
	hEventRender = hEvent;
	MFFrameRateToAverageTimePerFrame(dwFps,1,(PUINT64)&FpsDelayIn100ns);
	FpsDelayIn100ns /= 2;
	pSharedQueue->AddRef();
	pNotifySharedQueue = pSharedQueue;
}

VOID CD2DRenderWindow::ClearRecorder()
{
	hEventRender = NULL;
	SAFE_RELEASE(pNotifySharedQueue);
}

UINT CD2DRenderWindow::RenderThread()
{
	DWORD dwAvTaskIndex = 0;
	HANDLE hAvTask = AvSetMmThreadCharacteristicsW(L"Low Latency",&dwAvTaskIndex);
	AvSetMmThreadPriority(hAvTask,AVRT_PRIORITY_CRITICAL);
	_FOREVER_LOOP{
		DWORD ret = WaitForSingleObject(hSyncEvent, 1000);
		if (ret == WAIT_OBJECT_0)
		{
			if (nWidth == 0 || nHeight == 0)
				continue;
			if (Orientation != PrevOrientation)
			{
				SendMessageW(GetParent(hRender),WM_WP81_PC_ORI_CHANGED,Orientation,PrevOrientation);
				PrevOrientation = Orientation;
			}

			IWICBitmapSource* pRenderBitmap;
			if (!ConvertWICBitmapFormat(pWICFactory,GUID_WICPixelFormat32bppPBGRA,pRawBitmap,&pRenderBitmap))
				continue;
			if (pRenderBitmap == NULL)
				continue;
			if (pBltBitmap == NULL)
			{
				dwImageStride = nWidth * 4;
				pBufOfConverted = (PBYTE)malloc(dwImageStride * nHeight);
				pRender->CreateBitmapFromWicBitmap(pRenderBitmap,&pBltBitmap);
				pRender->Resize(D2D1::SizeU(nWidth,nHeight));
				SendMessageTimeoutW(GetParent(hRender),WM_WP81_PC_SHOW,nWidth,nHeight,0,2100,NULL);
			}else{
				pRenderBitmap->CopyPixels(NULL,dwImageStride,dwImageStride * nHeight,pBufOfConverted);
				pBltBitmap->CopyFromMemory(NULL,pBufOfConverted,dwImageStride);
			}
			if (pBltBitmap)
			{
				pRender->BeginDraw();
				pRender->DrawBitmap(pBltBitmap,NULL,1.0,D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
				pRender->EndDraw();
				nDrawFrames++;
			}
			if (pNotifySharedQueue)
			{
				MFTIME TimeIn100ns = MFGetSystemTime();
				if (TimeIn100ns - DrawTime100ns >= FpsDelayIn100ns)
				{
					DrawTime100ns = TimeIn100ns;
					pNotifySharedQueue->PushIUnknownSafe(pRenderBitmap);
					SetEvent(hEventRender);
				}
			}
			pRenderBitmap->Release();
		}
		else if (ret != WAIT_TIMEOUT){
			break;
		}
	}
	AvRevertMmThreadCharacteristics(hAvTask);
	hThreadRender = NULL;
	return NULL;
}

LRESULT CD2DRenderWindow::WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWnd, &ps);
		HBRUSH hBursh = CreateSolidBrush(0);
		RECT rc;
		GetClientRect(hWnd, &rc);
		FillRect(hDC, &rc, hBursh);
		DeleteObject(hBursh);
		EndPaint(hWnd, &ps);
	}
		break;
	case WM_POINTERDOWN:
	case WM_POINTERUP:
	case WM_POINTERUPDATE:
	{
		POINT pos;
		RECT rect;
		SIZE sz;
		pos.x = LOWORD(lParam);
		pos.y = HIWORD(lParam);
		ScreenToClient(hWnd, &pos);
		GetClientRect(hWnd, &rect);
		sz.cx = rect.right - rect.left;
		sz.cy = rect.bottom - rect.top;
		SendWinPhoneTouchEvent(hProjectionClient, uMsg, wParam, MAKELONG(pos.x, pos.y), MAKELONG(sz.cx, sz.cy), ForceOrientation);
	}
		return 0;
	}
	return DefWindowProcW(hWnd,uMsg,wParam,lParam);
}
BOOL CD2DRenderWindow::SendKey(UINT uMsg, WPARAM vkey)
{
	SendWinPhoneTouchEvent(hProjectionClient, uMsg, vkey, 0, 0, 0);
	return TRUE;
}

VOID CALLBACK CD2DRenderWindow::_StaticReaderThread(PVOID p)
{
	UINT ExitCode = ((CD2DRenderWindow*)p)->ReaderThread();
	ExitThread(ExitCode);
}

VOID CALLBACK CD2DRenderWindow::_StaticRenderThread(PVOID p)
{
	UINT ExitCode = ((CD2DRenderWindow*)p)->RenderThread();
	ExitThread(ExitCode);
}

LRESULT CALLBACK CD2DRenderWindow::_StaticWndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	CD2DRenderWindow* pThis = (CD2DRenderWindow*)GetWindowLongPtrW(hWnd,GWLP_USERDATA);
	if (pThis)
		return pThis->WndProc(hWnd,uMsg,wParam,lParam);
	else
		return DefWindowProcW(hWnd,uMsg,wParam,lParam);
}