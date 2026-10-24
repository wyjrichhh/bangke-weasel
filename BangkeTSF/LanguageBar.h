#pragma once
#include <msctf.h>
#include <BangkeUI.h>

class CLangBarItemButton : public ITfLangBarItemButton, public ITfSource {
 public:
  CLangBarItemButton(com_ptr<BangkeTSF> pTextService,
                     REFGUID guid,
                     bangke::UIStyle& style);
  ~CLangBarItemButton();

  /* IUnknown */
  STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject);
  STDMETHODIMP_(ULONG) AddRef();
  STDMETHODIMP_(ULONG) Release();

  /* ITfLangBarItem */
  STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo);
  STDMETHODIMP GetStatus(DWORD* pdwStatus);
  STDMETHODIMP Show(BOOL fShow);
  STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip);

  /* ITfLangBarItemButton */
  STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea);
  STDMETHODIMP InitMenu(ITfMenu* pMenu);
  STDMETHODIMP OnMenuSelect(UINT wID);
  STDMETHODIMP GetIcon(HICON* phIcon);
  STDMETHODIMP GetText(BSTR* pbstrText);

  /* ITfSource */
  STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie);
  STDMETHODIMP UnadviseSink(DWORD dwCookie);
  BOOL IsLangBarDisabled() { return (_status & TF_LBI_STATUS_DISABLED); }

  void UpdateBangkeStatus(bangke::Status stat);
  void SetLangbarStatus(DWORD dwStatus, BOOL fSet);

 private:
  static LRESULT CALLBACK ThemeWindowProc(HWND window, UINT message, WPARAM w, LPARAM l);
  HWND _themeWindow = nullptr;
  ATOM _themeClass = 0;
  std::wstring _themeClassName;
  GUID _guid;
  com_ptr<BangkeTSF> _pTextService;
  com_ptr<ITfLangBarItemSink> _pLangBarItemSink;
  LONG _cRef; /* COM Reference count */
  DWORD _status;
  bool ascii_mode;
  bangke::UIStyle& _style;
  std::wstring _current_schema_zhung_icon;
  std::wstring _current_schema_ascii_icon;
};
