#include "metrics.hpp"
#include "ui.hpp"
#include <cwctype>
#include <tlhelp32.h>
struct Process {
    DWORD pid;
    std::wstring name;
};
std::vector<Process> processes;
HWND list, search, summary, disks;
uint64_t oldIdle = 0, oldTotal = 0;
bool first = true, smoke = false;
double cpu = 0;
MEMORYSTATUSEX mem{sizeof(mem)};
std::wstring diskText;
uint64_t ticks(FILETIME f) {
    return (uint64_t(f.dwHighDateTime) << 32) | f.dwLowDateTime;
}
void render() {
    ListView_DeleteAllItems(list);
    auto query = text(search);
    std::transform(query.begin(), query.end(), query.begin(), towlower);
    int row = 0;
    for (auto &p : processes) {
        auto name = p.name;
        std::transform(name.begin(), name.end(), name.begin(), towlower);
        if (name.find(query) == std::wstring::npos &&
            std::to_wstring(p.pid).find(query) == std::wstring::npos)
            continue;
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = row;
        item.pszText = p.name.data();
        ListView_InsertItem(list, &item);
        auto pid = std::to_wstring(p.pid);
        ListView_SetItemText(list, row, 1, pid.data());
        ++row;
    }
}
void sample() {
    FILETIME i, k, u;
    if (!GetSystemTimes(&i, &k, &u))
        throw std::runtime_error("GetSystemTimes failed");
    auto idle = ticks(i), total = ticks(k) + ticks(u);
    cpu = first ? 0 : cpuUsage(oldIdle, oldTotal, idle, total);
    first = false;
    oldIdle = idle;
    oldTotal = total;
    if (!GlobalMemoryStatusEx(&mem))
        throw std::runtime_error("Memory sample failed");
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << L"CPU: " << cpu << L"%    RAM: "
        << gib(mem.ullTotalPhys - mem.ullAvailPhys) << L" / " << gib(mem.ullTotalPhys) << L" GiB ("
        << mem.dwMemoryLoad << L"%)";
    SetWindowTextW(summary, out.str().c_str());
    processes.clear();
    auto snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        throw std::runtime_error("Process snapshot failed");
    PROCESSENTRY32W p{};
    p.dwSize = sizeof(p);
    if (Process32FirstW(snap, &p)) {
        do {
            processes.push_back({p.th32ProcessID, p.szExeFile});
        } while (Process32NextW(snap, &p));
    }
    CloseHandle(snap);
    std::sort(processes.begin(), processes.end(), [](auto &a, auto &b) { return a.name < b.name; });
    DWORD drives = GetLogicalDrives();
    std::wostringstream disk;
    disk << std::fixed << std::setprecision(1);
    for (int d = 0; d < 26; ++d)
        if (drives & (1u << d)) {
            wchar_t root[] = {wchar_t(L'A' + d), L':', L'\\', 0};
            if (GetDriveTypeW(root) != DRIVE_FIXED)
                continue;
            ULARGE_INTEGER available, totalBytes, freeBytes;
            if (GetDiskFreeSpaceExW(root, &available, &totalBytes, &freeBytes))
                disk << root << L"  свободно " << gib(available.QuadPart) << L" / "
                     << gib(totalBytes.QuadPart) << L" GiB\r\n";
        }
    diskText = disk.str();
    SetWindowTextW(disks, diskText.c_str());
    render();
}
std::string report() {
    std::ostringstream s;
    s << "\xef\xbb\xbfSection,Metric,Value\r\n"
      << "System,CPU percent," << cpu << "\r\nSystem,RAM total bytes," << mem.ullTotalPhys
      << "\r\nSystem,RAM available bytes," << mem.ullAvailPhys << "\r\nDisks,Summary,"
      << csv(utf8(diskText)) << "\r\n";
    for (auto &p : processes)
        s << "Process," << csv(utf8(p.name)) << ',' << p.pid << "\r\n";
    return s.str();
}
LRESULT CALLBACK proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    try {
        switch (msg) {
        case WM_CREATE: {
            smoke = wcsstr(GetCommandLineW(), L"--smoke") != nullptr;
            control(w, L"STATIC", L"SYSTEMSCOPE  /  Ресурсы • процессы • диски", 0, 20, 20, 920,
                    30);
            summary = control(w, L"STATIC", L"Первый замер CPU появится через 2 секунды", 0, 20, 65,
                              920, 30);
            control(w, L"STATIC", L"Поиск по имени процесса или PID", 0, 20, 110, 430, 25);
            search = control(w, L"EDIT", L"", 10, 20, 140, 600, 30,
                             WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP);
            control(w, L"BUTTON", L"Обновить", 1, 645, 140, 140, 30, WS_TABSTOP);
            control(w, L"BUTTON", L"Экспорт CSV", 2, 805, 140, 140, 30, WS_TABSTOP);
            list = control(w, WC_LISTVIEWW, L"", 11, 20, 190, 925, 330,
                           WS_BORDER | LVS_REPORT | WS_TABSTOP);
            ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
            LVCOLUMNW col{};
            col.mask = LVCF_TEXT | LVCF_WIDTH;
            col.cx = 700;
            col.pszText = (LPWSTR)L"Процесс";
            ListView_InsertColumn(list, 0, &col);
            col.cx = 170;
            col.pszText = (LPWSTR)L"PID";
            ListView_InsertColumn(list, 1, &col);
            control(w, L"STATIC", L"Локальные диски", 0, 20, 540, 400, 25);
            disks = control(w, L"EDIT", L"", 12, 20, 575, 925, 90,
                            ES_MULTILINE | ES_READONLY | WS_VSCROLL);
            sample();
            SetTimer(w, 1, 2000, nullptr);
            if (smoke)
                SetTimer(w, 2, 4500, nullptr);
            return 0;
        }
        case WM_TIMER:
            if (wp == 2) {
                if (processes.empty() || mem.ullTotalPhys == 0 || report().empty()) {
                    PostQuitMessage(1);
                    return 0;
                }
                DestroyWindow(w);
                return 0;
            }
            sample();
            return 0;
        case WM_COMMAND:
            if (LOWORD(wp) == 10 && HIWORD(wp) == EN_CHANGE) {
                render();
                return 0;
            }
            if (LOWORD(wp) == 1) {
                sample();
                return 0;
            }
            if (LOWORD(wp) == 2) {
                auto path = savePath(w);
                if (!path.empty())
                    atomicWrite(path, report());
                return 0;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception &e) {
        if (smoke) {
            PostQuitMessage(1);
            return 0;
        }
        MessageBoxW(w, wide(e.what()).c_str(), L"Ошибка получения данных", MB_OK | MB_ICONERROR);
        if (msg == WM_CREATE)
            return -1;
    }
    return DefWindowProcW(w, msg, wp, lp);
}
int WINAPI wWinMain(HINSTANCE h, HINSTANCE, PWSTR, int) {
    return runApp(h, L"SystemScope", proc);
}
