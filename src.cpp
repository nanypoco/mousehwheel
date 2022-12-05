#include <windows.h>
#include <commctrl.h>
#include <winuser.h>
#include "aviutl/filter.h"
#include <iostream>

const TCHAR* track_name[] = { "�����x","�c臒l","�Y�[��臒l" };
int track_default[] = { 100,20,20 };
int track_s[] = { 20,1,1 };
int track_e[] = { +1000,+200,+200 };
constexpr auto track_n = 3;

const TCHAR* check_name[] = { "���z�C�[���Ή�","alt����ւ�","���x/臒l�ύX","�ݒ��ۑ�"};
int check_default[] = { 0,0,0,-1 };
constexpr auto check_n = 4;

FILTER_DLL filter = {
    FILTER_FLAG_ALWAYS_ACTIVE,
    NULL,NULL,
    const_cast<char*>("�}�E�X���z�C�[��+"),
    track_n, (TCHAR**)track_name, track_default,
    track_s, track_e,
    check_n, (TCHAR**)check_name, check_default,
    NULL,
    func_init, func_exit, func_update,
    func_WndProc,
    NULL,NULL,
    NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,NULL,NULL,
    NULL, NULL, func_project_save,
};
EXTERN_C FILTER_DLL __declspec(dllexport)* __stdcall GetFilterTable(void) {
    return &filter;
}


static int exedit_base;
BOOL(*exedit_func_WndProc)(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp);
static int scroll_rate_w;       // �����x
static int scroll_rate_h;       // �c�������l
static int scroll_rate_zoom;    // �Y�[���������l
static int hwheel_accumulation; // �c�̈ړ��ʗ݌v
static int zoom_accumulation;   // ctrl+�z�C�[���̈ړ��ʗ݌v
static int enable_hwheel;       // ���z�C�[����L���ɂ��邩
static int enable_exchange_alt; // Alt�̓�������ւ��邩
static int enable_change_rate;  // ���x/�������l�̐ݒ�/�ύX��L���ɂ��邩


FILTER* get_exeditfp(FILTER* fp) {
    SYS_INFO si;
    fp->exfunc->get_sys_info(NULL, &si);

    for (int i = 0; i < si.filter_n; i++) {
        FILTER* tfp = (FILTER*)fp->exfunc->get_filterp(i);
        if (tfp->information != NULL) {
            if (!strcmp(tfp->information, "�g���ҏW(exedit) version 0.92 by �j�d�m����")) return tfp;
        }
    }
    return NULL;
}


BOOL exedit_Replace8(int exedit_address, byte new_8) {
    DWORD oldProtect;
    byte* address = (byte*)(exedit_address + exedit_base);
    if (!VirtualProtect(address, 1, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return FALSE;
    }
    *address = new_8;
    return VirtualProtect(address, 1, oldProtect, &oldProtect);
}


BOOL exedit_func_WndProc_wrap(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, void* fp) {
    if (message == WM_MOUSEHWHEEL && enable_hwheel == 1) {
        if (((FILTER*)fp)->exfunc->is_saving(editp) == 0) {
            if ((wparam & 8) == 0) {
                int wheel_right = (short)(wparam >> 0x10);
                if (wheel_right != 0) {
                    int* timeline_zoom = (int*)(exedit_base + 0xa3fc8);
                    int* timeline_pos = (int*)(exedit_base + 0x1a52f0);
                    // ���x���L�����ǂ����؂�ւ���
                    if (enable_change_rate == 1) {
                        reinterpret_cast<void(__cdecl*)(int)>(exedit_base + 0x38c70)(*timeline_pos + (int)(wheel_right * scroll_rate_w * 100 / *timeline_zoom));
                    }
                    else {
                        reinterpret_cast<void(__cdecl*)(int)>(exedit_base + 0x38c70)(*timeline_pos + (int)(((0 < wheel_right) - 1 & 0xffffff38) + 100) * 10000 / *timeline_zoom);
                    }
                    return 0;
                }
            }
        }
    }
    return exedit_func_WndProc(hwnd, message, wparam, lparam, editp, fp);
}


// �z�C�[���ړ��ʂ�ۑ����Ă������l�𒴂�����X�N���[������
int hscroll_threshold(int wheel) {
    // �ݒ肪�����Ȃ�P��1���C���[���ړ�������(�f�t�H���g����Ɠ���)
    if (enable_change_rate == 0 || scroll_rate_h == 0) { return (wheel < 0) * 2 - 1; }

    // �O��Ƌt�����Ȃ�l�����Z�b�g
    if (hwheel_accumulation < 0 != wheel < 0) {
        hwheel_accumulation = 0;
    }
    hwheel_accumulation += wheel;

    // �~�ϒl���������l�𒴂�����ړ�������
    if (scroll_rate_h < abs(hwheel_accumulation)) {
        int hscroll = hwheel_accumulation / scroll_rate_h; // �ړ���
        hwheel_accumulation %= scroll_rate_h;
        return -hscroll;
    }
    return 0;
}


// ctrl+�z�C�[���ړ��ʂ�ۑ����Ă������l�𒴂�����g�嗦��ύX����
int zoom_threshold(int wheel) {
    // �ݒ肪�����Ȃ�P��1�ړ�������(�f�t�H���g����Ɠ���)
    if (enable_change_rate == 0 || scroll_rate_zoom == 0) { return (wheel > 0) * 2 - 1; }

    // �O��Ƌt�����Ȃ�l�����Z�b�g
    if (zoom_accumulation < 0 != wheel < 0) {
        zoom_accumulation = 0;
    }
    zoom_accumulation += wheel;

    // �~�ϒl���������l�𒴂�����ړ�������
    if (scroll_rate_zoom < abs(zoom_accumulation)) {
        int zoom = zoom_accumulation / scroll_rate_zoom; // �ړ���
        zoom_accumulation %= scroll_rate_zoom;
        return zoom;
    }
    return 0;
}


BOOL func_init(FILTER* fp) {
    FILTER* exeditfp = get_exeditfp(fp);
    if (exeditfp == NULL) {
        MessageBoxA(fp->hwnd, "�g���ҏW0.92��������܂���ł���", fp->name, MB_OK);
        return TRUE;
    }
    exedit_base = (int)exeditfp->dll_hinst;

    // aviutl.ini����ݒ��ǂ�
    fp->track[0] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_w", fp->track_default[0]);
    fp->track[1] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_h", fp->track_default[1]);
    fp->track[2] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_zoom", fp->track_default[2]);
    fp->check[0] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_hwheel", fp->check_default[0]);
    fp->check[1] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_exchange_alt", fp->check_default[1]);
    fp->check[2] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_change_rate", fp->check_default[2]);
    scroll_rate_w = fp->track[0];
    scroll_rate_h = fp->track[1];
    scroll_rate_zoom = fp->track[2];
    enable_hwheel = fp->check[0];
    enable_exchange_alt = fp->check[1];
    enable_change_rate = fp->check[2];

    hwheel_accumulation = 0;
    zoom_accumulation = 0;

    // �c�z�C�[�����c�X�N���[���ɁAAlt+�c�z�C�[�������X�N���[���ɂ���
    if (enable_exchange_alt == 1) {
        exedit_Replace8(0x3decd, 0x7c);
    }

    // ���z�C�[���ɑΉ�
    exedit_func_WndProc = exeditfp->func_WndProc;
    exeditfp->func_WndProc = exedit_func_WndProc_wrap;


    // �c�����̃X�N���[���ɕK�v�ȃz�C�[���ړ��ʂ�ݒ�
    static char executable_memory[] =
        "\x50"          // push eax
        "\xe8XXXX"      // call &hscroll_threshold
        "\x83\xc4\x04"  // add esp,4
        "\x33\xd2"      // xor edx,edx
        "\x85\xc0"      // test eax,eax
        "\x0f\x84XXXX"  // jz exedit_base + 0x43b4c // 0���Ԃ�����ړ����Ȃ�
        "\xe9XXXX"      // jmp exedit_base + 0x3dee4
        ;
    { // �ړ��揑������
        uint32_t ptr;
        ptr = (uint32_t)(&hscroll_threshold) - (uint32_t)(&executable_memory) - 6;
        memcpy(executable_memory + 2, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x43b4c - ((uint32_t)executable_memory + 15 + 4));
        memcpy(executable_memory + 15, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x3dee4 - ((uint32_t)executable_memory + 20 + 4));
        memcpy(executable_memory + 20, &ptr, 4);
    }
    DWORD oldProtect;
    VirtualProtect(&executable_memory, sizeof(executable_memory), PAGE_EXECUTE_READWRITE, &oldProtect);
    { // �W�����v������
        char code[] = "\xe9XXXX";
        uint32_t ptr = (uint32_t)&executable_memory - (exedit_base + 0x3ded9) - 5;
        memcpy_s(code + 1, 4, (void*)&ptr, 4);
        for (int i = 0; i < sizeof(code) - 1; i++) {
            exedit_Replace8(0x3ded9 + i, code[i]);
        }
    }


    // �^�C�����C���̃Y�[���ɕK�v�ȃz�C�[���ړ��ʂ�ݒ�
    static char executable_memory1[] =
        "\x50"          // push eax
        "\xe8XXXX"      // call &zoom_threshold
        "\x83\xc4\x04"  // add esp,4
        "\x85\xc0"      // test eax,eax
        "\x0f\x84XXXX"  // jz exedit_base + 0x43b4c // 0���Ԃ�����ړ����Ȃ�
        "\xe9XXXX"      // jmp exedit_base + 0x3df6b
        ;
    { // �ړ��揑������
        uint32_t ptr;
        ptr = (uint32_t)(&zoom_threshold) - (uint32_t)(&executable_memory1) - 6;
        memcpy(executable_memory1 + 2, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x43b4c - ((uint32_t)executable_memory1 + 13 + 4));
        memcpy(executable_memory1 + 13, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x3df6b - ((uint32_t)executable_memory1 + sizeof(executable_memory1) - 5 + 4));
        memcpy(executable_memory1 + sizeof(executable_memory1) - 5, &ptr, 4);
    }
    VirtualProtect(&executable_memory1, sizeof(executable_memory1), PAGE_EXECUTE_READWRITE, &oldProtect);
    { // �W�����v������
        char code[] =
            "\xe9XXXX"
            "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
            "\x8b\x15\x04\x53\x1a\x10"  // mov edx,dword ptr [DAT_101a5304]
            "\x52"                      // push edx
            "\x03\x05\xc4\x3f\x0a\x10"  // add eax,dword ptr [DAT_100a3fc4]
            "\x50"                      // push eax
            ;
        uint32_t ptr = (uint32_t)&executable_memory1 - (exedit_base + 0x3df5c) - 5;
        memcpy_s(code + 1, 4, (void*)&ptr, 4);
        for (int i = 0; i < sizeof(code) - 1; i++) {
            exedit_Replace8(0x3df5c + i, code[i]);
        }
    }


    // ���X�N���[���̌Œ�l����߂Ċ��x��ݒ肷��
    //exedit_Replace8(0x3df2a, 0x2);  // *10000��*100�ɂ���1
    {
        static char executable_memory2[] =
            "\x8b\x0dXXXX"              // mov ecx,DWORD PTR[&enable_change_rate]
            "\x85\xc9"                  // test ecx,ecx
            "\x74\x0f"                  // jz default:
            "\x0f\xaf\x05XXXX"          // imul eax,DWORD PTR[&scroll_rate_w] // �ړ���*rate*100
            "\x6b\xc0\x64"              // imul eax,eax,100
            "\xe9XXXX"                  // jmp exedit_base + 0x3df2b
                                    // default: // ���̏���
            "\x85\xc0"                  // test eax,eax
            "\x0f\x9e\xc2"              // setle dl
            "\xe9XXXX"                  // jmp exedit_base + 0x3df13
            ;
        uint32_t ptr;
        ptr = (uint32_t)(&enable_change_rate);
        memcpy(executable_memory2 + 2, &ptr, 4);
        ptr = (uint32_t)(&scroll_rate_w);
        memcpy(executable_memory2 + 13, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x3df2b - ((uint32_t)executable_memory2 + 21 + 4));
        memcpy(executable_memory2 + 21, &ptr, 4);
        ptr = (uint32_t)(exedit_base + 0x3df13 - ((uint32_t)executable_memory2 + sizeof(executable_memory2) - 5 + 4));
        memcpy(executable_memory2 + sizeof(executable_memory2) - 5, &ptr, 4);

        DWORD oldProtect;
        VirtualProtect(&executable_memory2, sizeof(executable_memory2), PAGE_EXECUTE_READWRITE, &oldProtect);

        char code[] = "\xe9XXXX";
        ptr = (uint32_t)&executable_memory2 - (exedit_base + 0x3df0e) - 5;
        memcpy_s(code + 1, 4, (void*)&ptr, 4);
        for (int i = 0; i < sizeof(code) - 1; i++) {
            exedit_Replace8(0x3df0e + i, code[i]);
        }
    }

#if 0
    // �e�X�g�p������
    RAWINPUTDEVICE device[1];
    device[0].usUsagePage = 0x01;	// �}�E�X�p�̒萔
    device[0].usUsage = 0x02;		// �}�E�X�p�̒萔
    device[0].dwFlags = RIDEV_EXINPUTSINK;
    device[0].hwndTarget = fp->hwnd;
    RegisterRawInputDevices(device, 1, sizeof(device[0]));
#endif

    return TRUE;
}


BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void* editp, FILTER* fp) {
    switch (message) {
    case WM_FILTER_COMMAND:
    case WM_COMMAND:
        // �ۑ��{�^���������ꂽ
        if (LOWORD(wparam) == MID_FILTER_BUTTON + 3) {
            func_exit(fp);
        }
        break;

#if 0
    case WM_INPUT:

        UINT dwSize = 0;
        RAWINPUTHEADER head;
        RAWMOUSE mouse;
        GetRawInputData((HRAWINPUT)lparam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

        unsigned char* lpb = new unsigned char[dwSize];
        if (lpb == NULL)
        {
            // �G���[
            return 0;
        }

        if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
        {
            // �G���[
        }

        RAWINPUT* raw = (RAWINPUT*)lpb;
        if (raw->header.dwType == RIM_TYPEMOUSE)
        {
             mouse = raw->data.mouse;
             head = raw->header;
             if (mouse.usButtonFlags == RI_MOUSE_WHEEL) {
                 std::cout << "h:" << (SHORT)mouse.usButtonData << std::endl;
             }
             if (mouse.usButtonFlags == RI_MOUSE_HWHEEL) {
                 std::cout << "w:" << (SHORT)mouse.usButtonData << std::endl;
             }
        }

        delete[] lpb;
        break;
#endif

    }
    return 0;
}


// ini�ɐݒ��ۑ�
BOOL func_exit(FILTER* fp) {
    fp->exfunc->ini_save_int(fp, (LPSTR)"scroll_rate_w", scroll_rate_w);
    fp->exfunc->ini_save_int(fp, (LPSTR)"scroll_rate_h", scroll_rate_h);
    fp->exfunc->ini_save_int(fp, (LPSTR)"scroll_rate_zoom", scroll_rate_zoom);
    fp->exfunc->ini_save_int(fp, (LPSTR)"enable_hwheel", enable_hwheel);
    fp->exfunc->ini_save_int(fp, (LPSTR)"enable_exchange_alt", enable_exchange_alt);
    fp->exfunc->ini_save_int(fp, (LPSTR)"enable_change_rate", enable_change_rate);
    return 0;
}


#if 0
// ini����ݒ��ǂݍ���
BOOL func_project_load(FILTER* fp, void* editp, void* data, int size) {
    fp->track[0] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_w", fp->track_default[0]);
    fp->track[1] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_h", fp->track_default[1]);
    fp->track[2] = fp->exfunc->ini_load_int(fp, (LPSTR)"scroll_rate_zoom", fp->track_default[2]);
    fp->check[0] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_hwheel", fp->check_default[0]);
    fp->check[1] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_exchange_alt", fp->check_default[1]);
    fp->check[2] = fp->exfunc->ini_load_int(fp, (LPSTR)"enable_change_rate", fp->check_default[2]);
    fp->exfunc->filter_window_update(fp);
    return 1;
}
#endif


// aup�ɂ͉����������܂Ȃ�
BOOL func_project_save(FILTER* fp, void* editp, void* data, int* size) {
    return 0;
}


BOOL func_update(FILTER* fp, int status){
    HWND tr = 0;
    switch (status) {
    case FILTER_UPDATE_STATUS_ALL:
        // AviUtl���v���W�F�N�g�؂�ւ��ŏ���ɏ������������X�Ɍ��̒l�ɏ���������
        fp->track[0] = scroll_rate_w;
        fp->track[1] = scroll_rate_h;
        fp->track[2] = scroll_rate_zoom;
        fp->check[0] = enable_hwheel;
        fp->check[1] = enable_exchange_alt;
        fp->check[2] = enable_change_rate;

        // �_�C�A���O�̕��i��
        tr = FindWindowEx(fp->hwnd, 0, TRACKBAR_CLASS, track_name[0]);
        SendMessage(tr, TBM_SETPOS, true, scroll_rate_w);
        tr = FindWindowEx(fp->hwnd, tr, "EDIT", 0);
        SetDlgItemInt(fp->hwnd, GetDlgCtrlID(tr), scroll_rate_w, false);

        tr = FindWindowEx(fp->hwnd, 0, TRACKBAR_CLASS, track_name[1]);
        SendMessage(tr, TBM_SETPOS, true, scroll_rate_h);
        tr = FindWindowEx(fp->hwnd, tr, "EDIT", 0);
        SetDlgItemInt(fp->hwnd, GetDlgCtrlID(tr), scroll_rate_h, false);

        tr = FindWindowEx(fp->hwnd, 0, TRACKBAR_CLASS, track_name[2]);
        SendMessage(tr, TBM_SETPOS, true, scroll_rate_zoom);
        tr = FindWindowEx(fp->hwnd, tr, "EDIT", 0);
        SetDlgItemInt(fp->hwnd, GetDlgCtrlID(tr), scroll_rate_zoom, false);

        SendMessage(FindWindowEx(fp->hwnd, 0, 0, check_name[0]), BM_SETCHECK, enable_hwheel, 0);
        SendMessage(FindWindowEx(fp->hwnd, 0, 0, check_name[1]), BM_SETCHECK, enable_exchange_alt, 0);
        SendMessage(FindWindowEx(fp->hwnd, 0, 0, check_name[2]), BM_SETCHECK, enable_change_rate, 0);

        break;

    // �l���ʂɕύX���ꂽ�甽�f
    case FILTER_UPDATE_STATUS_TRACK + 0:
        scroll_rate_w = fp->track[0];
        break;

    case FILTER_UPDATE_STATUS_TRACK + 1:
        scroll_rate_h = fp->track[1];
        break;

    case FILTER_UPDATE_STATUS_TRACK + 2:
        scroll_rate_zoom = fp->track[2];
        break;

    case FILTER_UPDATE_STATUS_CHECK + 0:
        enable_hwheel = fp->check[0];
        break;

    case FILTER_UPDATE_STATUS_CHECK + 1:
        enable_exchange_alt = fp->check[1];
        // ���I�����������Ăǂ��Ȃ�
        if (enable_exchange_alt == 1) {
            exedit_Replace8(0x3decd, 0x7c); // �c�z�C�[�����c�X�N���[���ɁAAlt+�c�z�C�[�������X�N���[���ɂ���
        }
        else {
            exedit_Replace8(0x3decd, 0x7d); // ���ɖ߂�
        }
        break;

    case FILTER_UPDATE_STATUS_CHECK + 2:
        enable_change_rate = fp->check[2];
        break;
    }

    return 0;
}
