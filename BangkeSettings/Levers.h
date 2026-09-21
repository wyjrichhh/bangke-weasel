#pragma once

#include <rime_api.h>
#include <rime_levers_api.h>

// rime_api.h 为 C API 的 int 语义把 Bool 定义成了宏，而宏会一并改写此后解析的
// 所有头里的同名标识符——Qt 的 QVariant::Type 枚举成员 Bool 就是这么被打成
// int 的（qvariant.h 报 C2144/C2589）。两个 rime 头到这里已解析完，换成语义等
// 价的类型别名：名字照旧能用，但不再越出本作用域。
#undef Bool
using Bool = int;

inline RimeLeversApi* leversApi() {
  RimeModule* module = rime_get_api()->find_module("levers");
  return module ? (RimeLeversApi*)module->get_api() : nullptr;
}
