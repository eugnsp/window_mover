#pragma once
#include "pch.hpp"

constexpr UINT MAX_WINDOW_TITLE_LEN = 80;

class Window_icon
{
public:
	Window_icon(HWND);
	Window_icon(const Window_icon&) = delete;

	Window_icon(Window_icon&&) noexcept;
	Window_icon& operator=(Window_icon&&) noexcept;

	~Window_icon();

	HBITMAP Handle() const;

private:
	HBITMAP icon_ = NULL;
};

class Window
{
public:
	Window(HWND);
	Window(Window&&) = default;

	Window& operator=(Window&&) noexcept;

	HWND Handle() const;
	CString Title() const;
	HBITMAP IconHandle() const;

	bool operator<(const Window&) const;

private:
	HWND wnd_;
	DWORD pid_;
	CString title_;
	Window_icon icon_;
	bool is_minimized_;
};

//////////////////////////////////////////////////////////////////////////

Window_icon::Window_icon(HWND wnd)
{
	auto icon = reinterpret_cast<HICON>(SendMessage(wnd, WM_GETICON, ICON_SMALL2, 0));
	if (!icon)
		icon = reinterpret_cast<HICON>(SendMessage(wnd, WM_GETICON, ICON_SMALL, 0));
	if (!icon)
		icon = reinterpret_cast<HICON>(GetClassLongPtr(wnd, GCLP_HICONSM));
	if (!icon)
		icon = reinterpret_cast<HICON>(SendMessage(wnd, WM_GETICON, ICON_BIG, 0));
	if (!icon)
		icon = reinterpret_cast<HICON>(GetClassLongPtr(wnd, GCLP_HICON));

	if (!icon)
		return;

	const auto width = GetSystemMetrics(SM_CXMENUCHECK);
	const auto height = GetSystemMetrics(SM_CYMENUCHECK);

	const HDC screen = GetDC(NULL);
	const HDC dc = CreateCompatibleDC(screen);
	icon_ = CreateCompatibleBitmap(screen, width, height);
	const auto old_bitmap = SelectObject(dc, icon_);

	const RECT rect{0, 0, width, height};
	FillRect(dc, &rect, reinterpret_cast<HBRUSH>(COLOR_MENU + 1));
	DrawIconEx(dc, 0, 0, icon, width, height, 0, NULL, DI_NORMAL);

	SelectObject(dc, old_bitmap);
	DeleteDC(dc);
	ReleaseDC(NULL, screen);
}

Window_icon::Window_icon(Window_icon&& other) noexcept
{
	std::swap(icon_, other.icon_);
}

Window_icon& Window_icon::operator=(Window_icon&& other) noexcept
{
	std::swap(icon_, other.icon_);
	return *this;
}

Window_icon::~Window_icon()
{
	if (icon_ != NULL)
		DeleteObject(icon_);
}

HBITMAP Window_icon::Handle() const
{
	return icon_;
}

//////////////////////////////////////////////////////////////////////////

Window::Window(HWND wnd)
	: wnd_(wnd), icon_(Window_icon(wnd))
{
	auto len = GetWindowTextLength(wnd);
	GetWindowText(wnd, title_.GetBufferSetLength(len), len + 1);
	title_.ReleaseBuffer();

	if (title_.GetLength() > MAX_WINDOW_TITLE_LEN)
	{
		title_.Truncate(MAX_WINDOW_TITLE_LEN - 3);
		title_ += _T("...");
	}

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(wnd, &wp);
	is_minimized_ = (wp.showCmd == SW_SHOWMINIMIZED);

	GetWindowThreadProcessId(wnd, &pid_);
}

Window& Window::operator=(Window&& other) noexcept
{
	wnd_ = other.wnd_;
	pid_ = other.pid_;
	title_ = other.title_;
	is_minimized_ = other.is_minimized_;
	std::swap(icon_, other.icon_);
	return *this;
}

HWND Window::Handle() const
{
	return wnd_;
}

CString Window::Title() const
{
	return is_minimized_ ? ('*' + title_) : title_;
}

HBITMAP Window::IconHandle() const
{
	return icon_.Handle();
}

bool Window::operator<(const Window& other) const
{
	if (pid_ != other.pid_)
		return title_.CompareNoCase(other.title_) < 0;
	else
		return false;
}

//////////////////////////////////////////////////////////////////////////

bool is_modal_window(HWND wnd)
{
	const auto style = GetWindowLongPtr(wnd, GWL_STYLE);
	const auto ex_style = GetWindowLongPtr(wnd, GWL_EXSTYLE);

	return (style & DS_MODALFRAME) || (ex_style & WS_EX_DLGMODALFRAME);
}

bool is_valid_window(HWND wnd)
{
	const auto style = GetWindowLongPtr(wnd, GWL_STYLE);
	const auto ex_style = GetWindowLongPtr(wnd, GWL_EXSTYLE);

	return (style & WS_VISIBLE)
		&& (style & WS_MAXIMIZEBOX || style & WS_MINIMIZEBOX || style & WS_DLGFRAME)
		&& !(ex_style & WS_EX_TOOLWINDOW);
}

BOOL CALLBACK enum_windows_proc(HWND wnd, LPARAM windows_ptr)
{
	if (!is_valid_window(wnd))
		return TRUE;

	RECT rect;
	GetWindowRect(wnd, &rect);
	if (rect.right <= rect.left || rect.bottom <= rect.top)
		return TRUE;

	const auto windows = reinterpret_cast<std::vector<Window>*>(windows_ptr);
	windows->emplace_back(wnd);

	return TRUE;
}
