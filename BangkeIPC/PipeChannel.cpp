#include "stdafx.h"

#include <PipeChannel.h>
#include <BangkeUtility.h>

using namespace bangke;
using namespace std;
using namespace boost;

// 客户端单次管道 IO 的等待上限;server 正常响应在毫秒级,
// 触发上限即按坏连接处理(断连重连),不再冻结宿主应用
static const DWORD kPipeIoTimeoutMs = 3000;

#define _ThrowLastError throw ::GetLastError()
#define _ThrowCode(__c) throw __c
#define _ThrowIfNot(__c)                 \
  {                                      \
    DWORD err;                           \
    if ((err = ::GetLastError()) != __c) \
      throw err;                         \
  }

PipeChannelBase::PipeChannelBase(std::wstring&& pn_cmd,
                                 size_t bs,
                                 SECURITY_ATTRIBUTES* s,
                                 bool overlapped)
    : pname(pn_cmd), buff_size(bs), io_overlapped(overlapped), sa(s) {};

PipeChannelBase::~PipeChannelBase() {
  // Thread-specific pointers are cleaned up automatically
}

bool PipeChannelBase::_Ensure() {
  try {
    HANDLE* phandle = _GetPipeHandle();
    if (_Invalid(*phandle)) {
      *phandle = _Connect(pname.c_str());
      return !_Invalid(*phandle);
    }
  } catch (...) {
    return false;
  }

  return true;
}

// 无界等待是应用冻结源:server 不在时 WaitNamedPipe 立即失败,
// 原先的 while 会原地忙转;有界重试(≤4s)后照常抛错走断连路径
HANDLE PipeChannelBase::_Connect(const wchar_t* name) {
  HANDLE pipe = INVALID_HANDLE_VALUE;
  for (int retry = 0; _Invalid(pipe = _TryConnect());) {
    if (!::WaitNamedPipe(name, 500) || ++retry >= 8)
      _ThrowLastError;
  }
  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe, &mode, NULL, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}

void PipeChannelBase::_Reconnect() {
  HANDLE* phandle = _GetPipeHandle();
  _FinalizePipe(*phandle);
  _Ensure();
}

HANDLE PipeChannelBase::_TryConnect() {
  auto pipe = ::CreateFile(pname.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                           OPEN_EXISTING,
                           io_overlapped ? FILE_FLAG_OVERLAPPED : 0, NULL);
  if (!_Invalid(pipe)) {
    // connected to the pipe
    return pipe;
  }
  // being busy is not really an error since we just need to wait.
  _ThrowIfNot(ERROR_PIPE_BUSY);
  // All pipe instances are busy
  return INVALID_HANDLE_VALUE;
}

// OVERLAPPED 读写带超时:句柄必须以 FILE_FLAG_OVERLAPPED 创建
// (在同步句柄上 overlapped 调用立即失败,p4 曾因此回滚)。
// 超时即 CancelIoEx 并置 ERROR_TIMEOUT,由既有异常路径断连
static bool _IoWithTimeout(HANDLE pipe,
                           bool is_write,
                           void* buf,
                           DWORD len,
                           DWORD timeout_ms,
                           DWORD* transferred) {
  OVERLAPPED ov{};
  ov.hEvent = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  if (!ov.hEvent)
    return false;
  BOOL ok = is_write ? ::WriteFile(pipe, buf, len, transferred, &ov)
                     : ::ReadFile(pipe, buf, len, transferred, &ov);
  if (!ok && ::GetLastError() != ERROR_IO_PENDING) {
    ::CloseHandle(ov.hEvent);
    return false;
  }
  if (!ok || *transferred == 0) {
    // pending:等待有界时间
    if (WaitForSingleObject(ov.hEvent, timeout_ms) != WAIT_OBJECT_0) {
      ::CancelIoEx(pipe, NULL);
      ::GetOverlappedResult(pipe, &ov, transferred, FALSE);
      ::CloseHandle(ov.hEvent);
      ::SetLastError(ERROR_TIMEOUT);
      return false;
    }
    if (!::GetOverlappedResult(pipe, &ov, transferred, FALSE)) {
      // 这条分支不显式设 last error，上层 _ThrowLastError 抛出来的可能就是 0 ——
      // 症状是"抛了个错误码 0"，看不出所以然。这里把真实状态记下来。
      BangkeDiagTrace(std::string("_IoWithTimeout: GetOverlappedResult failed, is_write=") +
                      (is_write ? "1" : "0") + " transferred=" +
                      std::to_string(*transferred) + " err=" +
                      std::to_string(GetLastError()));
      ::CloseHandle(ov.hEvent);
      return false;
    }
  }
  ::CloseHandle(ov.hEvent);
  return *transferred > 0 || !is_write;
}

size_t PipeChannelBase::_WritePipe(HANDLE pipe, size_t s, char* b) {
  DWORD lwritten = 0;
  bool ok = io_overlapped
                ? _IoWithTimeout(pipe, true, b, (DWORD)s, kPipeIoTimeoutMs,
                                 &lwritten)
                : (::WriteFile(pipe, b, s, &lwritten, NULL) && lwritten > 0);
  if (!ok || lwritten <= 0) {
    BangkeDiagTrace("_WritePipe FAILED want=" + std::to_string(s) +
                    " ok=" + (ok ? "1" : "0") + " written=" +
                    std::to_string(lwritten) + " err=" +
                    std::to_string(GetLastError()));
    _ThrowLastError;
  }
  ::FlushFileBuffers(pipe);
  return lwritten;
}

void PipeChannelBase::_FinalizePipe(HANDLE& p) {
  if (!_Invalid(p)) {
    DisconnectNamedPipe(p);
    CloseHandle(p);
  }
  p = INVALID_HANDLE_VALUE;
}

void PipeChannelBase::_Receive(HANDLE pipe, LPVOID msg, size_t rec_len) {
  DWORD lread = 0;
  bool success =
      io_overlapped
          ? _IoWithTimeout(pipe, false, msg, (DWORD)rec_len,
                           kPipeIoTimeoutMs, &lread)
          : (::ReadFile(pipe, msg, rec_len, &lread, NULL) != FALSE);
  auto ctx = _GetContext();
  // 消息头与请求体写在同一段字节里:只有消息比 rec_len 长(短读)才说明这次带了体。
  // 没带体时缓冲区里留着更早那一次请求的内容,拿它解析会读到过期数据
  ctx->body_received = !success;
  if (!success) {
    const DWORD first_err = GetLastError();
    BangkeDiagTrace("_Receive: short read rec_len=" + std::to_string(rec_len) +
                    " read=" + std::to_string(lread) + " err=" +
                    std::to_string(first_err));
    _ThrowIfNot(ERROR_MORE_DATA);

    memset(ctx->buffer.get(), 0, buff_size);
    success =
        io_overlapped
            ? _IoWithTimeout(pipe, false, ctx->buffer.get(),
                             (DWORD)buff_size, kPipeIoTimeoutMs, &lread)
            : (::ReadFile(pipe, ctx->buffer.get(), buff_size, &lread, NULL) !=
               FALSE);
    if (!success) {
      BangkeDiagTrace("_Receive: body read FAILED read=" +
                      std::to_string(lread) + " err=" +
                      std::to_string(GetLastError()));
      _ThrowLastError;
    }
    BangkeDiagTrace("_Receive: body read ok bytes=" + std::to_string(lread));
  }
  ctx->has_body = false;
}

HANDLE PipeChannelBase::_ConnectServerPipe(std::wstring& pn) {
  HANDLE pipe =
      CreateNamedPipe(pn.c_str(), PIPE_ACCESS_DUPLEX,
                      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                      PIPE_UNLIMITED_INSTANCES, buff_size, buff_size, 0, sa);
  if (pipe == INVALID_HANDLE_VALUE || !::ConnectNamedPipe(pipe, NULL)) {
    _ThrowLastError;
  }
  return pipe;
}
