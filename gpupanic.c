/*
 * gpupanic - trigger gpu crash
 * https://github.com/Tymbark7372/gpupanic
 * 
 * made by Tymbark7372
 * MIT License
 */

#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#define _CRT_SECURE_NO_WARNINGS

#include <initguid.h>

#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define MODE_SAFE     0
#define MODE_MEDIUM   1
#define MODE_NUCLEAR  2

// infinite loop shader - gpu will never finish this
static const char* SHADER_INFINITE = 
    "RWStructuredBuffer<uint> output : register(u0);\n"
    "[numthreads(1024, 1, 1)]\n"
    "void main(uint3 id : SV_DispatchThreadID) {\n"
    "    uint val = id.x;\n"
    "    [loop] while(true) {\n"
    "        val = val * 1103515245 + 12345;\n"
    "        output[id.x % 1024] = val;\n"
    "    }\n"
    "}\n";

// timed loop shader - hangs for a bit then finishes
static const char* SHADER_TIMED =
    "RWStructuredBuffer<uint> output : register(u0);\n"
    "cbuffer Constants : register(b0) { uint iterations; };\n"
    "[numthreads(1024, 1, 1)]\n"
    "void main(uint3 id : SV_DispatchThreadID) {\n"
    "    uint val = id.x;\n"
    "    for(uint i = 0; i < iterations; i++) {\n"
    "        val = val * 1103515245 + 12345;\n"
    "        output[id.x % 1024] = val;\n"
    "    }\n"
    "}\n";

static BOOL disable_tdr(void) {
    HKEY hKey;
    DWORD value = 0;
    LONG result;
    
    result = RegCreateKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL
    );
    
    if (result != ERROR_SUCCESS) {
        printf("  [!] failed to open registry (run as admin)\n");
        return FALSE;
    }
    
    result = RegSetValueExA(hKey, "TdrLevel", 0, REG_DWORD, (BYTE*)&value, sizeof(value));
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        printf("  [!] failed to set TdrLevel\n");
        return FALSE;
    }
    
    printf("  [+] TDR disabled - reboot for it to take effect\n");
    printf("  [!] after reboot, gpu hangs wont recover automatically\n");
    return TRUE;
}

static BOOL enable_tdr(void) {
    HKEY hKey;
    LONG result;
    
    result = RegOpenKeyExA(
        HKEY_LOCAL_MACHINE,
        "SYSTEM\\CurrentControlSet\\Control\\GraphicsDrivers",
        0, KEY_SET_VALUE, &hKey
    );
    
    if (result != ERROR_SUCCESS) return FALSE;
    
    RegDeleteValueA(hKey, "TdrLevel");
    RegCloseKey(hKey);
    
    printf("  [+] TDR restored - reboot for it to take effect\n");
    return TRUE;
}

static IDXGIAdapter* find_nvidia_adapter(void) {
    IDXGIFactory* factory = NULL;
    IDXGIAdapter* adapter = NULL;
    IDXGIAdapter* nvidia_adapter = NULL;
    DXGI_ADAPTER_DESC desc;
    UINT i = 0;
    
    if (FAILED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory))) {
        printf("  [!] failed to create DXGI factory\n");
        return NULL;
    }
    
    printf("\n  GPUs found:\n");
    printf("  -----------\n");
    
    while (IDXGIFactory_EnumAdapters(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        IDXGIAdapter_GetDesc(adapter, &desc);
        printf("  [%d] %ws\n", i, desc.Description);
        
        if (wcsstr(desc.Description, L"NVIDIA") != NULL) {
            nvidia_adapter = adapter;
            printf("       ^ this one will die\n");
        } else {
            IDXGIAdapter_Release(adapter);
        }
        i++;
    }
    
    IDXGIFactory_Release(factory);
    
    if (nvidia_adapter == NULL) {
        printf("\n  [!] no NVIDIA gpu found\n");
    }
    
    return nvidia_adapter;
}

static ID3DBlob* compile_shader(const char* source, const char* entry, const char* target) {
    ID3DBlob* blob = NULL;
    ID3DBlob* errors = NULL;
    
    HRESULT hr = D3DCompile(
        source, strlen(source),
        NULL, NULL, NULL,
        entry, target,
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        &blob, &errors
    );
    
    if (FAILED(hr)) {
        if (errors) {
            printf("  [!] shader error: %s\n", (char*)ID3D10Blob_GetBufferPointer(errors));
            ID3D10Blob_Release(errors);
        }
        return NULL;
    }
    
    return blob;
}

static int trigger_panic(int mode) {
    IDXGIAdapter* adapter = NULL;
    ID3D11Device* device = NULL;
    ID3D11DeviceContext* context = NULL;
    ID3D11ComputeShader* shader = NULL;
    ID3D11Buffer* buffer = NULL;
    ID3D11Buffer* cbuffer = NULL;
    ID3D11UnorderedAccessView* uav = NULL;
    ID3DBlob* blob = NULL;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr;
    
    printf("\n  +------------------------------------------+\n");
    printf("  |            GPU PANIC LOADING             |\n");
    printf("  +------------------------------------------+\n");
    
    adapter = find_nvidia_adapter();
    if (!adapter) {
        printf("\n  [!] need an nvidia gpu to continue\n");
        return 1;
    }
    
    printf("\n  [*] creating d3d11 device...\n");
    
    hr = D3D11CreateDevice(
        adapter,
        D3D_DRIVER_TYPE_UNKNOWN,
        NULL,
        0,
        NULL, 0,
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context
    );
    
    IDXGIAdapter_Release(adapter);
    
    if (FAILED(hr)) {
        printf("  [!] d3d11 device creation failed: 0x%08X\n", hr);
        return 1;
    }
    
    printf("  [+] device created\n");
    printf("  [*] compiling shader...\n");
    
    if (mode == MODE_SAFE || mode == MODE_MEDIUM) {
        blob = compile_shader(SHADER_TIMED, "main", "cs_5_0");
    } else {
        blob = compile_shader(SHADER_INFINITE, "main", "cs_5_0");
    }
    
    if (!blob) {
        printf("  [!] shader compilation failed\n");
        goto cleanup;
    }
    
    printf("  [+] shader ready\n");
    
    hr = ID3D11Device_CreateComputeShader(
        device,
        ID3D10Blob_GetBufferPointer(blob),
        ID3D10Blob_GetBufferSize(blob),
        NULL,
        &shader
    );
    
    ID3D10Blob_Release(blob);
    
    if (FAILED(hr)) {
        printf("  [!] failed to create compute shader\n");
        goto cleanup;
    }
    
    D3D11_BUFFER_DESC bufDesc = {
        .ByteWidth = 1024 * sizeof(UINT),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_UNORDERED_ACCESS,
        .MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
        .StructureByteStride = sizeof(UINT)
    };
    
    hr = ID3D11Device_CreateBuffer(device, &bufDesc, NULL, &buffer);
    if (FAILED(hr)) {
        printf("  [!] buffer creation failed\n");
        goto cleanup;
    }
    
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
        .Format = DXGI_FORMAT_UNKNOWN,
        .ViewDimension = D3D11_UAV_DIMENSION_BUFFER,
        .Buffer = { .FirstElement = 0, .NumElements = 1024 }
    };
    
    hr = ID3D11Device_CreateUnorderedAccessView(device, (ID3D11Resource*)buffer, &uavDesc, &uav);
    if (FAILED(hr)) {
        printf("  [!] UAV creation failed\n");
        goto cleanup;
    }
    
    if (mode == MODE_SAFE || mode == MODE_MEDIUM) {
        D3D11_BUFFER_DESC cbDesc = {
            .ByteWidth = 16,
            .Usage = D3D11_USAGE_DYNAMIC,
            .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
            .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
        };
        
        hr = ID3D11Device_CreateBuffer(device, &cbDesc, NULL, &cbuffer);
        if (FAILED(hr)) {
            printf("  [!] constant buffer failed\n");
            goto cleanup;
        }
        
        D3D11_MAPPED_SUBRESOURCE mapped;
        ID3D11DeviceContext_Map(context, (ID3D11Resource*)cbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        
        UINT* iterations = (UINT*)mapped.pData;
        if (mode == MODE_SAFE) {
            *iterations = 50000000;
        } else {
            *iterations = 500000000;
        }
        
        ID3D11DeviceContext_Unmap(context, (ID3D11Resource*)cbuffer, 0);
        ID3D11DeviceContext_CSSetConstantBuffers(context, 0, 1, &cbuffer);
    }
    
    ID3D11DeviceContext_CSSetShader(context, shader, NULL, 0);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(context, 0, 1, &uav, NULL);
    
    printf("\n  +------------------------------------------+\n");
    
    if (mode == MODE_NUCLEAR) {
        printf("  |          !!! NUCLEAR MODE !!!            |\n");
        printf("  |      only hard reboot will save you      |\n");
    } else if (mode == MODE_MEDIUM) {
        printf("  |             MEDIUM MODE                  |\n");
        printf("  |        ~10-20 sec hang, recovers         |\n");
    } else {
        printf("  |              SAFE MODE                   |\n");
        printf("  |         ~2 sec hang, recovers            |\n");
    }
    
    printf("  +------------------------------------------+\n\n");
    
    printf("  [*] sending shader to gpu...\n");
    printf("  [*] 3...\n"); Sleep(1000);
    printf("  [*] 2...\n"); Sleep(1000);
    printf("  [*] 1...\n"); Sleep(1000);
    printf("\n  >>> GPU GO BRRRR <<<\n\n");
    
    ID3D11DeviceContext_Dispatch(context, 65535, 1, 1);
    ID3D11DeviceContext_Flush(context);
    
    if (mode == MODE_NUCLEAR) {
        printf("  [*] gpu is gone. fans go crazy. goodbye.\n");
        
        ID3D11Query* query = NULL;
        D3D11_QUERY_DESC queryDesc = { .Query = D3D11_QUERY_EVENT };
        ID3D11Device_CreateQuery(device, &queryDesc, &query);
        ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)query);
        
        BOOL complete = FALSE;
        while (!complete) {
            ID3D11DeviceContext_GetData(context, (ID3D11Asynchronous*)query, &complete, sizeof(complete), 0);
            Sleep(100);
        }
        
        if (query) ID3D11Query_Release(query);
    } else {
        printf("  [*] gpu is working... waiting\n");
        Sleep(mode == MODE_SAFE ? 3000 : 25000);
        printf("  [+] should be back now (TDR saved you)\n");
    }
    
cleanup:
    printf("\n  [*] cleaning up...\n");
    
    if (uav) ID3D11UnorderedAccessView_Release(uav);
    if (buffer) ID3D11Buffer_Release(buffer);
    if (cbuffer) ID3D11Buffer_Release(cbuffer);
    if (shader) ID3D11ComputeShader_Release(shader);
    if (context) ID3D11DeviceContext_Release(context);
    if (device) ID3D11Device_Release(device);
    
    printf("  [+] done\n");
    return 0;
}

static void print_banner(void) {
    printf("\n");
    printf("  +------------------------------------------+\n");
    printf("  |       gpupanic - make your gpu cry       |\n");
    printf("  |           made by Tymbark7372            |\n");
    printf("  +------------------------------------------+\n");
}

static void print_help(void) {
    printf("\n");
    printf("  usage: gpupanic.exe [mode]\n\n");
    printf("  modes:\n");
    printf("    --safe        ~2 sec hang, auto-recovers\n");
    printf("    --medium      ~20 sec hang, auto-recovers\n");
    printf("    --nuclear     infinite hang, need hard reboot\n");
    printf("\n");
    printf("  other:\n");
    printf("    --disable-tdr   turn off TDR (needs admin + reboot)\n");
    printf("    --enable-tdr    turn TDR back on\n");
    printf("    --list          show your gpus\n");
    printf("    --help          this\n");
    printf("\n");
    printf("  tip: plug monitor into motherboard (igpu) so\n");
    printf("       screen stays on when nvidia dies\n");
    printf("\n");
}

static void list_gpus(void) {
    IDXGIFactory* factory = NULL;
    IDXGIAdapter* adapter = NULL;
    DXGI_ADAPTER_DESC desc;
    UINT i = 0;
    
    if (FAILED(CreateDXGIFactory(&IID_IDXGIFactory, (void**)&factory))) {
        printf("  [!] cant list gpus\n");
        return;
    }
    
    printf("\n  your gpus:\n");
    printf("  ----------\n");
    
    while (IDXGIFactory_EnumAdapters(factory, i, &adapter) != DXGI_ERROR_NOT_FOUND) {
        IDXGIAdapter_GetDesc(adapter, &desc);
        
        printf("  [%d] %ws\n", i, desc.Description);
        printf("      vram: %zu MB\n", desc.DedicatedVideoMemory / (1024*1024));
        
        if (wcsstr(desc.Description, L"NVIDIA") != NULL) {
            printf("      ^ TARGET\n");
        } else if (wcsstr(desc.Description, L"AMD") != NULL || 
                   wcsstr(desc.Description, L"Radeon") != NULL) {
            printf("      ^ safe (display stays here)\n");
        } else if (wcsstr(desc.Description, L"Intel") != NULL) {
            printf("      ^ safe (display stays here)\n");
        }
        printf("\n");
        
        IDXGIAdapter_Release(adapter);
        i++;
    }
    
    IDXGIFactory_Release(factory);
}

int main(int argc, char* argv[]) {
    int mode = -1;
    
    print_banner();
    
    if (argc < 2) {
        print_help();
        return 0;
    }
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--safe") == 0) {
            mode = MODE_SAFE;
        } else if (strcmp(argv[i], "--medium") == 0) {
            mode = MODE_MEDIUM;
        } else if (strcmp(argv[i], "--nuclear") == 0) {
            mode = MODE_NUCLEAR;
        } else if (strcmp(argv[i], "--disable-tdr") == 0) {
            disable_tdr();
            return 0;
        } else if (strcmp(argv[i], "--enable-tdr") == 0) {
            enable_tdr();
            return 0;
        } else if (strcmp(argv[i], "--list") == 0) {
            list_gpus();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help();
            return 0;
        } else {
            printf("  [!] unknown: %s\n", argv[i]);
            print_help();
            return 1;
        }
    }
    
    if (mode == -1) {
        print_help();
        return 0;
    }
    
    if (mode == MODE_NUCLEAR) {
        printf("\n  !!! NUCLEAR MODE !!!\n");
        printf("  your nvidia gpu will freeze until you hard reboot.\n");
        printf("  make sure display is on igpu or remote access ready.\n\n");
        printf("  type 'PANIC' to continue: ");
        
        char confirm[16];
        if (scanf("%15s", confirm) != 1 || strcmp(confirm, "PANIC") != 0) {
            printf("\n  aborted.\n");
            return 0;
        }
    }
    
    return trigger_panic(mode);
}
