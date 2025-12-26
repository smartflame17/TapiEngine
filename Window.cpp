#include "Window.h"
#include <sstream>
#include "resource.h"
#include "imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window Class Stuff
Window::WindowClass Window::WindowClass::wndClass;

Window::WindowClass::WindowClass() noexcept
	:
	hInst(GetModuleHandle(nullptr))
{
	WNDCLASSEX wc = { 0 };
	wc.cbSize = sizeof(wc);
	wc.style = CS_OWNDC;
	wc.lpfnWndProc = HandleMsgSetup;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = GetInstance();
	wc.hIcon = static_cast<HICON>(LoadImage(GetInstance(), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 32, 32, 0));		// load resource image icon with 32x32 size, in bitmap
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = GetName();
	wc.hIconSm = static_cast<HICON>(LoadImage(GetInstance(), MAKEINTRESOURCE(IDI_ICON2), IMAGE_ICON, 16, 16, 0));	// 16x16 small icon
	RegisterClassEx(&wc);
}

Window::WindowClass::~WindowClass()
{
	UnregisterClass(wndClassName, GetInstance());
}

const char* Window::WindowClass::GetName() noexcept
{
	return wndClassName;
}

HINSTANCE Window::WindowClass::GetInstance() noexcept
{
	return wndClass.hInst;
}

// Window Stuff
Window::Window(int width, int height, const char* name):
	width (width),
	height (height)
{
	// calculate window size based on desired client region size
	RECT wr;
	wr.left = 100;
	wr.right = width + wr.left;
	wr.top = 100;
	wr.bottom = height + wr.top;
	if (AdjustWindowRect(&wr, WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU, FALSE) == 0)		// specify size of WINDOW from the CLIENT window size (wr)
		throw SFWND_LAST_EXCEPT();
	// create window & get hWnd
	hWnd = CreateWindow(
		WindowClass::GetName(), name,
		WS_CAPTION | WS_MINIMIZEBOX | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,	// let windows decide initial window position
		nullptr, nullptr, WindowClass::GetInstance(), this						// pass pointer to class instance (to save the custom window class in message callback setup)
	);

	if (hWnd == nullptr)
		throw SFWND_LAST_EXCEPT();

	// show window
	ShowWindow(hWnd, SW_SHOWDEFAULT);

	// init imgui impl
	ImGui_ImplWin32_Init(hWnd); // if future projects use multiple windows, need to modify this to handle multiple windows

	// create graphics object after handle initialized
	pGfx = std::make_unique<Graphics>(hWnd, width, height);
}

Window::~Window() {
	ImGui_ImplWin32_Shutdown();
	DestroyWindow(hWnd);
}

void Window::SetTitle(const std::string& title)
{
	if (SetWindowText(hWnd, title.c_str()) == 0)
		throw SFWND_LAST_EXCEPT();
}

// Using PeekMessage instead of GetMessage (which blocks until new message) to allow loop continue without any user actions
std::optional<int> Window::ProcessMessages()
{
	MSG msg;
	// While queue has message, remove from queue and dispatch them
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT)
			return msg.wParam;	// if quit message, return arg for PostQuitMessage

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return {};	// return empty optional if not quitting app
}

Graphics& Window::Gfx()
{
	if (!pGfx)
		throw SFWND_NOGFX_EXCEPT();
	return *pGfx;
}

// Message handling
LRESULT WINAPI Window::HandleMsgSetup(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept		// inital static window procedure handler (defined in CreateWindow), sets up the pointer of each instance
{
	if (msg == WM_NCCREATE) {	// when window is first created
		const CREATESTRUCTW* const pCreate = reinterpret_cast<CREATESTRUCTW*> (lParam);		// lParam contains CREATESTRUCTW, which has the pointer to custom window class we set on CreatWindow of constructor
		Window* const pWnd = static_cast<Window*>(pCreate->lpCreateParams);					// cast void pointer into window class pointer

		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pWnd));			// set winapi-side custom user data to pointer of window class
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Window::HandleMsgThunk));	// set message procedure (callback) to non-setup function since setup is done

		return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT WINAPI Window::HandleMsgThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept		// default static window procedure handler, retrieves info of class instance and let their handler do stuff (basically invokes the member function)
{
	Window* const pWnd = reinterpret_cast<Window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));		// retrieve the instance pointer stored earlier as custom user data
	return pWnd->HandleMsg(hWnd, msg, wParam, lParam);
}

LRESULT Window::HandleMsg(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) noexcept
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;
	const auto imio = ImGui::GetIO();
	switch (msg) {
	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;		// only post quit message, as window destruction is handled by destructor

		// ----------- Keyboard Message Handling ----------- //
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:		//SYSKEY handling to hande the ALT(VK_MENU) key, thanks bill gates for the intuitive interface

		// stifle keyboard message when imgui is using keyboard input
		if (imio.WantCaptureKeyboard)
			break;

		if (!(lParam & 0x40000000) || kbd.IsAutoRepeatEnabled()) {	// bit 30 of lParam is 1 if previously held down (autorepeat), 
			kbd.OnKeyPressed(static_cast<unsigned char>(wParam));
		}
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		// stifle keyboard message when imgui is using keyboard input
		if (imio.WantCaptureKeyboard)
			break;

		kbd.OnKeyReleased(static_cast<unsigned char>(wParam));
		break;

	case WM_CHAR:		// both standard messages and WM_CHAR is sent when a ASCII-correspondant key is pressed
		// stifle keyboard message when imgui is using keyboard input
		if (imio.WantCaptureKeyboard)
			break;

		kbd.OnChar(static_cast<unsigned char>(wParam));
		break;

	case WM_KILLFOCUS:	// if user un-focuses window, clear the key states
		kbd.ClearState();
		break;
		// ----------- End Keyboard Message Handling ----------- //
		
		// ---------- Mouse message Handling ---------- //
	case WM_MOUSEMOVE: {
		// stifle mouse message when imgui is using mouse input
		if (imio.WantCaptureMouse)
			break;

		const POINTS pt = MAKEPOINTS(lParam);
		// if Mouse is in client region, we log the entry to buffer and capture the mouse to handle it
		if (pt.x >= 0 && pt.x < width && pt.y >= 0 && pt.y < height) {
			mouse.OnMouseMove(pt.x, pt.y);
			if (!mouse.IsInWindow()) {
				SetCapture(hWnd);
				mouse.OnMouseEnter();
			}
		}
		// if mouse is outside client region, we log the exit and maintain capture if button is held down
		else {
			if (wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON))
				mouse.OnMouseMove(pt.x, pt.y);
			// button released, remove capture and log leave to buffer
			else {
				ReleaseCapture();
				mouse.OnMouseLeave();
			}
		}
		break;
	}
	case WM_LBUTTONDOWN: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnLeftPressed(pt.x, pt.y);
		break;
	}
	case WM_RBUTTONDOWN: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnRightPressed(pt.x, pt.y);
		break;
	}
	case WM_MBUTTONDOWN: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnMiddlePressed(pt.x, pt.y);
		break;
	}
	case WM_LBUTTONUP: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnLeftReleased(pt.x, pt.y);
		break;
	}
	case WM_RBUTTONUP: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnRightReleased(pt.x, pt.y);
		break;
	}
	case WM_MBUTTONUP: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		mouse.OnMiddleReleased(pt.x, pt.y);
		break;
	}
	case WM_MOUSEWHEEL: {
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		mouse.OnWheelDelta(pt.x, pt.y, delta);
		break;
	}
	// ---------- End Mouse message Handling ---------- //
	}
	
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Exception handling
Window::HrException::HrException(int line, const char* file, HRESULT hr) noexcept:
	SmflmException(line, file),
	hr (hr)
{}

const char* Window::HrException::what() const noexcept
{
	std::ostringstream oss;
	oss << GetType() << std::endl
		<< "[Error Code]" << GetErrorCode() << std::endl
		<< "[Descripton]" << GetErrorString() << std::endl
		<< GetOriginString();
	whatBuffer = oss.str();
	return whatBuffer.c_str();
}

const char* Window::HrException::GetType() const noexcept
{
	return "Smflm Window Exception";
}

std::string Window::HrException::TranslateErrorCode(HRESULT hr) noexcept		// uses windows-provided macro to format error code into readable message
{
	char* pMsgBuf = nullptr;
	DWORD nMsgLen = FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&pMsgBuf), 0, nullptr);
	// Windows macro formats error code HRESULT hr into string
	if (nMsgLen == 0) return "Unidentified Error Code";
	std::string errorString = pMsgBuf;
	LocalFree(pMsgBuf);			// save to string object and deallocate pMsgBuf
	return errorString;
}

HRESULT Window::HrException::GetErrorCode() const noexcept
{
	return hr;
}

std::string Window::HrException::GetErrorString() const noexcept
{
	return TranslateErrorCode(hr);
}

const char* Window::NoGfxException::GetType() const noexcept
{
	return "Graphics Exception [Device Removed] (DXGI_ERROR_DEVICE_REMOVED)";
}
