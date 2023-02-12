#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "stun thread data.h"
#include "STUNClient.h"
#include <Windows.h>
#include <string>
#include <thread>
#include <chrono>

static HWND winHWND;
static char szTitle[100] = "STUN Client";
static char szWindowClass[100] = "STUN Client Window Class";
static unsigned short hPadding = 10;
static unsigned short vPadding = 10;
static unsigned short topMargin = 10;
static unsigned short rightMargin = 10;
static unsigned int msInterval = 6000;

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

static void updateWindow(HWND hwnd, std::string& str) {
	static bool firstTime = true;

	HDC hdc = GetDC(hwnd);

	RECT requiredRect;
	RECT desktopRect;
	requiredRect.top = 0;
	requiredRect.bottom = 0;
	requiredRect.left = 0;
	requiredRect.right = 0;

	DrawText(hdc, str.c_str(), str.size(), &requiredRect, DT_CALCRECT);

	ReleaseDC(hwnd, hdc);

	GetWindowRect(GetDesktopWindow(), &desktopRect);

	unsigned short calcWidth = requiredRect.right - requiredRect.left;
	unsigned short calcHeight = requiredRect.bottom - requiredRect.top;

	unsigned short totalWidth = calcWidth + hPadding * 2;
	unsigned short totalHeight = calcHeight + vPadding * 2;

	SetWindowPos(hwnd, HWND_TOPMOST, desktopRect.right - totalWidth - rightMargin, topMargin, totalWidth, 37, SWP_NOREDRAW);

	if (firstTime) {
		ShowWindow(winHWND, SW_SHOW);
		UpdateWindow(winHWND);
		firstTime = false;
	}

	InvalidateRect(hwnd, NULL, TRUE);
}

static void stunThreadProc(STUNThreadData* data) {
	while (true) {
		data->dataLock->lock();
		{
			data->state = STUNState::Fetching;
			std::string fetchingStateSTR("Contacting ");
			fetchingStateSTR.append(data->targetAddress);
			fetchingStateSTR.append(":");
			fetchingStateSTR.append(std::to_string(data->targetPort));
			fetchingStateSTR.append("...");
			data->result = fetchingStateSTR;
		}
		data->dataLock->unlock();

		updateWindow(winHWND, data->result);

		std::string result;
		bool success = fetch(data->targetAddress.c_str(), data->targetPort, result);

		data->dataLock->lock();
		{
			data->state = ((success) ? (STUNState::SuccessFetched) : (STUNState::Error));
			data->result = result;
		}
		data->dataLock->unlock();

		updateWindow(winHWND, data->result);

		std::this_thread::sleep_for(std::chrono::milliseconds(msInterval));
	}

	return;
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	STUNThreadData* stunData = new STUNThreadData();
	stunData->dataLock = new std::mutex();
	stunData->state = STUNState::Unknown;
	stunData->targetAddress = "stun.avigora.fr";
	stunData->targetPort = 3478;

	std::string startResult;
	bool successInit = start(startResult);

	if (!successInit)
		return EXIT_FAILURE;

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
		return EXIT_FAILURE;

	winHWND = CreateWindow(szWindowClass, szTitle, 0, CW_USEDEFAULT, CW_USEDEFAULT, 500, 100, nullptr, nullptr, hInstance, stunData);
	SetWindowLong(winHWND, GWL_STYLE, 0);

	if (!winHWND)
		return EXIT_FAILURE;

	ShowWindow(winHWND, SW_HIDE);

	std::thread stunThread(stunThreadProc, stunData);

	MSG msg;
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	TerminateThread(stunThread.native_handle(), 0);
	stunThread.join();

	stop();

	return (int)msg.wParam;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
		STUNThreadData* stunData = reinterpret_cast<STUNThreadData*>(pCreate->lpCreateParams);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)stunData);
	}
	break;
	case WM_GETMINMAXINFO:
	{
		LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
		lpMMI->ptMinTrackSize.x = 1;
		lpMMI->ptMinTrackSize.y = 1;
	}
	break;
	case WM_PAINT:
	{
		//Beep(10000, 10);
		STUNThreadData* stunData = reinterpret_cast<STUNThreadData*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

		RECT  rect;
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		GetClientRect(hWnd, &rect);
		rect.left += hPadding;
		rect.top += vPadding;

		stunData->dataLock->lock();

		switch (stunData->state) {
		case STUNState::Error:
			SetTextColor(hdc, RGB(255, 0, 0));
			break;
		case STUNState::Fetching:
		{
			int gray = 90;
			SetTextColor(hdc, RGB(gray, gray, gray));
		}
		break;
		default:
			SetTextColor(hdc, RGB(0, 0, 0));
		}

		DrawText(hdc, (LPSTR)stunData->result.c_str(), stunData->result.size(), &rect, 0);
		stunData->dataLock->unlock();

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}
	break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
}
