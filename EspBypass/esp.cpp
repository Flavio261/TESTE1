// EspBypass.dll - FASE 5 micro-passo 1: vtable hook EndScene + log
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>

#pragma comment(lib, "d3d9.lib")

typedef HRESULT(WINAPI* EndScene_t)(IDirect3DDevice9*);
static EndScene_t   oEndScene = nullptr;
static volatile LONG g_frames = 0;

static void logline(const char* fmt, ...) {
    char buf[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    OutputDebugStringA(buf);
    FILE* f = nullptr;
    if (fopen_s(&f, "C:\\kit\\esp_log.txt", "a") == 0 && f) {
        SYSTEMTIME st; GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, buf);
        fclose(f);
    }
}

static HRESULT WINAPI hkEndScene(IDirect3DDevice9* dev) {
    LONG n = InterlockedIncrement(&g_frames);
    if (n == 1)         logline("[esp] primeiro EndScene, device=0x%p", dev);
    if (n % 300 == 0)   logline("[esp] frame=%ld device=0x%p", n, dev);
    return oEndScene(dev);
}

static bool install_hook() {
    HMODULE h = GetModuleHandleA("d3d9.dll");
    if (!h) { logline("[esp] d3d9.dll nao carregada"); return false; }
    logline("[esp] d3d9.dll base=0x%p", (void*)h);

    typedef IDirect3D9* (WINAPI* D3DCreate9_t)(UINT);
    D3DCreate9_t pCreate = (D3DCreate9_t)GetProcAddress(h, "Direct3DCreate9");
    if (!pCreate) { logline("[esp] Direct3DCreate9 nao exportado"); return false; }

    IDirect3D9* d3d = pCreate(D3D_SDK_VERSION);
    if (!d3d) { logline("[esp] Direct3DCreate9 retornou null"); return false; }

    HWND hwnd = CreateWindowA("STATIC", "esp_dummy", WS_POPUP,
                              0, 0, 1, 1, nullptr, nullptr, nullptr, nullptr);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = hwnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
        &pp, &dev);
    if (FAILED(hr)) {
        logline("[esp] CreateDevice falhou hr=0x%08X", hr);
        d3d->Release(); DestroyWindow(hwnd); return false;
    }

    void** vt = *reinterpret_cast<void***>(dev);
    oEndScene = reinterpret_cast<EndScene_t>(vt[42]);
    logline("[esp] vtable=0x%p  EndScene_orig=0x%p", vt, (void*)oEndScene);

    DWORD oldp = 0;
    if (!VirtualProtect(&vt[42], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldp)) {
        logline("[esp] VirtualProtect falhou err=%lu", GetLastError());
    } else {
        vt[42] = reinterpret_cast<void*>(&hkEndScene);
        VirtualProtect(&vt[42], sizeof(void*), oldp, &oldp);
        logline("[esp] hook instalado em vtable[42]");
    }

    dev->Release();
    d3d->Release();
    DestroyWindow(hwnd);
    return true;
}

static DWORD WINAPI main_thread(LPVOID) {
    logline("[esp] EspBypass injetada, pid=%lu", GetCurrentProcessId());
    logline("[esp] install_hook -> %s", install_hook() ? "OK" : "FALHOU");
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(nullptr);
        CreateThread(nullptr, 0, main_thread, nullptr, 0, nullptr);
    }
    return TRUE;
}