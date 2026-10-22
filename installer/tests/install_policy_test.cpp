#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../BangkeInstaller/InstallPolicy.h"
#include "../BangkeBoot/PathList.h"
#include <cassert>
#include <iostream>

int main() {
  using namespace bangke::installer;
  assert(ParseVersion("0.4.13") > ParseVersion("0.4.9"));
  assert(ParseVersion("0.4.13.1200") > ParseVersion("0.4.13.1100"));
  assert(ParseVersion("1.2") == ParseVersion("1.2.0.0"));
  for (auto value : {"", "1..2", "1.2.", "1.2.3.4.5", "1.2.a", "-1", "999999999999999"})
    assert(!ParseVersion(value));
  assert(Succeeded(0) && Succeeded(3010) && Succeeded(1641));
  assert(!RequiresReboot(0) && RequiresReboot(3010));
  assert(!Succeeded(1603) && !Succeeded(-1));
  assert(StageIndex("UnregisterTSF", false) == -1);
  assert(StageIndex("RemoveFiles", false) == -1);
  assert(StageIndex("RegisterTSF", true) == -1);
  assert(StageIndex("UnregisterTSF", true) < StageIndex("Cleanup", true));
  assert(StageIndex("Cleanup", true) < StageIndex("RemoveFiles", true));
  assert(StageIndex("InstallFiles", false) < StageIndex("RegisterTSF", false));
  assert(StageIndex("RegisterTSF", false) < StageIndex("FirstDeploy", false));
  assert(StageIndex("nonsense", false) == -1);
  assert(ActionName("Action start 12:34:56: FirstDeploy.") == "FirstDeploy");
  assert(ActionName("操作开始 12:34:56: RegisterTSF.\r") == "RegisterTSF");
  assert(ActionName("操作开始 8:46:01: StartServer。") == "StartServer");
  assert(!ActionName("Property(S): FirstDeploy = whatever"));
  assert(!ActionName("Action ended 12:34:56: FirstDeploy. Return value 1."));
  assert(!ActionName("Action start 12:34:56: FirstDeploy"));
  assert(!AcceptStage(0, "InstallFinalize", false));
  assert(!AcceptStage(4, "InstallFiles", false));
  assert(AcceptStage(5, "InstallFinalize", false) == 7);
  assert(AcceptStage(3, "InstallFinalize", true) == 5);
  const std::wstring directory = L"C:\\Users\\bangke\\AppData\\Local\\Temp\\BangkeSetup-42";
  const auto paths = SinglePathList(directory);
  assert(paths.size() == directory.size() + 2);
  assert(paths.substr(0, directory.size()) == directory);
  assert(paths[directory.size()] == L'\0' && paths.back() == L'\0');
  assert(std::wstring(paths.c_str()) == directory);
  std::cout << "install policy: passed\n";
}
