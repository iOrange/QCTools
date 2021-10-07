#ifdef UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif

#include <Windows.h>
#include <Commctrl.h>
#include <Richedit.h>
#include <shellapi.h>
#include "resource.h"

#include <gdiplus.h>
namespace gdp = Gdiplus;

// have to undef here cuz stupid Gdi+ uses these defines
#undef min
#undef max

#include "qccommon.h"
#include "PCTTexture.h"

#define STBI_NO_JPEG
#define STBI_NO_BMP
#define STBI_NO_PSD
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


#include <cmath>
#include <locale>
#include <codecvt>
#include <algorithm>

#define IDM_TOOLBAR_OPEN            7001
#define IDM_TOOLBAR_SAVETGA         7002
#define IDM_TOOLBAR_SAVEDDS         7003
#define IDM_TOOLBAR_SAVEHDR         7004
#define IDM_TOOLBAR_INFO            7005
#define IDM_TOOLBAR_REPLACE_CONTENT 7006
#define IDM_TOOLBAR_EXPORT_PCT      7007
#define IDM_TOOLBAR_HELP            7008


#define BAR_ICON_OPEN               0
#define BAR_ICON_SAVETGA            1
#define BAR_ICON_SAVEDDS            2
#define BAR_ICON_SAVEHDR            3
#define BAR_ICON_INFO               4
#define BAR_ICON_REPLACE_CONTENT    5
#define BAR_ICON_EXPORT_PCT         6
#define BAR_ICON_HELP               7

namespace qc {

HBITMAP LoadBitmapFromPNGRes(HMODULE hInst, LPCTSTR resName, LPCTSTR resType) {
    HBITMAP result = nullptr;

    HRSRC resHndl = ::FindResource(hInst, resName, resType);
    if (resHndl && resHndl != INVALID_HANDLE_VALUE) {
        const DWORD imageSize = ::SizeofResource(hInst, resHndl);
        HGLOBAL resData = ::LoadResource(hInst, resHndl);
        const void* resPtr = ::LockResource(resData);
        if (resPtr) {
            HGLOBAL buffer = ::GlobalAlloc(GMEM_MOVEABLE, imageSize);
            if (buffer) {
                void* bufferPtr = ::GlobalLock(buffer);
                if (bufferPtr) {
                    ::CopyMemory(bufferPtr, resPtr, imageSize);

                    IStream* stream = nullptr;
                    if (::CreateStreamOnHGlobal(buffer, FALSE, &stream) == S_OK) {
                       gdp::Bitmap* bmp = gdp::Bitmap::FromStream(stream);
                        stream->Release();
                        if (bmp && bmp->GetLastStatus() == gdp::Ok) {
                            bmp->GetHBITMAP(gdp::Color::Transparent, &result);
                        }
                        delete bmp;
                    }
                    ::GlobalUnlock(buffer);
                }
                ::GlobalFree(buffer);
            }
            ::FreeResource(resData);
        }
    }

    return result;
}


struct ImageViewSettings {
    int width;
    int height;
    int posX;
    int posY;
    int scale;  // in %, 10 - 300
};

struct ScrollingInfo {
    // horizontal
    int xMinScroll;
    int xMaxScroll;
    int xCurScroll;
    // vertical
    int yMinScroll;
    int yMaxScroll;
    int yCurScroll;
};

class MyApp : public IDropTarget {
public:
    MyApp()
        : mHwnd(nullptr)
        , mToolbar(nullptr)
        , mViewPanel(nullptr)
        , mPCTTexture(nullptr)
        , mPCTBitmap(nullptr)
        , mCheckerBrush(nullptr)
        , mBackBufferBMP(nullptr)
        , mBackBufferGFX(nullptr)
        , mAcceptFile(false)
    {
    }

    void Initialize(HINSTANCE hInst) {
        WNDCLASSEX wcex = { 0 };
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = MyApp::MyWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(LONG_PTR);
        wcex.hInstance = hInst;
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW);
        wcex.lpszMenuName = MAKEINTRESOURCE(IDR_MENU1);
        wcex.hCursor = ::LoadCursor(nullptr, IDI_APPLICATION);
        wcex.lpszClassName = TEXT("QCPCTAppMainWnd");
        wcex.hIcon = ::LoadIcon(hInst, MAKEINTRESOURCE(IDI_MAINICON));

        ::RegisterClassEx(&wcex);

        const int resX = ::GetSystemMetrics(SM_CXSCREEN);
        const int resY = ::GetSystemMetrics(SM_CYSCREEN);

        const int wndWidth = (resX - (resX >> 2));
        const int wndHeight = (resY - (resY >> 2));

        mHwnd = ::CreateWindow(
            wcex.lpszClassName,
            TEXT("QCPCT"),
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
            (resX - wndWidth) >> 1,
            (resY - wndHeight) >> 1,
            wndWidth,
            wndHeight,
            nullptr, nullptr,
            hInst, this
        );

        memset(&mScrollingInfo, 0, sizeof(mScrollingInfo));

        if (mHwnd) {
            ::ShowWindow(mHwnd, SW_SHOWNORMAL);
            ::UpdateWindow(mHwnd);

            ::OleInitialize(nullptr);
            ::RegisterDragDrop(mHwnd, this);
        }
    }

    void RunMessageLoop() {
        MSG msg;
        while (::GetMessage(&msg, nullptr, 0, 0)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }
    }

    // IUnknown implementation
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        return S_OK;
    }
    virtual ULONG STDMETHODCALLTYPE AddRef() override {
        return 0;
    }
    virtual ULONG STDMETHODCALLTYPE Release(void) override {
        return 0;
    }

    // IDropTarget implementation
    virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        mAcceptFile = false;

        if (!pdwEffect || !pDataObj) {
            return E_INVALIDARG;
        }

        FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
        STGMEDIUM medium;
        if (pDataObj && SUCCEEDED(pDataObj->GetData(&fmte, &medium))) {
            TCHAR fileName[MAX_PATH + 1];
            ::DragQueryFile((HDROP)medium.hGlobal, 0, fileName, sizeof(fileName));

            if (medium.pUnkForRelease) {
                medium.pUnkForRelease->Release();
            } else {
                ::GlobalFree(medium.hGlobal);
            }

            const size_t nameLen = wcslen(fileName);
            if (nameLen > 5 && (fileName[nameLen - 3] == TEXT('p') && fileName[nameLen - 2] == TEXT('c') && fileName[nameLen - 1] == TEXT('t'))) {
                *pdwEffect = DROPEFFECT_COPY;
                mAcceptFile = true;
            } else {
                *pdwEffect = DROPEFFECT_NONE;
            }
        }
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        *pdwEffect = mAcceptFile ? DROPEFFECT_COPY : DROPEFFECT_NONE;
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE DragLeave() override {
        return S_OK;
    }
    virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override {
        if (!pdwEffect || !pDataObj) {
            return E_INVALIDARG;
        }
        if (mAcceptFile) {
            FORMATETC fmte = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
            STGMEDIUM medium;
            if (pDataObj && SUCCEEDED(pDataObj->GetData(&fmte, &medium))) {
                TCHAR fileName[MAX_PATH + 1];
                ::DragQueryFile((HDROP)medium.hGlobal, 0, fileName, sizeof(fileName));

                if (medium.pUnkForRelease) {
                    medium.pUnkForRelease->Release();
                } else {
                    ::GlobalFree(medium.hGlobal);
                }

                std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                const String name = convert.to_bytes(fileName);
                this->OpenPCT(name);

                *pdwEffect = DROPEFFECT_COPY;
            }
        }

        return S_OK;
    }

private:
    static LRESULT CALLBACK MyWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        LRESULT result = 0;

        if (message == WM_CREATE) {
            LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
            MyApp* app = reinterpret_cast<MyApp*>(pcs->lpCreateParams);
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

            app->CreateToolbar(hwnd);
            app->CreateViewPanel(hwnd);

            result = 1;
        } else {
            MyApp* app = reinterpret_cast<MyApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

            bool wasHandled = false;

            if (app) {
                switch (message) {
                    case WM_COMMAND: {
                        switch (LOWORD(wParam)) {
                            case ID_FILE_OPEN:
                            case IDM_TOOLBAR_OPEN: {
                                app->OnMenuFileOpen();
                            } break;

                            case IDM_TOOLBAR_SAVETGA:
                            case ID_FILE_EXPORTASTGA: {
                                app->OnMenuExportAsTGA();
                            } break;

                            case IDM_TOOLBAR_SAVEDDS:
                            case ID_FILE_EXPORTASDDS: {
                                app->OnMenuExportAsDDS();
                            } break;

                            case IDM_TOOLBAR_SAVEHDR:
                            case ID_FILE_EXPORTASHDR: {
                                app->OnMenuExportAsHDR();
                            } break;

                            case IDM_TOOLBAR_INFO: {
                                app->OnShowImageInfo();
                            } break;

                            case IDM_TOOLBAR_REPLACE_CONTENT:
                            case ID_PCTTOOL_REPLACECONTENT: {
                                app->OnReplaceContent();
                            } break;

                            case IDM_TOOLBAR_EXPORT_PCT:
                            case ID_PCTTOOL_EXPORTPCT: {
                                app->OnExportPCT();
                            } break;

                            case IDM_TOOLBAR_HELP:
                            case ID_HELP_ABOUT: {
                                app->OnMenuAbout();
                            } break;

                            case ID_FILE_EXIT: {
                                app->OnMenuExit();
                            } break;
                        }
                    } break;

                    //case WM_ERASEBKGND: {
                    //    // tell Windows that we handled it. (but don't actually draw anything)
                    //    result = 1;
                    //    wasHandled = true;
                    //} break; 

                    case WM_SIZE: {
                        const UINT width = LOWORD(lParam);
                        const UINT height = HIWORD(lParam);
                        app->OnResize(width, height);
                        result = 0;
                        wasHandled = true;
                    } break;

                    case WM_DISPLAYCHANGE: {
                        ::InvalidateRect(hwnd, nullptr, FALSE);
                        result = 0;
                        wasHandled = true;
                    } break;

                    case WM_DESTROY: {
                        ::PostQuitMessage(0);
                        result = 1;
                        wasHandled = true;
                    } break;
                }
            }

            if (!wasHandled) {
                result = ::DefWindowProc(hwnd, message, wParam, lParam);
            }
        }

        return result;
    }

    static LRESULT CALLBACK ViewPanelWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        LRESULT result = 0;

        if (message == WM_CREATE) {
            LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
            MyApp* app = reinterpret_cast<MyApp*>(pcs->lpCreateParams);
            ::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
            result = 1;
        } else {
            MyApp* app = reinterpret_cast<MyApp*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

            bool wasHandled = false;

            if (app) {
                switch (message) {
                    case WM_ERASEBKGND: {
                        // tell Windows that we handled it. (but don't actually draw anything)
                        result = 1;
                        wasHandled = true;
                    } break;

                    case WM_PAINT: {
                        PAINTSTRUCT ps;
                        HDC hdc = ::BeginPaint(hwnd, &ps);
                        app->OnRender(hdc);
                        ::EndPaint(hwnd, &ps);
                        result = 0;
                        wasHandled = true;
                    } break;

                    case WM_VSCROLL: {
                        app->OnVScroll(wParam);
                        result = 0;
                        wasHandled = true;
                    } break;

                    case WM_HSCROLL: {
                        app->OnHScroll(wParam);
                        result = 0;
                        wasHandled = true;
                    } break;
                }
            }

            if (!wasHandled) {
                result = ::DefWindowProc(hwnd, message, wParam, lParam);
            }
        }

        return result;
    }

    static INT_PTR CALLBACK DlgImgPropsProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (uMsg == WM_INITDIALOG) {
            MyApp* app = reinterpret_cast<MyApp*>(lParam);
            ::SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));

            if (app) {
                app->OnInitImgPropsDlg(hwndDlg);
            }
        } else {
            MyApp* app = reinterpret_cast<MyApp*>(::GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
            if (app) {
                switch (uMsg) {
                    case WM_COMMAND: {
                        switch (LOWORD(wParam)) {
                            case IDOK: {
                                ::EndDialog(hwndDlg, 0);
                            } break;
                        }
                    } break;

                    case WM_CLOSE: {
                        ::EndDialog(hwndDlg, 0);
                    } break;
                }
            }
        }

        return 0;
    }

    static INT_PTR CALLBACK DlgAboutProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_INITDIALOG: {
                const char* text = "{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033{\\fonttbl{\\f0\\fnil\\fcharset0 MS Shell Dlg 2;}}\n"
"{\\colortbl ;\\red255\\green0\\blue0;}\n"
"{\\*\\generator Msftedit 5.41.21.2510;}\\viewkind4\\uc1\\pard\\sa200\\sl276\\slmult1\\qc\\lang9\\b\\f0\\fs36 Quake Champions PCT Viewer\\fs24\\line v0.4\\par\n"
"Created by the big fan of the game\\line Sergii 'iOrange' Kudlai\\line 2019\\par\n"
"\\b0\\fs16 This software should be used for non-commercial purposes only.\\line You should own a legal copy of the game.\\line Do not distrubute any files extracted by this software\\par\n"
"\\ul\\i Logo image was taken from the game\\par\n"
"\\cf1\\ulnone\\b\\i0\\fs28 Play Quake ! ;)\\cf0\\b0\\fs22\\par\n"
"}\n";

                SETTEXTEX textInfo = { ST_SELECTION, CP_ACP };
                ::SendDlgItemMessage(hwndDlg, IDC_RICHEDIT22, EM_SETTEXTEX, (WPARAM)&textInfo, (LPARAM)text);

                HWND pct = ::GetDlgItem(hwndDlg, IDC_PICTURE);
                ::SetWindowPos(pct, nullptr, 0, 0, 256, 256, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);

                HBITMAP bmp = LoadBitmapFromPNGRes(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_QC_LOGO), TEXT("PNG"));
                ::SendDlgItemMessage(hwndDlg, IDC_PICTURE, STM_SETIMAGE, (WPARAM)IMAGE_BITMAP, (LPARAM)bmp);
                ::DeleteObject(bmp);
            } break;

            case WM_CLOSE: {
                ::EndDialog(hwndDlg, 0);
            } break;
        }

        return 0;
    }

    void OnRender(HDC hdc) {
        if (!mCheckerBrush) {
            this->CreateCheckerBrush();
        }

        if (mBackBufferBMP && mBackBufferGFX) {
            gdp::Graphics graphics(hdc);

            mBackBufferGFX->Clear(gdp::Color::White);

            if (mPCTBitmap) {
                mBackBufferGFX->FillRectangle(mCheckerBrush, gdp::Rect(mImgViewSettings.posX,
                                                                       mImgViewSettings.posY,
                                                                       mImgViewSettings.width,
                                                                       mImgViewSettings.height));

                mBackBufferGFX->DrawImage(mPCTBitmap, mImgViewSettings.posX, mImgViewSettings.posY);
            }

            graphics.DrawImage(mBackBufferBMP, 0, 0);
        }
    }

    void OnResize(UINT width, UINT height) {
        this->UpdateScrollBars(width, height);

        RECT rc;
        ::GetWindowRect(mToolbar, &rc);
        const UINT toolbarHeight = rc.bottom - rc.top;
        ::SetWindowPos(mToolbar, nullptr, 0, 0, width, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);

        ::SetWindowPos(mViewPanel, nullptr, 0, toolbarHeight, width, height - toolbarHeight, SWP_NOOWNERZORDER | SWP_NOZORDER);

        ::GetWindowRect(mViewPanel, &rc);

        if (mBackBufferBMP) {
            delete mBackBufferBMP;
        }
        mBackBufferBMP = new gdp::Bitmap(rc.right - rc.left, rc.bottom - rc.top);

        if (mBackBufferGFX) {
            delete mBackBufferGFX;
        }
        mBackBufferGFX = gdp::Graphics::FromImage(mBackBufferBMP);
    }

    void RecreateBitmap() {
        if (mPCTBitmap) {
            delete mPCTBitmap;
            mPCTBitmap = nullptr;
        }
        if (!mPCTUncompressedData.empty()) {
            mPCTUncompressedData.clear();
        }

        const int w = mPCTTexture->GetWidth();
        const int h = mPCTTexture->GetHeight();
        mPCTUncompressedData = mPCTTexture->UnpackToRGBA(0, true);
        mPCTBitmap = new gdp::Bitmap(w, h, 4 * w, PixelFormat32bppARGB, mPCTUncompressedData.data());

        mImgViewSettings.width = mPCTTexture->GetWidth();
        mImgViewSettings.height = mPCTTexture->GetHeight();
        mImgViewSettings.posX = 0;
        mImgViewSettings.posY = 0;
        mImgViewSettings.scale = 100;

        memset(&mScrollingInfo, 0, sizeof(mScrollingInfo));
        this->UpdateScrollBars(0, 0);
        ::InvalidateRect(mViewPanel, nullptr, FALSE);
    }

    void OpenPCT(const String& fileName) {
        if (mPCTTexture) {
            delete mPCTTexture;
        }

        mPCTTexture = new PCTTexture();
        if (mPCTTexture->LoadFromFile(fileName)) {
            this->RecreateBitmap();
        }
    }

    void ReplaceContent(const String& fileName) {
        String extension = fileName.substr(fileName.length() - 3);
        std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension == "png" || extension == "tga") {
            int w, h, bpp;
            u8* data = stbi_load(fileName.c_str(), &w, &h, &bpp, STBI_rgb_alpha);

            if (w == mPCTTexture->GetWidth() && h == mPCTTexture->GetHeight()) {
                mPCTTexture->ReplaceUsingRGBA(data);
                this->RecreateBitmap();
            } else {
                ::MessageBox(mHwnd,
                    TEXT("The replacement image must be the same size!"),
                    TEXT("QCPCT"),
                    MB_OK | MB_ICONERROR);
            }

            stbi_image_free(data);
        } else if (extension == "dds") {
            if (!mPCTTexture->ReplaceUsingDDS(fileName)) {
                ::MessageBox(mHwnd,
                    TEXT("Replacement failed!\nPlease make sure that the replacement image\nhas same or greater dimension!"),
                    TEXT("QCPCT"),
                    MB_OK | MB_ICONERROR);
            } else {
                this->RecreateBitmap();
            }
        }
    }

    // Menu callbacks
    void OnMenuFileOpen() {
        TCHAR szFile[MAX_PATH] = {0};
        OPENFILENAME ofn = {0};
        ofn.lStructSize = sizeof(OPENFILENAME);
        ofn.hwndOwner = mHwnd;
        ofn.hInstance = ::GetModuleHandle(nullptr);
        ofn.lpstrFilter = TEXT("Quake Champions texture\0*.pct\0\0");
        ofn.lpstrTitle = TEXT("Select Quake Champions texture");
        ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrDefExt = TEXT("pct");

        if (::GetOpenFileName(&ofn)) {
            std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
            const String name = convert.to_bytes(ofn.lpstrFile);
            this->OpenPCT(name);
        }
    }

    void OnMenuExportAsTGA() {
        if (mPCTTexture && mPCTBitmap) {
            TCHAR szFile[MAX_PATH] = { 0 };
            OPENFILENAME ofn = { 0 };
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = mHwnd;
            ofn.hInstance = ::GetModuleHandle(nullptr);
            ofn.lpstrFilter = TEXT("Targa image\0*.tga\0\0");
            ofn.lpstrTitle = TEXT("Save TGA image");
            ofn.Flags = OFN_EXPLORER;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrDefExt = TEXT("tga");

            if (::GetSaveFileName(&ofn)) {
                std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                const String name = convert.to_bytes(ofn.lpstrFile);
                mPCTTexture->ExportToTGA(name);
            }
        }
    }

    void OnMenuExportAsDDS() {
        if (mPCTTexture && mPCTBitmap) {
            TCHAR szFile[MAX_PATH] = { 0 };
            OPENFILENAME ofn = { 0 };
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = mHwnd;
            ofn.hInstance = ::GetModuleHandle(nullptr);
            ofn.lpstrFilter = TEXT("DDS image\0*.dds\0\0");
            ofn.lpstrTitle = TEXT("Save DDS image");
            ofn.Flags = OFN_EXPLORER;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrDefExt = TEXT("dds");

            if (::GetSaveFileName(&ofn)) {
                std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                const String name = convert.to_bytes(ofn.lpstrFile);
                mPCTTexture->ExportToDDS(name);
            }
        }
    }

    void OnMenuExportAsHDR() {
        if (mPCTTexture && mPCTBitmap) {
            bool doSave = mPCTTexture->IsHDR();
            if (!doSave) {
                const int btn = ::MessageBox(mHwnd,
                                             TEXT("The texture is not an HDR!\nExport to HDR anyway?"),
                                             TEXT("QCPCT"),
                                             MB_OKCANCEL | MB_DEFBUTTON2 | MB_ICONWARNING);
                doSave = (btn == IDOK);
            }

            if (doSave) {
                TCHAR szFile[MAX_PATH] = { 0 };
                OPENFILENAME ofn = { 0 };
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = mHwnd;
                ofn.hInstance = ::GetModuleHandle(nullptr);
                ofn.lpstrFilter = TEXT("HDR image\0*.hdr\0\0");
                ofn.lpstrTitle = TEXT("Save HDR image");
                ofn.Flags = OFN_EXPLORER;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrDefExt = TEXT("hdr");

                if (::GetSaveFileName(&ofn)) {
                    std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                    const String name = convert.to_bytes(ofn.lpstrFile);
                    mPCTTexture->ExportToHDR(name);
                }
            }
        }
    }

    void OnMenuExit() {
        ::PostQuitMessage(0);
    }

    void OnShowImageInfo() {
        if (mPCTTexture) {
            ::DialogBoxParam(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DLGIMGPROPS), mHwnd, &MyApp::DlgImgPropsProc, (LPARAM)this);
        }
    }

    void OnInitImgPropsDlg(HWND hwndDlg) {
        std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
        std::wstring wName = convert.from_bytes(mPCTTexture->GetFileName());
        ::SetDlgItemTextW(hwndDlg, IDC_TXTFILENAME, wName.c_str());

        if (mPCTTexture->IsCubemap()) {
            ::SetDlgItemText(hwndDlg, IDC_TXTFILETYPE, TEXT("Cubemap"));
        } else if(mPCTTexture->IsHDR()) {
            ::SetDlgItemText(hwndDlg, IDC_TXTFILETYPE, TEXT("HDR Texture"));
        } else {
            ::SetDlgItemText(hwndDlg, IDC_TXTFILETYPE, TEXT("2D Texture"));
        }

        switch (mPCTTexture->GetFormat()) {
            case qc::TF_RGBA: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("NONE (RGBA)"));
            } break;
            case qc::TF_RGBX: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("NONE (RGBx)"));
            } break;
            case qc::TF_BC3: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("BC3"));
            } break;
            case qc::TF_BC4: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("BC4"));
            } break;
            case qc::TF_BC5:
            case qc::TF_BC5X: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("BC5"));
            } break;
            case qc::TF_BC6H: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("BC6H"));
            } break;
            default: {
                ::SetDlgItemText(hwndDlg, IDC_TXTCOMPRESSION, TEXT("BC7"));
            } break;
        }

        qc::String dimensions = std::to_string(mPCTTexture->GetWidth()) + " x " + std::to_string(mPCTTexture->GetHeight());
        ::SetDlgItemTextA(hwndDlg, IDC_TXTDIMENSIONS, dimensions.c_str());

        ::SetDlgItemTextA(hwndDlg, IDC_TXTMIPMAPSCOUNT, std::to_string(mPCTTexture->GetMipsCount()).c_str());
    }

    void OnReplaceContent() {
        if (mPCTTexture && mPCTBitmap) {
            if (mPCTTexture->IsCubemap()) {
                ::MessageBox(mHwnd,
                    TEXT("Cubemap modification is not supported yet"),
                    TEXT("QCPCT"),
                    MB_OK | MB_ICONERROR);
            } else if (mPCTTexture->IsHDR()) {
                ::MessageBox(mHwnd,
                    TEXT("HDR modification is not supported yet"),
                    TEXT("QCPCT"),
                    MB_OK | MB_ICONERROR);
            } else {
                TCHAR szFile[MAX_PATH] = { 0 };
                OPENFILENAME ofn = { 0 };
                ofn.lStructSize = sizeof(OPENFILENAME);
                ofn.hwndOwner = mHwnd;
                ofn.hInstance = ::GetModuleHandle(nullptr);
                ofn.lpstrFilter = TEXT("Image files\0*.tga;*.png;*.dds\0\0");
                ofn.lpstrTitle = TEXT("Choose image file to replace PCT content");
                ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrDefExt = TEXT("tga");

                if (::GetOpenFileName(&ofn)) {
                    std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                    const String name = convert.to_bytes(ofn.lpstrFile);
                    this->ReplaceContent(name);
                }
            }
        }
    }

    void OnExportPCT() {
        if (mPCTTexture && mPCTBitmap) {
            TCHAR szFile[MAX_PATH] = { 0 };
            OPENFILENAME ofn = { 0 };
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = mHwnd;
            ofn.hInstance = ::GetModuleHandle(nullptr);
            ofn.lpstrFilter = TEXT("Quake Champions texture\0*.pct\0\0");
            ofn.lpstrTitle = TEXT("Select Quake Champions texture");
            ofn.Flags = OFN_EXPLORER;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrDefExt = TEXT("pct");

            if (::GetSaveFileName(&ofn)) {
                std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
                const String name = convert.to_bytes(ofn.lpstrFile);
                if (!mPCTTexture->SaveToFile(name)) {
                    ::MessageBox(mHwnd,
                        TEXT("Failed to export PCT file!"),
                        TEXT("QCPCT"),
                        MB_OK | MB_ICONERROR);
                }
            }
        }
    }

    void OnMenuAbout() {
        // https://msdn.microsoft.com/en-us/library/windows/desktop/hh298375(v=vs.85).aspx
        // "It is necessary to call the LoadLibrary function to load Riched32.dll, Riched20.dll, or Msftedit.dll before the dialog is created."
        ::LoadLibrary(TEXT("Msftedit.dll"));
        ::DialogBox(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDD_DLGABOUT), mHwnd, &MyApp::DlgAboutProc);
    }
    /////////////////

    void CreateToolbar(HWND hwnd) {
        const int ImageListID = 0;

        TBBUTTON tbButtons[] = {
            { MAKELONG(BAR_ICON_OPEN,            ImageListID), IDM_TOOLBAR_OPEN,            TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Open PCT") },
            { MAKELONG(BAR_ICON_SAVETGA,         ImageListID), IDM_TOOLBAR_SAVETGA,         TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Save as TGA") },
            { MAKELONG(BAR_ICON_SAVEDDS,         ImageListID), IDM_TOOLBAR_SAVEDDS,         TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Save as DDS") },
            { MAKELONG(BAR_ICON_SAVEHDR,         ImageListID), IDM_TOOLBAR_SAVEHDR,         TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Save as HDR") },
            { 0,                                               0,                           TBSTATE_ENABLED, BTNS_SEP,    {0}, 0, 0 },
            { MAKELONG(BAR_ICON_INFO,            ImageListID), IDM_TOOLBAR_INFO,            TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Image info") },
            { 0,                                               0,                           TBSTATE_ENABLED, BTNS_SEP,    {0}, 0, 0 },
            { MAKELONG(BAR_ICON_REPLACE_CONTENT, ImageListID), IDM_TOOLBAR_REPLACE_CONTENT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Replace content") },
            { MAKELONG(BAR_ICON_EXPORT_PCT,      ImageListID), IDM_TOOLBAR_EXPORT_PCT,      TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("Export PCT") },
            { 0,                                               0,                           TBSTATE_ENABLED, BTNS_SEP,    {0}, 0, 0 },
            { MAKELONG(BAR_ICON_HELP,            ImageListID), IDM_TOOLBAR_HELP,            TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, (INT_PTR)TEXT("About") }
        };

        const int numButtons = sizeof(tbButtons) / sizeof(tbButtons[0]);

        HINSTANCE hInst = ::GetModuleHandle(nullptr);

        HWND hWndToolbar = ::CreateWindowEx(0, TOOLBARCLASSNAME, nullptr,
                                            WS_CHILD | TBSTYLE_TOOLTIPS | TBSTYLE_FLAT,
                                            0, 0, 0, 0,
                                            hwnd,
                                            nullptr,
                                            hInst,
                                            nullptr);
        HIMAGELIST imgList = ::ImageList_Create(16, 16, ILC_COLOR32, numButtons, 0);
        HBITMAP bmp = LoadBitmapFromPNGRes(hInst, MAKEINTRESOURCE(IDB_TOOLBAR_ICONS), TEXT("PNG"));
        ::ImageList_AddMasked(imgList, bmp, 0);
        ::DeleteObject(bmp);

        ::SendMessage(hWndToolbar, TB_SETIMAGELIST, (WPARAM)ImageListID, (LPARAM)imgList);
        ::SendMessage(hWndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        ::SendMessage(hWndToolbar, TB_ADDBUTTONS, (WPARAM)numButtons, (LPARAM)&tbButtons);
        ::SendMessage(hWndToolbar, TB_SETMAXTEXTROWS, 0, 0);
        ::SendMessage(hWndToolbar, TB_AUTOSIZE, 0, 0);
        ::ShowWindow(hWndToolbar, SW_SHOW);

        mToolbar = hWndToolbar;
    }

    void CreateViewPanel(HWND hMainWnd) {
        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = MyApp::ViewPanelWndProc;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = sizeof(LONG_PTR);
        wcex.hInstance = ::GetModuleHandle(nullptr);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(WHITE_BRUSH));
        wcex.lpszMenuName = nullptr;
        wcex.hCursor = ::LoadCursor(nullptr, IDI_APPLICATION);
        wcex.lpszClassName = TEXT("QCPCTAppViewPanel");

        ::RegisterClassEx(&wcex);

        RECT rc;
        ::GetClientRect(hMainWnd, &rc);

        mViewPanel = ::CreateWindow(
            wcex.lpszClassName,
            nullptr,
            WS_CHILDWINDOW | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | WS_BORDER,
            0, 0,
            rc.right - rc.left,
            rc.bottom - rc.top,
            hMainWnd, nullptr,
            wcex.hInstance, this
        );
    }

    void CreateCheckerBrush() {
        const int cellSize = 8;
        const int imgWidth = cellSize * 2;

        const u32 colors[4] = { 0xFFFFFFFF, 0xCCCCCCCC, 0xCCCCCCCC, 0xFFFFFFFF };

        u32* image = reinterpret_cast<u32*>(alloca(imgWidth * imgWidth * 4));
        u32* ptr = image;
        for (int y = 0; y < imgWidth; ++y) {
            for (int x = 0; x < imgWidth; ++x, ++ptr) {
                const int idx = ((x / cellSize) << 1) | (y / cellSize);
                *ptr = colors[idx];
            }
        }

        gdp::Bitmap bmp(imgWidth, imgWidth, 4 * imgWidth, PixelFormat32bppRGB, reinterpret_cast<u8*>(image));
        mCheckerBrush = new gdp::TextureBrush(&bmp);
    }

    void UpdateScrollBars(UINT width, UINT height) {
        RECT rect;
        ::GetClientRect(mViewPanel, &rect);

        const int wndWidth = (!width) ? (rect.right - rect.left) : width;
        const int wndHeight = (!height) ? rect.bottom - rect.top : height;

        if (mPCTTexture) {
            mScrollingInfo.xMaxScroll = std::max(mImgViewSettings.width - wndWidth, 0);
            mScrollingInfo.xCurScroll = std::min(mScrollingInfo.xCurScroll, mScrollingInfo.xMaxScroll);

            SCROLLINFO si = { 0 };
            si.cbSize = sizeof(si);

            if (!mScrollingInfo.xMaxScroll) {
                ::ShowScrollBar(mViewPanel, SB_HORZ, FALSE);
            } else {
                si.fMask = SIF_RANGE | SIF_PAGE;
                si.nMin  = mScrollingInfo.xMinScroll;
                si.nMax  = mImgViewSettings.width;
                si.nPage = wndWidth;
                ::ShowScrollBar(mViewPanel, SB_HORZ, TRUE);
                ::SetScrollInfo(mViewPanel, SB_HORZ, &si, TRUE);
            }

            mScrollingInfo.yMaxScroll = std::max(mImgViewSettings.height - wndHeight, 0);
            mScrollingInfo.yCurScroll = std::min(mScrollingInfo.yCurScroll, mScrollingInfo.yMaxScroll);

            if (!mScrollingInfo.yMaxScroll) {
                ::ShowScrollBar(mViewPanel, SB_VERT, FALSE);
            } else {
                si.cbSize = sizeof(si);
                si.fMask = SIF_RANGE | SIF_PAGE;
                si.nMin  = mScrollingInfo.yMinScroll;
                si.nMax  = mImgViewSettings.height;
                si.nPage = wndHeight;
                ::ShowScrollBar(mViewPanel, SB_VERT, TRUE);
                ::SetScrollInfo(mViewPanel, SB_VERT, &si, TRUE);
            }
        } else {
            ::ShowScrollBar(mViewPanel, SB_HORZ, FALSE);
            ::ShowScrollBar(mViewPanel, SB_VERT, FALSE);
        }
    }

    void OnVScroll(WPARAM wParam) {
        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        ::GetScrollInfo(mViewPanel, SB_VERT, &si);
        const int iVertPos = si.nPos;

        switch (LOWORD(wParam)) {
            case SB_TOP:
                si.nPos = si.nMin;
            break;

            case SB_BOTTOM:
                si.nPos = si.nMax;
            break;

            case SB_LINEUP:
                si.nPos -= 1;
            break;

            case SB_LINEDOWN:
                si.nPos += 1;
            break;

            case SB_PAGEUP:
                si.nPos -= si.nPage;
            break;

            case SB_PAGEDOWN:
                si.nPos += si.nPage;
            break;

            case SB_THUMBTRACK:
            case SB_THUMBPOSITION:
                si.nPos = si.nTrackPos;
            break;

            default:
                break;
        }

        // Set the position and then retrieve it.  Due to adjustments
        //   by Windows it may not be the same as the value set.
        si.fMask = SIF_POS;
        ::SetScrollInfo(mViewPanel, SB_VERT, &si, TRUE);
        ::GetScrollInfo(mViewPanel, SB_VERT, &si);

        // If the position has changed, scroll the window and update it
        if (si.nPos != iVertPos) {
            //::ScrollWindow(mViewPanel, 0, iVertPos - si.nPos, nullptr, nullptr);
            ::UpdateWindow(mViewPanel);

            mImgViewSettings.posY = -si.nPos;
            ::InvalidateRect(mViewPanel, nullptr, FALSE);
        }
    }

    void OnHScroll(WPARAM wParam) {
        SCROLLINFO si = { 0 };
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        ::GetScrollInfo(mViewPanel, SB_HORZ, &si);
        const int iHorzPos = si.nPos;

        switch (LOWORD(wParam)) {
            case SB_LEFT:
                si.nPos = si.nMin;
            break;

            case SB_RIGHT:
                si.nPos = si.nMax;
            break;

            case SB_LINELEFT:
                si.nPos -= 1;
            break;

            case SB_LINERIGHT:
                si.nPos += 1;
            break;

            case SB_PAGELEFT:
                si.nPos -= si.nPage;
            break;

            case SB_PAGERIGHT:
                si.nPos += si.nPage;
            break;

            case SB_THUMBTRACK:
            case SB_THUMBPOSITION:
                si.nPos = si.nTrackPos;
            break;

            default:
            break;
        }

        // Set the position and then retrieve it.  Due to adjustments
        //   by Windows it may not be the same as the value set.
        si.fMask = SIF_POS;
        ::SetScrollInfo(mViewPanel, SB_HORZ, &si, TRUE);
        ::GetScrollInfo(mHwnd, SB_HORZ, &si);

        // If the position has changed, scroll the window 
        if (si.nPos != iHorzPos) {
            //::ScrollWindow(mViewPanel, iHorzPos - si.nPos, 0, nullptr, nullptr);
            ::UpdateWindow(mViewPanel);

            mImgViewSettings.posX = -si.nPos;
            ::InvalidateRect(mViewPanel, nullptr, FALSE);
        }
    }

private:
    HWND                mHwnd;
    HWND                mToolbar;
    HWND                mViewPanel;
    PCTTexture*         mPCTTexture;
    Array<u8>           mPCTUncompressedData;
    gdp::Bitmap*        mPCTBitmap;
    gdp::Brush*         mCheckerBrush;
    ImageViewSettings   mImgViewSettings;
    ScrollingInfo       mScrollingInfo;

    // Double-buffering stuff
    gdp::Bitmap*        mBackBufferBMP;
    gdp::Graphics*      mBackBufferGFX;

    // Drag'n'Drop support
    bool                mAcceptFile;
};

} // namespace qc


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */, LPSTR /* lpCmdLine */, int /* nCmdShow */) {
    ::HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);

    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof(INITCOMMONCONTROLSEX);
    iccx.dwICC = ICC_WIN95_CLASSES;
    ::InitCommonControlsEx(&iccx);

    // Initialize GDI+.
    ULONG_PTR gdiplusToken;
    gdp::GdiplusStartupInput gdiplusStartupInput;
    gdp::Status status = gdp::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    if (gdp::Ok == status) {
        qc::MyApp app;
        app.Initialize(hInstance);
        app.RunMessageLoop();

        gdp::GdiplusShutdown(gdiplusToken);
    }

    return 0;
}

