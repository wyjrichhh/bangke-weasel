#include "stdafx.h"
#include "BangkePanel.h"

#include <utility>
#include <ShellScalingApi.h>
#include <VersionHelpers.hpp>
#include <BangkeIPCData.h>
#include <algorithm>

#include "VerticalLayout.h"
#include "HorizontalLayout.h"

// for IDI_ZH, IDI_EN
#include <resource.h>
#define COLORTRANSPARENT(color) ((color & 0xff000000) == 0)
#define COLORNOTTRANSPARENT(color) ((color & 0xff000000) != 0)
#define TRANS_COLOR 0x00000000
#define HALF_ALPHA_COLOR(color) \
  ((((color & 0xff000000) >> 25) & 0xff) << 24) | (color & 0x00ffffff)

#pragma comment(lib, "Shcore.lib")

static const WCHAR PANEL_CLASS[] = L"BangkePanelWnd";

template <class t0, class t1, class t2>
inline void LoadIconNecessary(t0& a, t1& b, t2& c, int d) {
  if (a == b)
    return;
  a = b;
  if (b.empty())
    c.LoadIconW(d, STATUS_ICON_SIZE, STATUS_ICON_SIZE, LR_DEFAULTCOLOR);
  else
    c = (HICON)LoadImage(NULL, b.c_str(), IMAGE_ICON, STATUS_ICON_SIZE,
                         STATUS_ICON_SIZE, LR_LOADFROMFILE);
}

static inline void ReconfigRoundInfo(IsToRoundStruct& rd,
                                     const int& i,
                                     const int& m_candidateCount) {
  if (i == 0 && m_candidateCount > 1) {
    std::swap(rd.IsTopLeftNeedToRound, rd.IsBottomLeftNeedToRound);
    std::swap(rd.IsTopRightNeedToRound, rd.IsBottomRightNeedToRound);
  }
  if (i == m_candidateCount - 1) {
    std::swap(rd.IsTopLeftNeedToRound, rd.IsBottomLeftNeedToRound);
    std::swap(rd.IsTopRightNeedToRound, rd.IsBottomRightNeedToRound);
  }
}

static inline D2D1_COLOR_F ColorFFromARGB(const COLORREF& color) {
  return D2D1::ColorF((float)GetRValue(color) / 255.0f,
                      (float)GetGValue(color) / 255.0f,
                      (float)GetBValue(color) / 255.0f,
                      (float)((color >> 24) & 255) / 255.0f);
}

BangkePanel::BangkePanel(bangke::UI& ui)
    : m_ctx(ui.ctx()),
      m_octx(ui.octx()),
      m_status(ui.status()),
      m_in_server(ui.InServer()),
      m_style(ui.style()),
      m_ostyle(ui.ostyle()),
      m_inputPos(CRect()),
      pDWR(ui.pdwr()),
      _UICallback(ui.uiCallback()) {
  memset(m_offsetys, 0, sizeof(m_offsetys));
  m_iconDisabled.LoadIconW(IDI_RELOAD, STATUS_ICON_SIZE, STATUS_ICON_SIZE,
                           LR_DEFAULTCOLOR);
  m_iconEnabled.LoadIconW(IDI_ZH, STATUS_ICON_SIZE, STATUS_ICON_SIZE,
                          LR_DEFAULTCOLOR);
  m_iconAlpha.LoadIconW(IDI_EN, STATUS_ICON_SIZE, STATUS_ICON_SIZE,
                        LR_DEFAULTCOLOR);
  m_iconFull.LoadIconW(IDI_FULL_SHAPE, STATUS_ICON_SIZE, STATUS_ICON_SIZE,
                       LR_DEFAULTCOLOR);
  m_iconHalf.LoadIconW(IDI_HALF_SHAPE, STATUS_ICON_SIZE, STATUS_ICON_SIZE,
                       LR_DEFAULTCOLOR);
  // 布局层的 GDI+ Region 命中测试依赖 GDI+ 运行时
  GdiplusStartup(&_m_gdiplusToken, &_m_gdiplusStartupInput, NULL);

  HMONITOR hMonitor = MonitorFromRect(m_inputPos, MONITOR_DEFAULTTONEAREST);
  UINT dpiX = 96, dpiY = 96;
  if (hMonitor) {
    GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
    m_hMonitor = hMonitor;
  }
  dpi = dpiX;
  _InitFontRes();
  m_ostyle = m_style;
}

BangkePanel::~BangkePanel() {
  if (IsWindow())
    Destroy();
  Gdiplus::GdiplusShutdown(_m_gdiplusToken);
  delete m_layout;
  m_layout = NULL;
}

void BangkePanel::RegisterPanelClass() {
  static bool registered = false;
  if (registered)
    return;
  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW;
  wc.lpfnWndProc = &BangkePanel::WndProc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszClassName = PANEL_CLASS;
  RegisterClassExW(&wc);
  registered = true;
}

bool BangkePanel::Create(HWND parent) {
  RegisterPanelClass();
  m_hWnd = ::CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
      PANEL_CLASS, L"", WS_POPUP | WS_CLIPSIBLINGS | WS_DISABLED, 0, 0, 0, 0,
      parent, NULL, GetModuleHandle(NULL), this);
  return m_hWnd != NULL;
}

void BangkePanel::Destroy() {
  if (m_hWnd) {
    ::DestroyWindow(m_hWnd);
    m_hWnd = NULL;
  }
}

// 快照就绪通知:由本进程内的监听线程 Post(④ 起);
// WM_APP 私有消息只在本进程内流动,不经 UIPI,提权应用同样可达
static const UINT WM_BANGKE_ASYNC_REFRESH = WM_APP + 0x43;

LRESULT CALLBACK BangkePanel::WndProc(HWND hwnd,
                                      UINT uMsg,
                                      WPARAM wParam,
                                      LPARAM lParam) {
  BangkePanel* self = NULL;
  if (uMsg == WM_NCCREATE) {
    self = (BangkePanel*)((CREATESTRUCT*)lParam)->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)self);
  } else {
    self = (BangkePanel*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  }
  if (!self)
    return DefWindowProc(hwnd, uMsg, wParam, lParam);

  if (uMsg == WM_BANGKE_ASYNC_REFRESH) {
    if (self->on_async_refresh)
      self->on_async_refresh(lParam);
    return 0;
  }

  switch (uMsg) {
    case WM_NCCREATE:
      break;
    case WM_CREATE:
      self->m_mouse_entry = false;
      self->m_hoverIndex = -1;
      self->Refresh();
      return TRUE;
    case WM_DESTROY:
      self->m_hoverIndex = -1;
      self->m_lastMousePos = {-1, -1};
      self->m_sticky = false;
      delete self->m_layout;
      self->m_layout = NULL;
      return 0;
    case WM_DPICHANGED:
      self->Refresh();
      return 0;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
      self->OnLButtonDown(wParam, lParam);
      return 0;
    case WM_LBUTTONUP:
      self->OnLButtonUp(wParam, lParam);
      return 0;
    case WM_MOUSEWHEEL: {
      int delta = GET_WHEEL_DELTA_WPARAM(wParam);
      if (self->_UICallback && delta != 0) {
        bool nextpage = delta < 0;
        self->_UICallback(NULL, NULL, NULL, &nextpage);
      }
      return 0;
    }
    case WM_MOUSEMOVE:
      self->OnMouseMove(wParam, lParam);
      return 0;
    case WM_MOUSELEAVE:
      self->m_hoverIndex = -1;
      if (self->IsWindow())
        ::InvalidateRect(hwnd, &self->rcw, TRUE);
      self->m_mouse_entry = false;
      return 0;
    default:
      break;
  }
  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void BangkePanel::_ResizeWindow() {
  CSize m_size = m_layout->GetContentSize();
  ::SetWindowPos(m_hWnd, NULL, 0, 0, m_size.cx, m_size.cy,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOREDRAW);
}

void BangkePanel::_CreateLayout() {
  if (m_layout != NULL)
    delete m_layout;

  Layout* layout = NULL;
  if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL ||
      m_style.layout_type == UIStyle::LAYOUT_VERTICAL_TEXT) {
    // LAYOUT_VERTICAL_TEXT 未做逐字竖排,先回落到竖排
    layout = new VerticalLayout(m_style, m_ctx, m_status, pDWR);
  } else {
    layout = new HorizontalLayout(m_style, m_ctx, m_status, pDWR);
  }
  m_layout = layout;
}

// 更新界面
void BangkePanel::Refresh() {
  bool should_show_icon =
      (m_status.ascii_mode || !m_status.composing || !m_ctx.aux.empty());
  m_candidateCount = min(m_ctx.cinfo.candies.size(), MAX_CANDIDATES_COUNT);
  if (m_lastCandidateCount > 0 && m_candidateCount == 0) {
    m_sticky = false;
  }
  m_lastCandidateCount = m_candidateCount;
  bool show_tips =
      m_in_server &&
      ((!m_ctx.aux.empty() && m_ctx.cinfo.empty() && m_ctx.preedit.empty()) ||
       (m_ctx.empty() && should_show_icon));
  bool show_schema_menu = m_status.schema_id == L".default";
  bool margin_negative =
      (DPI_SCALE(m_style.margin_x) < 0 || DPI_SCALE(m_style.margin_y) < 0);
  bool inline_no_candidates =
      (m_style.inline_preedit && m_candidateCount == 0) && !show_tips;
  hide_candidates = inline_no_candidates ||
                    (margin_negative && !show_tips && !show_schema_menu);

  if (!hide_candidates || inline_no_candidates) {
    _InitFontRes();
    _CreateLayout();

    HDC dc = ::GetDC(m_hWnd);
    m_layout->DoLayout(dc, pDWR);
    ::ReleaseDC(m_hWnd, dc);
    {
      CSize sz = m_layout->GetContentSize();

    }
    _ResizeWindow();
    _RepositionWindow();
    if (m_ctx != m_octx) {
      m_octx = m_ctx;
      RedrawWindow();
    } else {
    }
  }
}

void BangkePanel::_InitFontRes(bool forced) {
  HMONITOR hMonitor = MonitorFromRect(m_inputPos, MONITOR_DEFAULTTONEAREST);
  UINT dpiX = 96, dpiY = 96;
  if (hMonitor)
    GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
  if (forced || (pDWR == NULL) || (m_ostyle != m_style) || (dpiX != dpi)) {
    pDWR.reset();
    pDWR = std::make_shared<DirectWriteResources>(m_style, dpiX);
    pDWR->pRenderTarget->SetTextAntialiasMode(
        (D2D1_TEXT_ANTIALIAS_MODE)m_style.antialias_mode);
  }
  m_ostyle = m_style;
  dpi = dpiX;
  dpiScaleLayout = (float)dpi / 96.0f;
}

static HBITMAP CopyDCToBitmap(HDC hDC, LPRECT lpRect) {
  if (!hDC || !lpRect || IsRectEmpty(lpRect))
    return NULL;
  HDC hMemDC = NULL;
  HBITMAP hBitmap = NULL, hOldBitmap = NULL;
  int nX, nY, nX2, nY2;
  int nWidth, nHeight;

  nX = lpRect->left;
  nY = lpRect->top;
  nX2 = lpRect->right;
  nY2 = lpRect->bottom;
  nWidth = nX2 - nX;
  nHeight = nY2 - nY;

  hMemDC = CreateCompatibleDC(hDC);
  if (!hMemDC)
    return NULL;
  hBitmap = CreateCompatibleBitmap(hDC, nWidth, nHeight);
  if (!hBitmap) {
    DeleteDC(hMemDC);
    return NULL;
  }
  hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);
  if (!hOldBitmap) {
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    return NULL;
  }
  if (!BitBlt(hMemDC, 0, 0, nWidth, nHeight, hDC, nX, nY, SRCCOPY)) {
    SelectObject(hMemDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemDC);
    return NULL;
  }
  SelectObject(hMemDC, hOldBitmap);
  DeleteDC(hMemDC);
  return hBitmap;
}

void BangkePanel::_CaptureRect(CRect& rect) {
  HDC ScreenDC = ::GetDC(NULL);
  CRect rc;
  ::GetWindowRect(m_hWnd, &rc);
  POINT WindowPosAtScreen = {rc.left, rc.top};
  CRect captureRect = rect;
  captureRect.OffsetRect(WindowPosAtScreen);
  HBITMAP bmp = CopyDCToBitmap(ScreenDC, LPRECT(captureRect));
  if (!bmp) {
    ::ReleaseDC(NULL, ScreenDC);
    return;
  }
  if (!::OpenClipboard(m_hWnd)) {
    BK_DEBUG << "_CaptureRect: OpenClipboard failed";
    DeleteObject(bmp);
    ::ReleaseDC(NULL, ScreenDC);
    return;
  }
  EmptyClipboard();
  if (!SetClipboardData(CF_BITMAP, bmp)) {
    BK_DEBUG << "_CaptureRect: SetClipboardData failed";
    DeleteObject(bmp);
  }
  CloseClipboard();
  ::ReleaseDC(NULL, ScreenDC);
}

void BangkePanel::OnLButtonDown(WPARAM wParam, LPARAM lParam) {
  if (hide_candidates)
    return;
  CPoint point;
  point.x = GET_X_LPARAM(lParam);
  point.y = GET_Y_LPARAM(lParam);

  // capture
  if (m_style.click_to_capture) {
    CRect recth = m_layout->GetCandidateRect((int)m_ctx.cinfo.highlighted);
    if (m_istorepos)
      recth.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
    recth.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                      DPI_SCALE(m_style.hilite_padding_y));
    if (recth.PtInRect(point))
      _CaptureRect(recth);
    else {
      if (COLORTRANSPARENT(m_style.shadow_color) &&
          DPI_SCALE(m_style.shadow_radius) != 0) {
        CRect crc(rcw);
        int shadow_gap = (DPI_SCALE(m_style.shadow_offset_x) == 0 &&
                          DPI_SCALE(m_style.shadow_offset_y) == 0)
                             ? 2 * DPI_SCALE(m_style.shadow_radius)
                             : DPI_SCALE(m_style.shadow_radius) +
                                   DPI_SCALE(m_style.shadow_radius) / 2;
        int ofx = DPI_SCALE(m_style.hilite_padding_x) +
                          abs(DPI_SCALE(m_style.shadow_offset_x)) +
                          shadow_gap >
                      abs(DPI_SCALE(m_style.margin_x))
                  ? DPI_SCALE(m_style.hilite_padding_x) +
                        abs(DPI_SCALE(m_style.shadow_offset_x)) +
                        shadow_gap - abs(DPI_SCALE(m_style.margin_x))
                  : 0;
        int ofy = DPI_SCALE(m_style.hilite_padding_y) +
                          abs(DPI_SCALE(m_style.shadow_offset_y)) +
                          shadow_gap >
                      abs(DPI_SCALE(m_style.margin_y))
                  ? DPI_SCALE(m_style.hilite_padding_y) +
                        abs(DPI_SCALE(m_style.shadow_offset_y)) +
                        shadow_gap - abs(DPI_SCALE(m_style.margin_y))
                  : 0;
        crc.DeflateRect(m_layout->offsetX - ofx, m_layout->offsetY - ofy);
        _CaptureRect(crc);
      } else {
        _CaptureRect(rcw);
      }
    }
  }
  {
    if (!m_style.inline_preedit && m_candidateCount != 0 &&
        COLORNOTTRANSPARENT(m_style.prevpage_color) &&
        COLORNOTTRANSPARENT(m_style.nextpage_color)) {
      // click prepage
      if (m_ctx.cinfo.currentPage != 0) {
        CRect prc = m_layout->GetPrepageRect();
        if (m_istorepos)
          prc.OffsetRect(0, m_offsety_preedit);
        if (prc.PtInRect(point)) {
          bool nextPage = false;
          if (_UICallback)
            _UICallback(NULL, NULL, &nextPage, NULL);
          return;
        }
      }
      // click nextpage
      if (!m_ctx.cinfo.is_last_page) {
        CRect prc = m_layout->GetNextpageRect();
        if (m_istorepos)
          prc.OffsetRect(0, m_offsety_preedit);
        if (prc.PtInRect(point)) {
          bool nextPage = true;
          if (_UICallback)
            _UICallback(NULL, NULL, &nextPage, NULL);
          return;
        }
      }
    }
    for (size_t i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
      CRect rect = m_layout->GetCandidateRect((int)i);
      if (m_istorepos)
        rect.OffsetRect(0, m_offsetys[i]);
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      if (rect.PtInRect(point)) {
        bar_scale_ = 0.8f;
        if (i != m_ctx.cinfo.highlighted) {
          if (_UICallback)
            _UICallback(NULL, &i, NULL, NULL);
        } else {
          RedrawWindow();
        }
        ptimer = UINT_PTR(this);
        ::SetTimer(m_hWnd, AUTOREV_TIMER, 1000, &BangkePanel::OnTimer);
        return;
      }
    }
  }
}

void BangkePanel::OnLButtonUp(WPARAM wParam, LPARAM lParam) {
  if (hide_candidates)
    return;
  CPoint point;
  point.x = GET_X_LPARAM(lParam);
  point.y = GET_Y_LPARAM(lParam);

  ::KillTimer(m_hWnd, AUTOREV_TIMER);
  bar_scale_ = 1.0;
  ptimer = 0;
  {
    // select by click
    CRect rect = m_layout->GetCandidateRect((int)m_ctx.cinfo.highlighted);
    if (m_istorepos)
      rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
    rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                     DPI_SCALE(m_style.hilite_padding_y));
    if (rect.PtInRect(point)) {
      size_t i = m_ctx.cinfo.highlighted;
      if (_UICallback) {
        m_mouse_entry = false;
        _UICallback(&i, NULL, NULL, NULL);
        if (!m_status.composing)
          Destroy();
      }
    } else {
      RedrawWindow();
    }
  }
}

UINT_PTR BangkePanel::ptimer = 0;
VOID CALLBACK BangkePanel::OnTimer(_In_ HWND hwnd,
                                   _In_ UINT uMsg,
                                   _In_ UINT_PTR idEvent,
                                   _In_ DWORD dwTime) {
  ::KillTimer(hwnd, idEvent);
  BangkePanel* self = (BangkePanel*)ptimer;
  ptimer = 0;
  if (self) {
    self->bar_scale_ = 1.0;
    self->RedrawWindow();
  }
}

void BangkePanel::OnMouseMove(WPARAM wParam, LPARAM lParam) {
  if (m_style.hover_type == UIStyle::NONE)
    return;
  if (m_mouse_entry == false) {
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(TRACKMOUSEEVENT);
    tme.dwFlags = TME_LEAVE;
    tme.dwHoverTime = 20;  // unit: ms
    tme.hwndTrack = m_hWnd;
    TrackMouseEvent(&tme);
  }
  m_mouse_entry = true;
  CPoint point;
  point.x = GET_X_LPARAM(lParam);
  point.y = GET_Y_LPARAM(lParam);

  CPoint ptScreen = point;
  ::ClientToScreen(m_hWnd, &ptScreen);
  if (ptScreen == m_lastMousePos || m_lastMousePos.x == -1) {
    if (m_lastMousePos.x == -1)
      m_lastMousePos = ptScreen;
    return;
  }
  m_lastMousePos = ptScreen;

  for (size_t i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
    CRect rect = m_layout->GetCandidateRect((int)i);
    if (m_istorepos)
      rect.OffsetRect(0, m_offsetys[i]);
    rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                     DPI_SCALE(m_style.hilite_padding_y));
    if (rect.PtInRect(point)) {
      if (i != m_ctx.cinfo.highlighted) {
        if (m_style.hover_type == UIStyle::HoverType::HILITE) {
          if (_UICallback)
            _UICallback(NULL, &i, NULL, NULL);
        } else if (m_hoverIndex != i) {
          m_hoverIndex = static_cast<int>(i);
          ::InvalidateRect(m_hWnd, &rcw, TRUE);
        }
      } else if (m_style.hover_type == UIStyle::HoverType::SEMI_HILITE &&
                 m_hoverIndex != -1) {
        m_hoverIndex = -1;
        ::InvalidateRect(m_hWnd, &rcw, TRUE);
      }
    }
  }
}


namespace {
// 连续圆角(超椭圆角,指数 n>2):角部曲率从直边平滑渐入,是 mac「圆润」
// 观感的数学来源;标准圆弧在与直边的切点处曲率突跳,同半径观感更「硬」。
// mac 侧对应 BangkeView.swift 的 drawSmoothLines 贝塞尔
constexpr float kCornerExponent = 4.5f;
constexpr int kCornerSteps = 10;

// 沿超椭圆弧 |dx/r|^n+|dy/r|^n=1 从角 a0 扫到 a1(弧度,屏幕坐标顺时针),
// 采样加点。n=2 时退化为圆弧
void AddSmoothCorner(ID2D1GeometrySink* sink, float cx, float cy, float r,
                     float a0, float a1) {
  const float e = 2.0f / kCornerExponent;
  for (int i = 1; i <= kCornerSteps; ++i) {
    const float t = (float)i / kCornerSteps;
    const float a = a0 + (a1 - a0) * t;
    const float c = cosf(a), sn = sinf(a);
    sink->AddLine(D2D1::Point2F(
        cx + r * copysignf(powf(fabsf(c), e), c),
        cy + r * copysignf(powf(fabsf(sn), e), sn)));
  }
}

// 构建整块圆角路径(直边 + 四角按 rd 决定圆/直;圆角一律连续圆角)
void BuildRoundRectPath(ID2D1GeometrySink* sink, const CRect& rc, float radius,
                        const IsToRoundStruct& rd, bool filled) {
  float l = (float)rc.left, t = (float)rc.top, r = (float)rc.right,
        b = (float)rc.bottom;
  const float pi = 3.14159265f;
  sink->SetFillMode(D2D1_FILL_MODE_WINDING);
  sink->BeginFigure(D2D1::Point2F(l + (rd.IsTopLeftNeedToRound ? radius : 0), t),
                    filled ? D2D1_FIGURE_BEGIN_FILLED
                           : D2D1_FIGURE_BEGIN_HOLLOW);
  sink->AddLine(D2D1::Point2F(r - (rd.IsTopRightNeedToRound ? radius : 0), t));
  if (rd.IsTopRightNeedToRound)
    AddSmoothCorner(sink, r - radius, t + radius, radius, -pi / 2, 0);
  sink->AddLine(
      D2D1::Point2F(r, b - (rd.IsBottomRightNeedToRound ? radius : 0)));
  if (rd.IsBottomRightNeedToRound)
    AddSmoothCorner(sink, r - radius, b - radius, radius, 0, pi / 2);
  sink->AddLine(
      D2D1::Point2F(l + (rd.IsBottomLeftNeedToRound ? radius : 0), b));
  if (rd.IsBottomLeftNeedToRound)
    AddSmoothCorner(sink, l + radius, b - radius, radius, pi / 2, pi);
  sink->AddLine(D2D1::Point2F(l, t + (rd.IsTopLeftNeedToRound ? radius : 0)));
  if (rd.IsTopLeftNeedToRound)
    AddSmoothCorner(sink, l + radius, t + radius, radius, pi, 3 * pi / 2);
  sink->EndFigure(D2D1_FIGURE_END_CLOSED);
}
}  // namespace

void BangkePanel::_FillRoundRect(const CRect& rc,
                                 const D2D1_COLOR_F& color,
                                 float radius,
                                 const IsToRoundStruct& rd) {
  ID2D1RenderTarget* rt = pDWR->pRenderTarget.Get();
  if (pDWR->CreateBrush(color) != S_OK)
    return;
  if (radius <= 0.5f) {
    rt->FillRectangle(
        D2D1::RectF((float)rc.left, (float)rc.top, (float)rc.right,
                    (float)rc.bottom),
        pDWR->pBrush.Get());
    return;
  }
  ComPtr<ID2D1PathGeometry> geo;
  ComPtr<ID2D1GeometrySink> sink;
  if (FAILED(pDWR->pD2d1Factory->CreatePathGeometry(&geo)) ||
      FAILED(geo->Open(&sink)))
    return;
  BuildRoundRectPath(sink.Get(), rc, radius, rd, true);
  if (SUCCEEDED(sink->Close()))
    rt->FillGeometry(geo.Get(), pDWR->pBrush.Get());
}

void BangkePanel::_StrokeRoundRect(const CRect& rc,
                                   const D2D1_COLOR_F& color,
                                   float radius,
                                   const IsToRoundStruct& rd,
                                   float strokeWidth) {
  ID2D1RenderTarget* rt = pDWR->pRenderTarget.Get();
  ComPtr<ID2D1SolidColorBrush> brush;
  if (FAILED(rt->CreateSolidColorBrush(color, &brush)))
    return;
  if (radius <= 0.5f) {
    rt->DrawRectangle(
        D2D1::RectF((float)rc.left, (float)rc.top, (float)rc.right,
                    (float)rc.bottom),
        brush.Get(), strokeWidth);
    return;
  }
  ComPtr<ID2D1PathGeometry> geo;
  ComPtr<ID2D1GeometrySink> sink;
  if (FAILED(pDWR->pD2d1Factory->CreatePathGeometry(&geo)) ||
      FAILED(geo->Open(&sink)))
    return;
  BuildRoundRectPath(sink.Get(), rc, radius, rd, false);
  if (SUCCEEDED(sink->Close()))
    rt->DrawGeometry(geo.Get(), brush.Get(), strokeWidth);
}

// 阴影：多层递减透明度圆角矩形环，替代原 GDI+ 高斯模糊方案
void BangkePanel::_DrawShadow(const CRect& rc,
                              const COLORREF& shadowColor,
                              float radius) {
  int shadowRadius = DPI_SCALE(m_style.shadow_radius);
  if (shadowRadius <= 0 || COLORTRANSPARENT(shadowColor))
    return;
  BYTE alpha = (BYTE)((shadowColor >> 24) & 255);
  D2D1_COLOR_F base = ColorFFromARGB(shadowColor);
  int rings = min(shadowRadius, 24);
  CRect rect(rc);
  rect.OffsetRect(DPI_SCALE(m_style.shadow_offset_x),
                  DPI_SCALE(m_style.shadow_offset_y));
  for (int i = 0; i < rings; i++) {
    float fall = 1.0f - (float)i / (float)rings;
    D2D1_COLOR_F c = D2D1::ColorF(base.r, base.g, base.b,
                                  (float)alpha / 255.0f * fall * fall);
    _FillRoundRect(rect, c, radius + 1.0f + i, IsToRoundStruct());
    rect.InflateRect(1, 1);
  }
}

void BangkePanel::_HighlightBack(const CRect& rc,
                                 const COLORREF& color,
                                 const COLORREF& shadowColor,
                                 const int& radius,
                                 const BackType& type,
                                 const IsToRoundStruct& rd,
                                 const COLORREF& bordercolor) {
  float fradius = (float)radius;
  IsToRoundStruct useRd(rd);
  // 贴住面板外缘时(rd.Hemispherical)圆角必须用背景那一圈的半径:高亮和背景是两条
  // 同心的弧,半径不同就错开,边上漏出一条背景色。描边再让出半个线宽(线以路径为中心
  // 画),填充不让——原先只换了描边,填充仍旧用自己的半径,所以看着还是不贴合。
  const bool matchOuter = rd.Hemispherical && type != BackType::BACKGROUND;
  const float outerRadius = (float)DPI_SCALE(m_style.round_corner_ex);
  const float fillRadius = type == BackType::BACKGROUND
                               ? outerRadius
                               : (matchOuter ? outerRadius : fradius);
  float strokeRadius = fillRadius;
  if (matchOuter && (DPI_SCALE(m_style.border) % 2))
    strokeRadius -= DPI_SCALE(m_style.border) / 2;
  // 阴影
  if (DPI_SCALE(m_style.shadow_radius) && COLORNOTTRANSPARENT(shadowColor)) {
    _DrawShadow(rc, shadowColor, fillRadius);
  }
  // 背景填充
  if (COLORNOTTRANSPARENT(color)) {
    _FillRoundRect(rc, ColorFFromARGB(color), fillRadius, useRd);
  }
  // 边框
  if (COLORNOTTRANSPARENT(bordercolor) && DPI_SCALE(m_style.border) > 0) {
    IsToRoundStruct borderRd;
    _StrokeRoundRect(rc, ColorFFromARGB(bordercolor), strokeRadius, borderRd,
                     (float)DPI_SCALE(m_style.border));
  }
}

// 高亮贴到面板外缘的那几条边不再留内缩,并让圆角改用背景那一圈(见 _HighlightBack)。
// 判据取自布局给的圆角信息:一条边的两个角都开着圆角,那条边就是窗口边——Layout 只为
// 贴着窗口的角开圆角(StandardLayout 的 is_to_round_corner)。不对齐的话,
// real_margin 与 hilite_padding 的差值(默认 12 vs 8)会在边缘留一条 4px 的缝。
void BangkePanel::_SnapHighlightToPanel(CRect& rect, IsToRoundStruct& rd) {
  const CRect bg = m_layout->GetContentRect();
  bool atWindowEdge = false;
  // 水平贴边满铺只属于多行(mac 单行高亮只包住第一个候选词,
  // 左右各留边距);单行跳过水平 snap,高亮保持第一候选文字矩形+padding
  const bool horizontalLayout =
      m_style.layout_type == UIStyle::LAYOUT_HORIZONTAL;
  if (!horizontalLayout && rd.IsTopLeftNeedToRound &&
      rd.IsBottomLeftNeedToRound) {
    rect.left = bg.left;
    atWindowEdge = true;
  }
  if (!horizontalLayout && rd.IsTopRightNeedToRound &&
      rd.IsBottomRightNeedToRound) {
    rect.right = bg.right;
    atWindowEdge = true;
  }
  if (rd.IsTopLeftNeedToRound && rd.IsTopRightNeedToRound) {
    rect.top = bg.top;
    atWindowEdge = true;
  }
  if (rd.IsBottomLeftNeedToRound && rd.IsBottomRightNeedToRound) {
    rect.bottom = bg.bottom;
    atWindowEdge = true;
  }
  if (atWindowEdge)
    rd.Hemispherical = true;
}

// draw preedit text, text only
bool BangkePanel::_DrawPreedit(const Text& text, const CRect& rc) {
  bool drawn = false;
  std::wstring const& t = text.str;
  IDWriteTextFormat1* txtFormat = pDWR->pPreeditTextFormat.Get();

  if (!t.empty()) {
    bangke::TextRange range = m_layout->GetPreeditRange();

    if (range.start < range.end) {
      std::wstring before_str = t.substr(0, range.start);
      std::wstring hilited_str = t.substr(range.start, range.end);
      std::wstring after_str = t.substr(range.end);
      CSize beforeSz = m_layout->GetBeforeSize();
      CSize hilitedSz = m_layout->GetHilitedSize();
      CSize afterSz = m_layout->GetAfterSize();

      int x = rc.left;
      int y = rc.top;

      if (range.start > 0) {
        std::wstring str_before(t.substr(0, range.start));
        CRect rc_before;
        rc_before = CRect(x, rc.top, rc.left + beforeSz.cx, rc.bottom);
        _TextOut(rc_before, str_before.c_str(), str_before.length(),
                 m_style.text_color, txtFormat);
        x += beforeSz.cx + DPI_SCALE(m_style.hilite_spacing);
      }
      {
        std::wstring str_highlight(
            t.substr(range.start, (size_t)range.end - range.start));
        CRect rc_hi;
        rc_hi = CRect(x, rc.top, x + hilitedSz.cx, rc.bottom);
        _TextOut(rc_hi, str_highlight.c_str(), str_highlight.length(),
                 m_style.hilited_text_color, txtFormat);
        x += rc_hi.Width() + DPI_SCALE(m_style.hilite_spacing);
      }
      if (range.end < static_cast<int>(t.length())) {
        std::wstring str_after(t.substr(range.end));
        CRect rc_after;
        rc_after = CRect(x, rc.top, x + afterSz.cx, rc.bottom);
        _TextOut(rc_after, str_after.c_str(), str_after.length(),
                 m_style.text_color, txtFormat);
      }
    } else {
      CRect rcText(rc.left, rc.top, rc.right, rc.bottom);
      _TextOut(rcText, t.c_str(), t.length(), m_style.text_color, txtFormat);
    }
    // pager mark
    if (m_candidateCount && !m_style.inline_preedit &&
        COLORNOTTRANSPARENT(m_style.prevpage_color) &&
        COLORNOTTRANSPARENT(m_style.nextpage_color)) {
      const std::wstring pre = L"<";
      const std::wstring next = L">";
      CRect prc = m_layout->GetPrepageRect();
      int color =
          m_ctx.cinfo.currentPage ? m_style.prevpage_color : m_style.text_color;
      if (m_istorepos)
        prc.OffsetRect(0, m_offsety_preedit);
      _TextOut(prc, pre.c_str(), pre.length(), color, txtFormat);

      CRect nrc = m_layout->GetNextpageRect();
      color = m_ctx.cinfo.is_last_page ? m_style.text_color
                                       : m_style.nextpage_color;
      if (m_istorepos)
        nrc.OffsetRect(0, m_offsety_preedit);
      _TextOut(nrc, next.c_str(), next.length(), color, txtFormat);
    }
    drawn = true;
  }
  return drawn;
}

// draw hilited back color, back only
bool BangkePanel::_DrawPreeditBack(const Text& text, const CRect& rc) {
  bool drawn = false;
  std::wstring const& t = text.str;

  if (!t.empty()) {
    bangke::TextRange range = m_layout->GetPreeditRange();

    if (range.start < range.end) {
      CSize beforeSz = m_layout->GetBeforeSize();
      CSize hilitedSz = m_layout->GetHilitedSize();

      int x = rc.left;
      int y = rc.top;

      if (range.start > 0) {
        x += beforeSz.cx + DPI_SCALE(m_style.hilite_spacing);
      }
      {
        CRect rc_hi;
        rc_hi = CRect(x, rc.top, x + hilitedSz.cx, rc.bottom);
        if (m_layout->ShouldDisplayStatusIcon()) {
          if (hilitedSz.cy < STATUS_ICON_SIZE)
            rc_hi.InflateRect(0, (STATUS_ICON_SIZE - hilitedSz.cy) / 2);
        }

        rc_hi.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                          DPI_SCALE(m_style.hilite_padding_y));
        IsToRoundStruct rd = m_layout->GetTextRoundInfo();
        if (m_istorepos) {
          std::swap(rd.IsTopLeftNeedToRound, rd.IsBottomLeftNeedToRound);
          std::swap(rd.IsTopRightNeedToRound, rd.IsBottomRightNeedToRound);
        }
        _HighlightBack(rc_hi, m_style.hilited_back_color,
                       m_style.hilited_shadow_color,
                       DPI_SCALE(m_style.round_corner), BackType::TEXT, rd,
                       TRANS_COLOR);
      }
    }
    drawn = true;
  }
  return drawn;
}

bool BangkePanel::_DrawCandidates(bool back) {
  bool drawn = false;
  const std::vector<Text>& candidates(m_ctx.cinfo.candies);
  const std::vector<Text>& comments(m_ctx.cinfo.comments);
  const std::vector<Text>& labels(m_ctx.cinfo.labels);
  if (pDWR->pTextFormat.Get() == nullptr &&
      pDWR->pLabelTextFormat.Get() == nullptr &&
      pDWR->pCommentTextFormat.Get() == nullptr) {
    _InitFontRes(true);
  }
  ComPtr<IDWriteTextFormat1> txtFormat = pDWR->pTextFormat;
  ComPtr<IDWriteTextFormat1> labeltxtFormat = pDWR->pLabelTextFormat;
  ComPtr<IDWriteTextFormat1> commenttxtFormat = pDWR->pCommentTextFormat;
  BackType bkType = BackType::CAND;

  CRect rect;
  if (back) {
    if (COLORNOTTRANSPARENT(m_style.candidate_shadow_color)) {
      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex)
          continue;
        rect = m_layout->GetCandidateRect((int)i);
        IsToRoundStruct rd = m_layout->GetRoundInfo(i);
        if (m_istorepos) {
          rect.OffsetRect(0, m_offsetys[i]);
          ReconfigRoundInfo(rd, i, m_candidateCount);
        }
        rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                         DPI_SCALE(m_style.hilite_padding_y));
        _HighlightBack(rect, 0x00000000, m_style.candidate_shadow_color,
                       DPI_SCALE(m_style.round_corner), bkType, rd,
                       TRANS_COLOR);
        drawn = true;
      }
    }
    if ((COLORNOTTRANSPARENT(m_style.candidate_back_color) ||
         COLORNOTTRANSPARENT(m_style.candidate_border_color))) {
      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex)
          continue;
        rect = m_layout->GetCandidateRect((int)i);
        IsToRoundStruct rd = m_layout->GetRoundInfo(i);
        if (m_istorepos) {
          rect.OffsetRect(0, m_offsetys[i]);
          ReconfigRoundInfo(rd, i, m_candidateCount);
        }
        rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                         DPI_SCALE(m_style.hilite_padding_y));
        _HighlightBack(rect, m_style.candidate_back_color, 0x00000000,
                       DPI_SCALE(m_style.round_corner), bkType, rd,
                       m_style.candidate_border_color);
        drawn = true;
      }
    }
    if (m_hoverIndex >= 0) {
      rect = m_layout->GetCandidateRect(m_hoverIndex);
      IsToRoundStruct rd = m_layout->GetRoundInfo(m_hoverIndex);
      if (m_istorepos) {
        rect.OffsetRect(0, m_offsetys[m_hoverIndex]);
        ReconfigRoundInfo(rd, m_hoverIndex, m_candidateCount);
      }
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      _SnapHighlightToPanel(rect, rd);
      _HighlightBack(rect, HALF_ALPHA_COLOR(m_style.hilited_candidate_back_color),
                     HALF_ALPHA_COLOR(m_style.hilited_candidate_shadow_color),
                     DPI_SCALE(m_style.round_corner), bkType, rd,
                     HALF_ALPHA_COLOR(m_style.hilited_candidate_border_color));
    }
    {
      rect = m_layout->GetHighlightRect();
      bool markSt = bar_scale_ == 1.0 || (!m_style.mark_text.empty());
      IsToRoundStruct rd = m_layout->GetRoundInfo(m_ctx.cinfo.highlighted);
      if (m_istorepos) {
        rect.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
        ReconfigRoundInfo(rd, m_ctx.cinfo.highlighted, m_candidateCount);
      }
      rect.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
      // mac 的多行高亮向下吃掉整行距(highlightedRect += linespace),
      // 高亮块之间零空隙;末行不吃(留给面板底边距)。不加的话
      // 行间露底色,第二行候选叠空隙读成"灰色堆叠"
      if (m_style.layout_type == UIStyle::LAYOUT_VERTICAL &&
          m_ctx.cinfo.highlighted + 1 < m_candidateCount) {
        // 扩到下一行文字顶(净空隙 = 行距 - 已扩的 padding),零空隙且不盖字
        rect.bottom += DPI_SCALE(m_style.candidate_spacing) -
                       DPI_SCALE(m_style.hilite_padding_y);
      }
      _SnapHighlightToPanel(rect, rd);
      _HighlightBack(rect, m_style.hilited_candidate_back_color,
                     markSt ? m_style.hilited_candidate_shadow_color : 0,
                     DPI_SCALE(m_style.round_corner), bkType, rd,
                     m_style.hilited_candidate_border_color);
      if (m_style.mark_text.empty() &&
          COLORNOTTRANSPARENT(m_style.hilited_mark_color)) {
        int height =
            min(rect.Height() - DPI_SCALE(m_style.hilite_padding_y) * 2,
                rect.Height() - DPI_SCALE(m_style.round_corner) * 2);
        int width = min(rect.Width() - DPI_SCALE(m_style.hilite_padding_x) * 2,
                        rect.Width() - DPI_SCALE(m_style.round_corner) * 2);
        width = min(width, static_cast<int>(rect.Width() * 0.618));
        height = min(height, static_cast<int>(rect.Height() * 0.618));
        if (bar_scale_ != 1.0f) {
          width = static_cast<int>(width * bar_scale_);
          height = static_cast<int>(height * bar_scale_);
        }
        int y = rect.top + (rect.Height() - height) / 2;
        CRect mkrc{rect.left, y, rect.left + m_layout->mark_width, y + height};
        _FillRoundRect(mkrc, ColorFFromARGB(m_style.hilited_mark_color),
                       (float)mkrc.Width() / 2.0f, IsToRoundStruct());
      }
      drawn = true;
    }
  } else {
    int label_text_color, candidate_text_color, comment_text_color;
    for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
      if (i == m_ctx.cinfo.highlighted || i == m_hoverIndex) {
        label_text_color = m_style.hilited_label_text_color;
        candidate_text_color = m_style.hilited_candidate_text_color;
        comment_text_color = m_style.hilited_comment_text_color;
      } else {
        label_text_color = m_style.label_text_color;
        candidate_text_color = m_style.candidate_text_color;
        comment_text_color = m_style.comment_text_color;
      }
      // label
      std::wstring label = m_layout->GetLabelText(
          labels, (int)i, m_style.label_text_format.c_str());
      if (!label.empty()) {
        rect = m_layout->GetCandidateLabelRect((int)i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        _TextOut(rect, label.c_str(), label.length(), label_text_color,
                 labeltxtFormat.Get());
      }
      // text
      std::wstring text = candidates.at(i).str;
      if (!text.empty()) {
        rect = m_layout->GetCandidateTextRect((int)i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        _TextOut(rect, text.c_str(), text.length(), candidate_text_color,
                 txtFormat.Get());
      }
      // comment
      std::wstring comment = comments.at(i).str;
      if (!comment.empty() && COLORNOTTRANSPARENT(comment_text_color)) {
        rect = m_layout->GetCandidateCommentRect((int)i);
        if (m_istorepos)
          rect.OffsetRect(0, m_offsetys[i]);
        _TextOut(rect, comment.c_str(), comment.length(), comment_text_color,
                 commenttxtFormat.Get());
      }
      drawn = true;
    }
    {
      if (!m_style.mark_text.empty() &&
          COLORNOTTRANSPARENT(m_style.hilited_mark_color)) {
        CRect rc = m_layout->GetHighlightRect();
        if (m_istorepos)
          rc.OffsetRect(0, m_offsetys[m_ctx.cinfo.highlighted]);
        rc.InflateRect(DPI_SCALE(m_style.hilite_padding_x),
                       DPI_SCALE(m_style.hilite_padding_y));
        int vgap = m_layout->mark_height
                       ? (rc.Height() - m_layout->mark_height) / 2
                       : 0;
        int hgap =
            m_layout->mark_width ? (rc.Width() - m_layout->mark_width) / 2 : 0;
        CRect hlRc;
        hlRc = CRect(rc.left + DPI_SCALE(m_style.hilite_padding_x),
                     rc.top + vgap,
                     rc.left + DPI_SCALE(m_style.hilite_padding_x) +
                         m_layout->mark_width,
                     rc.bottom - vgap);
        _TextOut(hlRc, m_style.mark_text.c_str(), m_style.mark_text.length(),
                 m_style.hilited_mark_color, pDWR->pTextFormat.Get());
      }
    }
  }
  return drawn;
}

// 供 DoPaint 使用的 32bpp top-down DIB，尺寸变化时重建
static void CreateDIB(HDC hdc, int w, int h, HBITMAP& bmp, HDC& memDC) {
  if (memDC)
    DeleteDC(memDC);
  if (bmp)
    DeleteObject(bmp);
  BITMAPINFO bi = {0};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = NULL;
  bmp = CreateDIBSection(hdc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
  memDC = CreateCompatibleDC(hdc);
  SelectObject(memDC, bmp);
}

// draw client area
void BangkePanel::DoPaint() {
  // turn off WS_EX_TRANSPARENT, for better resp performance
  LONG_PTR exStyle = GetWindowLongPtr(m_hWnd, GWL_EXSTYLE);
  SetWindowLongPtr(m_hWnd, GWL_EXSTYLE,
                   (exStyle & ~WS_EX_TRANSPARENT) | WS_EX_LAYERED);
  ::GetClientRect(m_hWnd, &rcw);
  // 异步刷新时窗口尺寸可能尚未跟上内容（AI 候选变宽/变高），
  // 位图按客户区与布局内容尺寸的较大者，避免新内容被裁成一角
  if (m_layout) {
    CSize content = m_layout->GetContentSize();
    if (content.cx > rcw.Width() || content.cy > rcw.Height())
      rcw.SetRect(rcw.left, rcw.top, rcw.left + content.cx, rcw.top + content.cy);

  }
  HDC hdc = ::GetDC(m_hWnd);
  HBITMAP memBitmap = NULL;
  HDC memDC = NULL;
  CreateDIB(hdc, rcw.Width(), rcw.Height(), memBitmap, memDC);
  ::ReleaseDC(m_hWnd, hdc);
  bool drawn = false;
  if (!hide_candidates) {
    CRect auxrc = m_layout->GetAuxiliaryRect();
    CRect preeditrc = m_layout->GetPreeditRect();
    if (m_istorepos) {
      CRect* rects = new CRect[m_candidateCount];
      int* btmys = new int[m_candidateCount];
      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        rects[i] = m_layout->GetCandidateRect(i);
        btmys[i] = rects[i].bottom;
      }
      if (m_candidateCount) {
        if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty())
          m_offsety_preedit =
              rects[m_candidateCount - 1].bottom - preeditrc.bottom;
        if (!m_ctx.aux.str.empty())
          m_offsety_aux = rects[m_candidateCount - 1].bottom - auxrc.bottom;
      } else {
        m_offsety_preedit = 0;
        m_offsety_aux = 0;
      }
      int base_gap = 0;
      if (!m_ctx.aux.str.empty())
        base_gap = auxrc.Height() + m_style.spacing;
      else if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty())
        base_gap = preeditrc.Height() + m_style.spacing;

      for (auto i = 0; i < m_candidateCount && i < MAX_CANDIDATES_COUNT; ++i) {
        if (i == 0)
          m_offsetys[i] =
              btmys[m_candidateCount - i - 1] - base_gap - rects[i].bottom;
        else
          m_offsetys[i] = (rects[i - 1].top + m_offsetys[i - 1] -
                           DPI_SCALE(m_style.candidate_spacing)) -
                          rects[i].bottom;
      }
      delete[] rects;
      delete[] btmys;
    }
    // 全部绘制走 D2D：BindDC 到 DIB 上
    if (FAILED(pDWR->pRenderTarget->BindDC(memDC, &rcw))) {
      _InitFontRes(true);
      pDWR->pRenderTarget->BindDC(memDC, &rcw);
    }
    pDWR->pRenderTarget->BeginDraw();
    pDWR->pRenderTarget->Clear(D2D1::ColorF(0, 0, 0, 0));
    // background and candidates back, hilite back drawing start
    if ((!m_ctx.empty() && !m_style.inline_preedit) ||
        (m_style.inline_preedit && (m_candidateCount || !m_ctx.aux.empty()))) {
      CRect backrc = m_layout->GetContentRect();
      _HighlightBack(backrc, m_style.back_color, m_style.shadow_color,
                     DPI_SCALE(m_style.round_corner_ex), BackType::BACKGROUND,
                     IsToRoundStruct(), m_style.border_color);
    }
    if (!m_ctx.aux.str.empty()) {
      if (m_istorepos)
        auxrc.OffsetRect(0, m_offsety_aux);
      drawn |= _DrawPreeditBack(m_ctx.aux, auxrc);
    }
    if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty()) {
      if (m_istorepos)
        preeditrc.OffsetRect(0, m_offsety_preedit);
      drawn |= _DrawPreeditBack(m_ctx.preedit, preeditrc);
    }
    if (m_candidateCount)
      drawn |= _DrawCandidates(true);
    // texts
    if (!m_ctx.aux.str.empty())
      drawn |= _DrawPreedit(m_ctx.aux, auxrc);
    if (!m_layout->IsInlinePreedit() && !m_ctx.preedit.str.empty())
      drawn |= _DrawPreedit(m_ctx.preedit, preeditrc);
    if (m_candidateCount)
      drawn |= _DrawCandidates();
    if (FAILED(pDWR->pRenderTarget->EndDraw())) {
      _InitFontRes(true);
      Refresh();
    }
  }
  // status icon
  if (!hide_candidates && m_layout && m_layout->ShouldDisplayStatusIcon()) {
    LoadIconNecessary(m_current_zhung_icon, m_style.current_zhung_icon,
                      m_iconEnabled, IDI_ZH);
    LoadIconNecessary(m_current_ascii_icon, m_style.current_ascii_icon,
                      m_iconAlpha, IDI_EN);
    LoadIconNecessary(m_current_half_icon, m_style.current_half_icon,
                      m_iconHalf, IDI_HALF_SHAPE);
    LoadIconNecessary(m_current_full_icon, m_style.current_full_icon,
                      m_iconFull, IDI_FULL_SHAPE);
    CRect iconRect(m_layout->GetStatusIconRect());
    if (m_istorepos && !m_ctx.aux.str.empty())
      iconRect.OffsetRect(0, m_offsety_aux);
    else if (m_istorepos && !m_layout->IsInlinePreedit() &&
             !m_ctx.preedit.str.empty())
      iconRect.OffsetRect(0, m_offsety_preedit);

    CIcon& icon(
        m_status.disabled ? m_iconDisabled
        : m_status.ascii_mode
            ? m_iconAlpha
            : (m_status.type == SCHEMA
                   ? m_iconEnabled
                   : (m_status.full_shape ? m_iconFull : m_iconHalf)));
    DrawIconEx(memDC, iconRect.left, iconRect.top, icon, 0, 0, 0, NULL,
               DI_NORMAL);
    drawn = true;
  }
  m_has_content = drawn;
  /* Nothing drawn, hide candidate window */
  if (!drawn)
    ::ShowWindow(m_hWnd, SW_HIDE);
  else {
    // _LayerUpdate
    HDC ScreenDC = ::GetDC(NULL);
    CRect rect;
    ::GetWindowRect(m_hWnd, &rect);
    POINT WindowPosAtScreen = {rect.left, rect.top};
    POINT PointOriginal = {0, 0};
    SIZE sz = {rcw.Width(), rcw.Height()};
    BLENDFUNCTION bf = {AC_SRC_OVER, 0,
                        (BYTE)max(0, min(255, m_style.window_alpha)),
                        AC_SRC_ALPHA};
    UpdateLayeredWindow(m_hWnd, ScreenDC, &WindowPosAtScreen, &sz, memDC,
                        &PointOriginal, RGB(0, 0, 0), &bf, ULW_ALPHA);
    ::ReleaseDC(NULL, ScreenDC);
  }

  // clean objs
  DeleteDC(memDC);
  DeleteObject(memBitmap);
}

// 由于某些软件并不依赖 WM_PAINT 消息来重绘，这里手动调用 DoPaint() 强制重绘
void BangkePanel::RedrawWindow() {
  if (!IsWindow())
    return;
  DoPaint();
}

void BangkePanel::MoveTo(RECT const& rc) {
  if (!m_layout)
    return;
  m_redraw_by_monitor_change = false;
  bool should_reset_sticky =
      (m_ctx.empty() || (abs(rc.left - m_inputPos.left) > 50) ||
       (abs(rc.bottom - m_inputPos.bottom) > 50));
  if (should_reset_sticky && m_sticky) {
    m_sticky = false;
    m_inputPos = rc;
    m_inputPos.OffsetRect(0, 6);
    _RepositionWindow(true);
    RedrawWindow();
    return;
  }
  if (m_style.ascii_tip_follow_cursor && m_ctx.empty() &&
      (!m_status.composing) && m_layout->ShouldDisplayStatusIcon()) {
    POINT p;
    ::GetCursorPos(&p);
    RECT irc{p.x - STATUS_ICON_SIZE, p.y - STATUS_ICON_SIZE, p.x, p.y};
    m_inputPos = irc;
    _RepositionWindow(true);
    RedrawWindow();
  } else if (!(rc.left == m_inputPos.left && rc.bottom != m_inputPos.bottom &&
               abs(rc.bottom - m_inputPos.bottom) < 6) ||
             m_layout->ShouldDisplayStatusIcon()) {
    m_inputPos = rc;
    m_inputPos.OffsetRect(0, 6);
    bool m_istorepos_buf = m_istorepos;
    _RepositionWindow(true);
    if (m_istorepos != m_istorepos_buf || !m_ctx.aux.empty() ||
        m_layout->ShouldDisplayStatusIcon() || m_redraw_by_monitor_change)
      RedrawWindow();
  }
}

void BangkePanel::_RepositionWindow(const bool& adj) {
  RECT rcWorkArea;
  memset(&rcWorkArea, 0, sizeof(rcWorkArea));
  HMONITOR hMonitor = MonitorFromRect(m_inputPos, MONITOR_DEFAULTTONEAREST);
  if (hMonitor) {
    MONITORINFO info;
    info.cbSize = sizeof(MONITORINFO);
    if (GetMonitorInfo(hMonitor, &info)) {
      rcWorkArea = info.rcWork;
    }
    if (hMonitor != m_hMonitor) {
      m_hMonitor = hMonitor;
      m_redraw_by_monitor_change = true;
    }
  }
  RECT rcWindow;
  ::GetWindowRect(m_hWnd, &rcWindow);
  int width = (rcWindow.right - rcWindow.left);
  int height = (rcWindow.bottom - rcWindow.top);
  rcWorkArea.right -= width;
  rcWorkArea.bottom -= height;
  int x = m_inputPos.left;
  int y = m_inputPos.bottom;
  if (DPI_SCALE(m_style.shadow_radius)) {
    x -= (DPI_SCALE(m_style.shadow_offset_x) >= 0 ||
          COLORTRANSPARENT(m_style.shadow_color))
             ? m_layout->offsetX
             : (m_layout->offsetX / 2);
    if (adj)
      y -= (DPI_SCALE(m_style.shadow_offset_y) > 0 ||
            COLORTRANSPARENT(m_style.shadow_color))
               ? m_layout->offsetY
               : (m_layout->offsetY / 2);
  }
  if (adj)
    m_istorepos = false;
  if (x > rcWorkArea.right)
    x = rcWorkArea.right;
  if (x < rcWorkArea.left)
    x = rcWorkArea.left;
  if (y > rcWorkArea.bottom || m_sticky) {
    if (!m_sticky)
      m_sticky = true;
    y = m_inputPos.top - height - 6;
    if (DPI_SCALE(m_style.shadow_radius) &&
        DPI_SCALE(m_style.shadow_offset_y) > 0)
      y -= DPI_SCALE(m_style.shadow_offset_y);
    m_istorepos = (m_style.vertical_auto_reverse &&
                   m_style.layout_type == UIStyle::LAYOUT_VERTICAL);
    if (DPI_SCALE(m_style.shadow_radius) > 0)
      y += (DPI_SCALE(m_style.shadow_offset_y) < 0 ||
            COLORTRANSPARENT(m_style.shadow_color))
               ? m_layout->offsetY
               : (m_layout->offsetY / 2);
  }
  if (y < rcWorkArea.top)
    y = rcWorkArea.top;
  m_inputPos.bottom = y;
  ::SetWindowPos(m_hWnd, HWND_TOPMOST, x, y, 0, 0,
                 SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
}

void BangkePanel::_TextOut(const CRect& rc,
                           const std::wstring& psz,
                           const size_t& cch,
                           const int& inColor,
                           IDWriteTextFormat1* const pTextFormat) {
  if (pTextFormat == NULL)
    return;
  // 空矩形 = 布局禁排(注释/标签被门控);D2D 绘制不裁剪,照画会溢出成残影
  if (rc.IsRectEmpty())
    return;
  float r = (float)(GetRValue(inColor)) / 255.0f;
  float g = (float)(GetGValue(inColor)) / 255.0f;
  float b = (float)(GetBValue(inColor)) / 255.0f;
  float alpha = (float)((inColor >> 24) & 255) / 255.0f;
  HRESULT hr = S_OK;
  if (pDWR->pBrush == NULL) {
    HR(pDWR->CreateBrush(D2D1::ColorF(r, g, b, alpha)));
  } else
    pDWR->SetBrushColor(D2D1::ColorF(r, g, b, alpha));

  HR(pDWR->CreateTextLayout(psz.c_str(), (int)cch, pTextFormat,
                            (float)rc.Width(), (float)rc.Height()));

  float offsetx = (float)rc.left;
  float offsety = (float)rc.top;
  DWRITE_OVERHANG_METRICS omt;
  HR(pDWR->GetLayoutOverhangMetrics(&omt));
  if (omt.left > 0)
    offsetx += omt.left;
  if (omt.top > 0)
    offsety += omt.top;

  if (pDWR->pTextLayout != NULL) {
    pDWR->DrawTextLayoutAt({offsetx, offsety});
  }
}
