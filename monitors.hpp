#pragma once
#include "pch.hpp"
#include "windows.hpp"
#include "resource.h"

struct Monitor
{
	const HMONITOR handle;
	const CRect rect;
	const CString id;
	CString name;
};

extern std::vector<Monitor> monitors;

BOOL CALLBACK monitor_enum_proc(HMONITOR monitor, HDC, LPRECT, LPARAM monitors_ptr)
{
	MONITORINFOEX mi;
	mi.cbSize = sizeof(mi);
	if (!GetMonitorInfo(monitor, &mi))
		return TRUE;

	DISPLAY_DEVICE dd;
	dd.cb = sizeof(dd);
	if (!EnumDisplayDevices(mi.szDevice, 0, &dd, EDD_GET_DEVICE_INTERFACE_NAME))
		return TRUE;

	const auto monitors = reinterpret_cast<std::vector<Monitor>*>(monitors_ptr);
	monitors->push_back({monitor, mi.rcWork, dd.DeviceID, mi.szDevice});

	return TRUE;
}

bool enum_monitors()
{
	monitors.clear();
	if (!EnumDisplayMonitors(0, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(&monitors)))
		return false;

	UINT32 n_paths, n_modes;
	auto r = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &n_paths, &n_modes);
	if (r != ERROR_SUCCESS)
		return false;

	std::vector<DISPLAYCONFIG_PATH_INFO> display_paths(n_paths);
	std::vector<DISPLAYCONFIG_MODE_INFO> display_modes(n_modes);

	r = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &n_paths, display_paths.data(), &n_modes, display_modes.data(), NULL);
	if (r != ERROR_SUCCESS)
		return false;

	// Get friendly names
	for (const auto& mode : display_modes)
	{
		if (mode.infoType != DISPLAYCONFIG_MODE_INFO_TYPE_TARGET)
			continue;

		DISPLAYCONFIG_TARGET_DEVICE_NAME deviceName;
		DISPLAYCONFIG_DEVICE_INFO_HEADER header;
		header.size = sizeof(DISPLAYCONFIG_TARGET_DEVICE_NAME);
		header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
		header.adapterId = mode.adapterId;
		header.id = mode.id;
		deviceName.header = header;

		if (DisplayConfigGetDeviceInfo(&deviceName.header) != ERROR_SUCCESS ||
			wcslen(deviceName.monitorFriendlyDeviceName) == 0)
			continue;

		const auto m = std::find_if(monitors.begin(), monitors.end(),
			[&deviceName](auto& m) { return m.id == deviceName.monitorDevicePath; });

		if (m != monitors.end())
			m->name = deviceName.monitorFriendlyDeviceName;
	}

	return true;
}

void move_window_to_monitor(HWND wnd, const Monitor& dst_mon)
{
	const auto curr_mon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
	if (curr_mon == dst_mon.handle)
		return;

	WINDOWPLACEMENT wp;
	wp.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(wnd, &wp);

	MONITORINFO mi;
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(curr_mon, &mi);
	const CRect src_mon_rect = mi.rcWork;

	const auto& dst_rect = dst_mon.rect;
	CRect rect = wp.rcNormalPosition;
	rect -= src_mon_rect.TopLeft();

	if (!is_modal_window(wnd))
	{
		// Non-modal window: resize and move
		rect.SetRect(MulDiv(rect.left, dst_rect.Width(), src_mon_rect.Width()),
			MulDiv(rect.top, dst_rect.Height(), src_mon_rect.Height()),
			MulDiv(rect.right, dst_rect.Width(), src_mon_rect.Width()),
			MulDiv(rect.bottom, dst_rect.Height(), src_mon_rect.Height()));
	}
	else
	{
		// Modal frame: do not resize, just move
		rect.MoveToXY(MulDiv(rect.left, dst_rect.Width() - rect.Width(), src_mon_rect.Width() - rect.Width()),
			MulDiv(rect.top, dst_rect.Height() - rect.Height(), src_mon_rect.Height() - rect.Height()));
	}

	rect += dst_rect.TopLeft();
	wp.rcNormalPosition = rect;
	SetWindowPlacement(wnd, &wp);

	if (wp.showCmd == SW_SHOWMAXIMIZED)
	{
		ShowWindow(wnd, SW_RESTORE);
		ShowWindow(wnd, SW_MAXIMIZE);
	}
	else
		SetWindowPos(wnd, 0, rect.left, rect.top, rect.Width(), rect.Height(), SWP_NOZORDER);

	if (wp.showCmd == SW_SHOWMINIMIZED)
		ShowWindow(wnd, SW_RESTORE);
}
