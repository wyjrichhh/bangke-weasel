#pragma once

#include <functional>
#include <BangkeIPCData.h>
#include <BangkeUI.h>
#include "StandardLayout.h"
#include "Layout.h"

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

using namespace bangke;

enum class BackType {
  TEXT = 0,
  CAND = 1,
  BACKGROUND = 2
};

// 纯 Win32 分层窗口候选面板，绘制全部走 Direct2D/DirectWrite
class BangkePanel {
 public:
  BangkePanel(bangke::UI& ui);
  ~BangkePanel();

  bool Create(HWND parent);

  // AI 候选快照就绪时（服务端推送信号）触发的刷新回调，参数为快照序号
  std::function<void(UINT_PTR)> on_async_refresh;
  void Destroy();
  // 监听线程调用:向本窗口投递快照就绪(进程内,UIPI 无关)
  void PostSnapshotReady() {
    if (m_hWnd)
      ::PostMessage(m_hWnd, WM_APP + 0x43, 0, 0);
  }
  bool IsWindow() const { return m_hWnd != NULL && ::IsWindow(m_hWnd); }

  void MoveTo(RECT const& rc);
  void Refresh();
  void DoPaint();
  bool GetIsReposition() { return m_istorepos; }
  void RedrawWindow();
  // 有内容才允许显示。DoPaint 在什么都没画出来时已经 SW_HIDE，若这里照旧
  // SW_SHOWNA，就会把上一次 UpdateLayeredWindow 留在窗口上的旧位图重新露出来——
  // ai_only 推理期间菜单是空的，用户看到的就是「候选框没消失、露出一块」。
  // 返回窗口是否已可见，调用方据它维护自己的 shown 状态。
  bool Show(int cmd) {
    if (!IsWindow())
      return false;
    if (cmd != SW_HIDE && !m_has_content)
      return false;
    ::ShowWindow(m_hWnd, cmd);
    return cmd != SW_HIDE;
  }
  HWND hwnd() const { return m_hWnd; }

  static VOID CALLBACK OnTimer(_In_ HWND hwnd,
                               _In_ UINT uMsg,
                               _In_ UINT_PTR idEvent,
                               _In_ DWORD dwTime);
  static const int AUTOREV_TIMER = 20240315;
  static UINT_PTR ptimer;

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
  static void RegisterPanelClass();

  void OnLButtonDown(WPARAM wParam, LPARAM lParam);
  void OnLButtonUp(WPARAM wParam, LPARAM lParam);
  void OnMouseMove(WPARAM wParam, LPARAM lParam);

  template <typename T>
  int DPI_SCALE(T t) {
    return (int)(t * dpiScaleLayout);
  }
  void _InitFontRes(bool forced = false);
  void _CaptureRect(CRect& rect);
  void _CreateLayout();
  void _ResizeWindow();
  void _RepositionWindow(const bool& adj = false);
  bool _DrawPreedit(const Text& text, const CRect& rc);
  bool _DrawPreeditBack(const Text& text, const CRect& rc);
  bool _DrawCandidates(bool back = false);
  void _HighlightBack(const CRect& rc,
                      const COLORREF& color,
                      const COLORREF& shadowColor,
                      const int& radius,
                      const BackType& type,
                      const IsToRoundStruct& rd,
                      const COLORREF& bordercolor);
  void _FillRoundRect(const CRect& rc,
                      const D2D1_COLOR_F& color,
                      float radius,
                      const IsToRoundStruct& rd);
  void _StrokeRoundRect(const CRect& rc,
                        const D2D1_COLOR_F& color,
                        float radius,
                        const IsToRoundStruct& rd,
                        float strokeWidth);
  void _DrawShadow(const CRect& rc,
                   const COLORREF& shadowColor,
                   float radius);
  void _TextOut(const CRect& rc,
                const std::wstring& psz,
                const size_t& cch,
                const int& inColor,
                IDWriteTextFormat1* const pTextFormat = NULL);

  HWND m_hWnd = NULL;
  bool m_mouse_entry = false;
  CPoint m_lastMousePos = {-1, -1};

  bangke::Layout* m_layout = NULL;
  bangke::Context& m_ctx;
  bangke::Context& m_octx;
  bangke::Status& m_status;
  bangke::UIStyle& m_style;
  bangke::UIStyle& m_ostyle;
  const bool& m_in_server;

  CRect m_inputPos;
  int m_offsetys[MAX_CANDIDATES_COUNT];
  int m_offsety_preedit = 0;
  int m_offsety_aux = 0;
  bool m_istorepos = false;

  CIcon m_iconDisabled;
  CIcon m_iconEnabled;
  CIcon m_iconAlpha;
  CIcon m_iconFull;
  CIcon m_iconHalf;
  std::wstring m_current_zhung_icon;
  std::wstring m_current_ascii_icon;
  std::wstring m_current_half_icon;
  std::wstring m_current_full_icon;
  // 布局层(StandardLayout)的 GDI+ Region 命中测试仍需要 GDI+ 运行时
  Gdiplus::GdiplusStartupInput _m_gdiplusStartupInput;
  ULONG_PTR _m_gdiplusToken = 0;

  UINT dpi = 96;

  CRect rcw;
  BYTE m_candidateCount = 0;
  BYTE m_lastCandidateCount = 0;

  bool hide_candidates = false;
  // 上一帧 DoPaint 是否真画出了东西（与末尾的 !drawn -> SW_HIDE 同源）
  bool m_has_content = false;
  bool m_sticky = false;
  PDWR pDWR;
  std::function<void(size_t* const, size_t* const, bool* const, bool* const)>&
      _UICallback;
  float bar_scale_ = 1.0;
  float dpiScaleLayout = 1.0f;
  int m_hoverIndex = -1;
  HMONITOR m_hMonitor = NULL;
  bool m_redraw_by_monitor_change = false;
};
