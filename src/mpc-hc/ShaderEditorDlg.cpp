/*
 * (C) 2003-2006 Gabest
 * (C) 2006-2013 see Authors.txt
 *
 * This file is part of MPC-HC.
 *
 * MPC-HC is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * MPC-HC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "stdafx.h"
#include "mplayerc.h"
#include "ShaderEditorDlg.h"
#include "MainFrm.h"

#undef SubclassWindow


// CShaderLabelComboBox

BEGIN_MESSAGE_MAP(CShaderLabelComboBox, CComboBox)
    ON_WM_CTLCOLOR()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

HBRUSH CShaderLabelComboBox::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_EDIT) {
        if (m_edit.GetSafeHwnd() == nullptr) {
            m_edit.SubclassWindow(pWnd->GetSafeHwnd());
        }
    }

    return __super::OnCtlColor(pDC, pWnd, nCtlColor);
}

void CShaderLabelComboBox::OnDestroy()
{
    if (m_edit.GetSafeHwnd() != nullptr) {
        m_edit.UnsubclassWindow();
    }

    __super::OnDestroy();
}

// CShaderEdit

CShaderEdit::CShaderEdit()
{
    m_acdlg.Create(CShaderAutoCompleteDlg::IDD, nullptr);

    m_nEndChar = -1;
    m_nIDEvent = (UINT_PTR) - 1;
}

CShaderEdit::~CShaderEdit()
{
    m_acdlg.DestroyWindow();
}

BOOL CShaderEdit::PreTranslateMessage(MSG* pMsg)
{
    if (m_acdlg.IsWindowVisible()
            && pMsg->message == WM_KEYDOWN
            && (pMsg->wParam == VK_UP || pMsg->wParam == VK_DOWN
                || pMsg->wParam == VK_PRIOR || pMsg->wParam == VK_NEXT
                || pMsg->wParam == VK_RETURN || pMsg->wParam == VK_ESCAPE)) {
        int i = m_acdlg.m_list.GetCurSel();

        if (pMsg->wParam == VK_RETURN && i >= 0) {
            CString str;
            m_acdlg.m_list.GetText(i, str);
            i = str.Find('(') + 1;
            if (i > 0) {
                str = str.Left(i);
            }

            int nStartChar = 0, nEndChar = -1;
            GetSel(nStartChar, nEndChar);

            CString text;
            GetWindowText(text);
            while (nStartChar > 0 && _istalnum(text.GetAt(nStartChar - 1))) {
                nStartChar--;
            }

            SetSel(nStartChar, nEndChar);
            ReplaceSel(str, TRUE);
        } else if (pMsg->wParam == VK_ESCAPE) {
            m_acdlg.ShowWindow(SW_HIDE);
            return GetParent()->PreTranslateMessage(pMsg);
        } else {
            m_acdlg.m_list.SendMessage(pMsg->message, pMsg->wParam, pMsg->lParam);
        }

        return TRUE;
    }

    return __super::PreTranslateMessage(pMsg);
}

BEGIN_MESSAGE_MAP(CShaderEdit, CLineNumberEdit)
    ON_CONTROL_REFLECT(EN_UPDATE, OnUpdate)
    ON_WM_KILLFOCUS()
    ON_WM_TIMER()
END_MESSAGE_MAP()

void CShaderEdit::OnUpdate()
{
    if (m_nIDEvent == (UINT_PTR) - 1) {
        m_nIDEvent = SetTimer(1, 100, nullptr);
    }

    CString text;
    int nStartChar = 0, nEndChar = -1;
    GetSel(nStartChar, nEndChar);

    if (nStartChar == nEndChar) {
        GetWindowText(text);
        while (nStartChar > 0 && _istalnum(text.GetAt(nStartChar - 1))) {
            nStartChar--;
        }
    }

    if (nStartChar < nEndChar) {
        text = text.Mid(nStartChar, nEndChar - nStartChar);
        text.TrimRight('(');
        text.MakeLower();

        m_acdlg.m_list.ResetContent();

        CString key, value;
        POSITION pos = m_acdlg.m_inst.GetStartPosition();
        while (pos) {
            POSITION cur = pos;
            m_acdlg.m_inst.GetNextAssoc(pos, key, value);

            if (key.Find(text) == 0) {
                CAtlList<CString> sl;
                Explode(value, sl, '|', 2);
                if (sl.GetCount() != 2) {
                    continue;
                }
                CString name = sl.RemoveHead();
                CString description = sl.RemoveHead();
                int i = m_acdlg.m_list.AddString(name);
                m_acdlg.m_list.SetItemDataPtr(i, cur);
            }
        }

        if (m_acdlg.m_list.GetCount() > 0) {
            int lineheight = GetLineHeight();

            CPoint p = PosFromChar(nStartChar);
            p.y += lineheight;
            ClientToScreen(&p);
            CRect r(p, CSize(100, 100));

            m_acdlg.MoveWindow(r);
            m_acdlg.SetWindowPos(&wndTopMost, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            m_acdlg.ShowWindow(SW_SHOWNOACTIVATE);

            m_nEndChar = nEndChar;

            return;
        }
    }

    m_acdlg.ShowWindow(SW_HIDE);
}

void CShaderEdit::OnKillFocus(CWnd* pNewWnd)
{
    CString text;
    GetWindowText(text);
    __super::OnKillFocus(pNewWnd);
    GetWindowText(text);

    m_acdlg.ShowWindow(SW_HIDE);
}

void CShaderEdit::OnTimer(UINT_PTR nIDEvent)
{
    if (m_nIDEvent == nIDEvent) {
        int nStartChar = 0, nEndChar = -1;
        GetSel(nStartChar, nEndChar);
        if (nStartChar != nEndChar || m_nEndChar != nEndChar) {
            m_acdlg.ShowWindow(SW_HIDE);
        }
    }

    __super::OnTimer(nIDEvent);
}

// CShaderEditorDlg dialog

CShaderEditorDlg::CShaderEditorDlg()
    : CResizableDialog(CShaderEditorDlg::IDD, nullptr)
    , m_fSplitterGrabbed(false)
    , m_hD3DCompiler(nullptr)
    , m_pShader(nullptr)
    , m_nIDEventShader(0)
{
}

CShaderEditorDlg::~CShaderEditorDlg()
{
    if (m_hD3DCompiler) {
        FreeLibrary(m_hD3DCompiler);
    }
}

BOOL CShaderEditorDlg::Create(CWnd* pParent)
{
    if (!__super::Create(IDD, pParent)) {
        return FALSE;
    }

    AddAnchor(IDC_COMBO1, TOP_LEFT, TOP_RIGHT);
    AddAnchor(IDC_COMBO2, TOP_RIGHT);
    AddAnchor(IDC_EDIT1, TOP_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_EDIT2, BOTTOM_LEFT, BOTTOM_RIGHT);
    AddAnchor(IDC_BUTTON1, TOP_RIGHT);

    m_srcdata.SetTabStops(16);

    SetMinTrackSize(CSize(250, 40));

    m_targets.AddString(L"ps_2_0");
    m_targets.AddString(L"ps_2_a");
    m_targets.AddString(L"ps_2_b");
    m_targets.AddString(L"ps_3_0");

    const CAppSettings& s = AfxGetAppSettings();
    POSITION pos = s.m_shaders.GetHeadPosition();
    while (pos) {
        Shader const& shader = s.m_shaders.GetNext(pos);
        m_labels.SetItemDataPtr(m_labels.AddString(shader.label), (void*)&shader);
    }
    CorrectComboListWidth(m_labels);

    m_nIDEventShader = SetTimer(1, 1000, nullptr);

    return TRUE;
}

void CShaderEditorDlg::DoDataExchange(CDataExchange* pDX)
{
    __super::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_COMBO1, m_labels);
    DDX_Control(pDX, IDC_COMBO2, m_targets);
    DDX_Control(pDX, IDC_EDIT1, m_srcdata);
    DDX_Control(pDX, IDC_EDIT2, m_output);
}

bool CShaderEditorDlg::HitTestSplitter(CPoint p)
{
    CRect r, rs, ro;
    m_srcdata.GetWindowRect(&rs);
    m_output.GetWindowRect(&ro);
    ScreenToClient(&rs);
    ScreenToClient(&ro);
    GetClientRect(&r);
    r.left = ro.left;
    r.right = ro.right;
    r.top = rs.bottom;
    r.bottom = ro.top;
    return !!r.PtInRect(p);
}

BEGIN_MESSAGE_MAP(CShaderEditorDlg, CResizableDialog)
    ON_CBN_SELCHANGE(IDC_COMBO1, OnCbnSelchangeCombo1)
    ON_BN_CLICKED(IDC_BUTTON1, OnBnClickedButton2)
    ON_WM_TIMER()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_MOUSEMOVE()
    ON_WM_SETCURSOR()
END_MESSAGE_MAP()

// CShaderEditorDlg message handlers

BOOL CShaderEditorDlg::PreTranslateMessage(MSG* pMsg)
{
    if (pMsg->message == WM_KEYDOWN) {
        if (pMsg->wParam == VK_RETURN) {
            if (pMsg->hwnd == m_labels.m_edit.GetSafeHwnd()) {
                OnCbnSelchangeCombo1();
                return TRUE;
            }
        } else if (pMsg->wParam == VK_TAB) {
            if (pMsg->hwnd == m_srcdata.GetSafeHwnd()) {
                int nStartChar, nEndChar;
                m_srcdata.GetSel(nStartChar, nEndChar);
                if (nStartChar == nEndChar) {
                    m_srcdata.ReplaceSel(_T("\t"));
                }
                return TRUE;
            }
        } else if (pMsg->wParam == VK_ESCAPE) {
            return GetParent()->PreTranslateMessage(pMsg);
        }
    }

    return __super::PreTranslateMessage(pMsg);
}

void CShaderEditorDlg::OnCbnSelchangeCombo1()
{
    int i = m_labels.GetCurSel();

    if (i < 0) {
        Shader ss;

        m_labels.GetWindowTextW(ss.label);
        ss.label.Trim();

        if (ss.label.IsEmpty()) { return; }

        CStringA srcdata;
        if (!LoadResource(IDF_SHADER_EMPTY, srcdata, L"SHADER")) {
            return;
        }
        ss.srcdata = srcdata;
        ss.target = L"ps_2_0";

        CAppSettings& s = AfxGetAppSettings();
        POSITION pos = s.m_shaders.AddTail(ss);

        i = m_labels.AddString(ss.label);
        m_labels.SetCurSel(i);
        m_labels.SetItemDataPtr(i, (void*)&s.m_shaders.GetAt(pos));
    }

    m_pShader = (Shader*)m_labels.GetItemDataPtr(i);

    CStringW target = m_pShader->target;
    m_targets.SetWindowTextW(target);

    CStringW srcdata = m_pShader->srcdata;
    srcdata.Replace(L"\n", L"\r\n");
    m_srcdata.SetWindowTextW(srcdata);

    ((CMainFrame*)AfxGetMainWnd())->UpdateShaders(m_pShader->label);
}

void CShaderEditorDlg::OnBnClickedButton2()
{
    if (!m_pShader) {
        return;
    }

    if (IDYES != AfxMessageBox(IDS_SHADEREDITORDLG_0, MB_ICONQUESTION | MB_YESNO, 0)) {
        return;
    }

    CAppSettings& s = AfxGetAppSettings();

    for (POSITION pos = s.m_shaders.GetHeadPosition(); pos; s.m_shaders.GetNext(pos)) {
        if (m_pShader == &s.m_shaders.GetAt(pos)) {
            m_pShader = nullptr;
            s.m_shaders.RemoveAt(pos);
            int i = m_labels.GetCurSel();
            if (i >= 0) {
                m_labels.DeleteString(i);
            }
            m_labels.SetWindowText(_T(""));
            m_targets.SetWindowText(_T(""));
            m_srcdata.SetWindowText(_T(""));
            m_output.SetWindowText(_T(""));
            ((CMainFrame*)AfxGetMainWnd())->UpdateShaders(_T(""));
            break;
        }
    }
}

void CShaderEditorDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == m_nIDEventShader && IsWindowVisible() && m_pShader) {
        CStringW srcdata;
        m_srcdata.GetWindowTextW(srcdata);
        srcdata.Replace(L"\r", L"");
        srcdata.Trim();

        CStringW target;
        m_targets.GetWindowTextW(target);
        target.Trim();

        // TODO: autosave
        if (!srcdata.IsEmpty() && !target.IsEmpty() && (m_pShader->srcdata != srcdata || m_pShader->target != target)) {
            KillTimer(m_nIDEventShader);

            m_pShader->srcdata = srcdata;
            m_pShader->target = target;

#if D3DX_SDK_VERSION != 43
#error DirectX SDK June 2010 (v43) is required to build this, if the DirectX SDK has been updated, add loading functions to this part of the code and the class initializer
#endif// this code has duplicates in ShaderEditorDlg.cpp, DX9AllocatorPresenter.cpp and SyncRenderer.cpp
            if (!m_hD3DCompiler) {// load latest compatible version of the DLL that is available, and only once it is needed
                HMODULE hD3DCompiler = LoadLibraryW(L"D3DCompiler_43.dll");
                if (!hD3DCompiler) {
                    ASSERT(0);
                    m_output.SetWindowTextW(L"The installed DirectX End-User Runtime is outdated. Please download and install the June 2010 release or newer in order for MPC-HC to function properly.\n");// this text is a duplicate, the compiler will properly take care of it
                    goto FunctionEnd;
                }
                m_hD3DCompiler = hD3DCompiler;
                {
                    // import functions from D3DCompiler_43.dll
                    uintptr_t pModule = reinterpret_cast<uintptr_t>(m_hD3DCompiler);// just a named alias
                    IMAGE_DOS_HEADER const* pDOSHeader = reinterpret_cast<IMAGE_DOS_HEADER const*>(pModule);
                    IMAGE_NT_HEADERS const* pNTHeader = reinterpret_cast<IMAGE_NT_HEADERS const*>(pModule + static_cast<size_t>(static_cast<ULONG>(pDOSHeader->e_lfanew)));
                    IMAGE_EXPORT_DIRECTORY const* pEAT = reinterpret_cast<IMAGE_EXPORT_DIRECTORY const*>(pModule + static_cast<size_t>(pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress));
                    uintptr_t pAONbase = pModule + static_cast<size_t>(pEAT->AddressOfNames);
                    uintptr_t pAONObase = pModule + static_cast<size_t>(pEAT->AddressOfNameOrdinals);
                    uintptr_t pAOFbase = pModule + static_cast<size_t>(pEAT->AddressOfFunctions);
                    DWORD dwLoopCount = pEAT->NumberOfNames - 1;
                    {
                        __declspec(align(8)) static char const kszFunc[] = "D3DCompile";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                        ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                        for (;;) {
                            unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                            char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                            if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                                    && *reinterpret_cast<__int16 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int16 const*>(kszFunc + 8)
                                    && kszName[10] == kszFunc[10]) {// note that this part must compare zero end inclusive
                                // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                                break;
                            } else if (--i < 0) {
                                ASSERT(0);
                                goto d3dc_43EHandling;
                            }
                        }
                        unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                        unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                        m_fnD3DCompile = reinterpret_cast<D3DCompilePtr>(pModule + static_cast<size_t>(u32AOF));
                    }
                    {
                        __declspec(align(8)) static char const kszFunc[] = "D3DDisassemble";// 8-byte alignment used to facititate optimal 8-byte comparisons for the memcmp() intrinsic
                        ptrdiff_t i = static_cast<size_t>(dwLoopCount);// convert to signed for the loop system and pointer-sized for the pointer operations
                        for (;;) {
                            unsigned __int32 u32AON = *reinterpret_cast<unsigned __int32 const*>(pAONbase + i * 4);// table of four-byte elements
                            char const* kszName = reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON));
                            if (*reinterpret_cast<__int64 __unaligned const*>(kszName) == *reinterpret_cast<__int64 const*>(kszFunc)
                                    && *reinterpret_cast<__int32 __unaligned const*>(kszName + 8) == *reinterpret_cast<__int32 const*>(kszFunc + 8)
                                    && *reinterpret_cast<__int16 __unaligned const*>(kszName + 12) == *reinterpret_cast<__int16 const*>(kszFunc + 12)
                                    && kszName[14] == kszFunc[14]) {// note that this part must compare zero end inclusive
                                // if (!memcmp(reinterpret_cast<char const*>(pModule + static_cast<size_t>(u32AON)), kszFunc, sizeof(kszFunc))) { assembly checked; inlining failed
                                break;
                            } else if (--i < 0) {
                                ASSERT(0);
                                goto d3dc_43EHandling;
                            }
                        }
                        unsigned __int16 u16AONO = *reinterpret_cast<unsigned __int16 const*>(pAONObase + i * 2);// table of two-byte elements
                        unsigned __int32 u32AOF = *reinterpret_cast<unsigned __int32 const*>(pAOFbase + static_cast<size_t>(u16AONO) * 4);// table of four-byte elements
                        m_fnD3DDisassemble = reinterpret_cast<D3DDisassemblePtr>(pModule + static_cast<size_t>(u32AOF));
                    }
                    goto Skipd3dc_43EHandling;
d3dc_43EHandling:
                    m_output.SetWindowTextW(L"Could not read data from D3DCompiler_43.dll.\n");
                    goto FunctionEnd;
                }
            }
Skipd3dc_43EHandling:

            {
                // to scope FunctionEnd properly
                CStringA errmsg = "D3DCompile failed\n";
                ID3DBlob* pShaderBin, *pMessages;
                if (SUCCEEDED(m_fnD3DCompile(CStringA(srcdata), srcdata.GetLength(), NULL, NULL, NULL, "main", CStringA(target), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pShaderBin, &pMessages))) {
                    errmsg = "D3DCompile succeeded\n";
                    if (pMessages) {// non-critical debug warnings, note that this can be NULL if there are no warnings
                        errmsg.Append(reinterpret_cast<const char*>(pMessages->GetBufferPointer()), pMessages->GetBufferSize());
                        pMessages->Release();
                    }
                    m_fnD3DDisassemble(pShaderBin->GetBufferPointer(), pShaderBin->GetBufferSize(), 0, NULL, &pMessages);
                    pShaderBin->Release();
                    static_cast<CMainFrame*>(AfxGetMainWnd())->UpdateShaders(m_pShader->label);
                }
                if (pMessages) {// eiter critical failure errors or disassembly text
                    errmsg.Append(reinterpret_cast<const char*>(pMessages->GetBufferPointer()), pMessages->GetBufferSize());
                    pMessages->Release();
                }

                errmsg.Replace("\n", "\r\n");
                m_output.SetWindowTextW(CStringW(errmsg));
            }
FunctionEnd:
            m_nIDEventShader = SetTimer(1, 1000, nullptr);
        }
    }

    __super::OnTimer(nIDEvent);
}

void CShaderEditorDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    if (HitTestSplitter(point)) {
        m_fSplitterGrabbed = true;
        SetCapture();
    }

    __super::OnLButtonDown(nFlags, point);
}

void CShaderEditorDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_fSplitterGrabbed) {
        ReleaseCapture();
        m_fSplitterGrabbed = false;
    }

    __super::OnLButtonUp(nFlags, point);
}

void CShaderEditorDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_fSplitterGrabbed) {
        CRect r, rs, ro;
        GetClientRect(&r);
        m_srcdata.GetWindowRect(&rs);
        m_output.GetWindowRect(&ro);
        ScreenToClient(&rs);
        ScreenToClient(&ro);

        int dist = ro.top - rs.bottom;
        int avgdist = dist / 2;

        rs.bottom = min(max(point.y, rs.top + 40), ro.bottom - 40) - avgdist;
        ro.top = rs.bottom + dist;
        m_srcdata.MoveWindow(&rs);
        m_output.MoveWindow(&ro);

        int div = 100 * ((rs.bottom + ro.top) / 2) / (ro.bottom - rs.top);

        RemoveAnchor(IDC_EDIT1);
        RemoveAnchor(IDC_EDIT2);
        AddAnchor(IDC_EDIT1, TOP_LEFT, CSize(100, div)/*BOTTOM_RIGHT*/);
        AddAnchor(IDC_EDIT2, CSize(0, div)/*BOTTOM_LEFT*/, BOTTOM_RIGHT);
    }

    __super::OnMouseMove(nFlags, point);
}

BOOL CShaderEditorDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    CPoint p;
    GetCursorPos(&p);
    ScreenToClient(&p);
    if (HitTestSplitter(p)) {
        ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
        return TRUE;
    }

    return __super::OnSetCursor(pWnd, nHitTest, message);
}
