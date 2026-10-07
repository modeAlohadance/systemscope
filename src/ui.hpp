#pragma once
#define NOMINMAX
#include <algorithm>
#include <commctrl.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
inline std::string utf8(const std::wstring &s) {
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
    return r;
}
inline std::wstring wide(const std::string &s) {
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring r(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n);
    return r;
}
inline std::wstring text(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n + 1, 0);
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(n);
    return s;
}
inline HWND control(HWND p, const wchar_t *cls, const wchar_t *label, int id, int x, int y, int w,
                    int h, DWORD style = 0) {
    auto c = CreateWindowExW(0, cls, label, WS_CHILD | WS_VISIBLE | style, x, y, w, h, p,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(c, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    return c;
}
inline std::filesystem::path savePath(HWND owner) {
    wchar_t path[32768] = L"report.csv";
    OPENFILENAMEW o{};
    o.lStructSize = sizeof(o);
    o.hwndOwner = owner;
    o.lpstrFilter = L"CSV (*.csv)\0*.csv\0\0";
    o.lpstrFile = path;
    o.nMaxFile = 32768;
    o.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    o.lpstrDefExt = L"csv";
    return GetSaveFileNameW(&o) ? std::filesystem::path(path) : std::filesystem::path();
}
inline std::string csv(const std::string &s) {
    std::string r = "\"";
    for (char c : s) {
        if (c == '\"')
            r += '\"';
        r += c;
    }
    return r + '\"';
}
inline void atomicWrite(const std::filesystem::path &p, const std::string &data) {
    auto tmp = p;
    tmp += L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f || !f.write(data.data(), data.size()) || !(f.flush()))
            throw std::runtime_error("Cannot save file");
    }
    if (!MoveFileExW(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace file");
}
inline int runApp(HINSTANCE instance, const wchar_t *title, WNDPROC proc) {
    INITCOMMONCONTROLSEX c{sizeof(c), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&c);
    WNDCLASSW wc{};
    wc.lpfnWndProc = proc;
    wc.hInstance = instance;
    wc.lpszClassName = title;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassW(&wc);
    auto w =
        CreateWindowW(title, title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                      CW_USEDEFAULT, CW_USEDEFAULT, 1000, 730, nullptr, nullptr, instance, nullptr);
    if (!w)
        return 1;
    ShowWindow(w, SW_SHOW);
    MSG m{};
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    return (int)m.wParam;
}
