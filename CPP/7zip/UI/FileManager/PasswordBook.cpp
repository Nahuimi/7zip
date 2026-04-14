#include "StdAfx.h"

#include "../../../../C/Md5.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/FileName.h"
#include "../../../Windows/Registry.h"

#include "PasswordBook.h"
#include "PasswordPluginApi.h"

using namespace NWindows;
using namespace NWindows::NFile;
using namespace NWindows::NFile::NFind;
using namespace NWindows::NFile::NIO;
using namespace NWindows::NRegistry;

namespace NPasswordBook {

static LPCTSTR const kPasswordBookRegPath =
    TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip");
static LPCTSTR const kPasswordBookEnabled = TEXT("UsePasswordBook");

typedef UINT (WINAPI *Func_GetApiVersion)();
typedef HRESULT (WINAPI *Func_EnsureDatabase)(LPCWSTR dbPath, BSTR *errorMessage);
typedef HRESULT (WINAPI *Func_LoadDatabase)(LPCWSTR dbPath, BOOL allowMissing, Z7PasswordPlugin_EnumCallback callback, void *callbackParam, UInt32 *invalidCount, BSTR *errorMessage);
typedef HRESULT (WINAPI *Func_SaveDatabase)(LPCWSTR dbPath, const Z7PasswordPlugin_EntryW *entries, UInt32 numEntries, BSTR *errorMessage);
typedef HRESULT (WINAPI *Func_LookupPassword)(LPCWSTR dbPath, LPCSTR md5, BSTR *password, BSTR *errorMessage);
typedef HRESULT (WINAPI *Func_StorePassword)(LPCWSTR dbPath, LPCSTR md5, LPCWSTR password, BSTR *errorMessage);

class CPlugin
{
  enum ELoadState
  {
    kNotLoaded,
    kLoaded,
    kFailed
  };

  ELoadState _state;
  NDLL::CLibrary _lib;
  UString _errorMessage;

public:
  Func_GetApiVersion GetApiVersion;
  Func_EnsureDatabase EnsureDatabase;
  Func_LoadDatabase LoadDatabase;
  Func_SaveDatabase SaveDatabase;
  Func_LookupPassword LookupPassword;
  Func_StorePassword StorePassword;

  CPlugin():
      _state(kNotLoaded),
      GetApiVersion(NULL),
      EnsureDatabase(NULL),
      LoadDatabase(NULL),
      SaveDatabase(NULL),
      LookupPassword(NULL),
      StorePassword(NULL)
      {}

  static void BstrToString_And_Free(BSTR bstr, UString &s)
  {
    s.Empty();
    if (bstr)
    {
      s = bstr;
      ::SysFreeString(bstr);
    }
  }

private:
  static void AddCandidate(CObjectVector<FString> &paths, const FString &path)
  {
    FOR_VECTOR (i, paths)
      if (paths[i] == path)
        return;
    paths.Add(path);
  }

  static FString GetParentDir(FString path)
  {
    if (path.IsEmpty())
      return path;
    if (IS_PATH_SEPAR(path.Back()))
      path.DeleteBack();
    const int pos = path.ReverseFind_PathSepar();
    if (pos < 0)
    {
      path.Empty();
      return path;
    }
    path.DeleteFrom((unsigned)pos + 1);
    return path;
  }

  static FString GetLastDirName(FString path)
  {
    if (path.IsEmpty())
      return path;
    if (IS_PATH_SEPAR(path.Back()))
      path.DeleteBack();
    const int pos = path.ReverseFind_PathSepar();
    if (pos >= 0)
      return path.Ptr((unsigned)pos + 1);
    return path;
  }

  void LoadNow()
  {
    _state = kFailed;
    _errorMessage = L"Password plugin DLL not found";

    const FString moduleDir = NDLL::GetModuleDirPrefix();
    const FString dllName = us2fs(UString(Z7_PASSWORD_PLUGIN_NAME_W));

    CObjectVector<FString> candidates;
    AddCandidate(candidates, moduleDir + dllName);

    const FString platformDir = GetLastDirName(moduleDir);
    const FString uiDir = GetParentDir(GetParentDir(moduleDir));
    if (!uiDir.IsEmpty() && !platformDir.IsEmpty())
      AddCandidate(candidates, uiDir + FTEXT("PasswordPlugin") FSTRING_PATH_SEPARATOR + platformDir + FSTRING_PATH_SEPARATOR + dllName);

    const FString rootDir = GetParentDir(uiDir);
    if (!rootDir.IsEmpty() && !platformDir.IsEmpty())
      AddCandidate(candidates, rootDir + FTEXT("UI") FSTRING_PATH_SEPARATOR FTEXT("PasswordPlugin") FSTRING_PATH_SEPARATOR + platformDir + FSTRING_PATH_SEPARATOR + dllName);

    FOR_VECTOR (i, candidates)
    {
      const FString &path = candidates[i];
      if (!DoesFileExist_Raw(path))
        continue;
      if (!_lib.Load(path))
      {
        _errorMessage = L"Cannot load password plugin DLL: ";
        _errorMessage += fs2us(path);
        continue;
      }
      break;
    }

    if (!_lib.IsLoaded())
      return;

    GetApiVersion = (Func_GetApiVersion)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_GetApiVersion");
    EnsureDatabase = (Func_EnsureDatabase)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_EnsureDatabase");
    LoadDatabase = (Func_LoadDatabase)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_LoadDatabase");
    SaveDatabase = (Func_SaveDatabase)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_SaveDatabase");
    LookupPassword = (Func_LookupPassword)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_LookupPassword");
    StorePassword = (Func_StorePassword)(void *)::GetProcAddress(_lib.Get_HMODULE(), "Z7PasswordPlugin_StorePassword");

    if (!GetApiVersion || !EnsureDatabase || !LoadDatabase || !SaveDatabase || !LookupPassword || !StorePassword)
    {
      _errorMessage = L"Password plugin DLL is missing required exports";
      return;
    }

    if (GetApiVersion() != Z7_PASSWORD_PLUGIN_API_VERSION)
    {
      _errorMessage = L"Password plugin DLL API version mismatch";
      return;
    }

    _errorMessage.Empty();
    _state = kLoaded;
  }

public:
  bool EnsureLoaded(UString &errorMessage)
  {
    if (_state == kNotLoaded)
      LoadNow();
    errorMessage = _errorMessage;
    return _state == kLoaded;
  }
};

static CPlugin &GetPlugin()
{
  static CPlugin plugin;
  return plugin;
}

static bool GetPlugin(CPlugin *&plugin, UString &errorMessage)
{
  plugin = &GetPlugin();
  return plugin->EnsureLoaded(errorMessage);
}

static bool SetErrorFromPluginCall(HRESULT res, BSTR errorBstr, UString &errorMessage)
{
  CPlugin::BstrToString_And_Free(errorBstr, errorMessage);
  if (res == S_OK || res == S_FALSE)
    return true;
  if (errorMessage.IsEmpty())
  {
    errorMessage = L"Password plugin call failed";
    if (res != E_FAIL)
    {
      errorMessage += L": 0x";
      char temp[16];
      ConvertUInt32ToHex((UInt32)res, temp);
      errorMessage += GetUnicodeString(temp);
    }
  }
  return false;
}

bool ReadEnabled()
{
  bool enabled = false;
  CKey key;
  if (key.Open(HKEY_CURRENT_USER, kPasswordBookRegPath, KEY_READ) == ERROR_SUCCESS)
    key.GetValue_bool_IfOk(kPasswordBookEnabled, enabled);
  return enabled;
}

void SaveEnabled(bool enabled)
{
  CKey key;
  key.Create(HKEY_CURRENT_USER, kPasswordBookRegPath);
  key.SetValue(kPasswordBookEnabled, enabled);
}

FString GetDatabasePath()
{
  return NDLL::GetModuleDirPrefix() + FTEXT("7zPasswordBook.db");
}

FString GetDefaultExchangePath()
{
  return NDLL::GetModuleDirPrefix() + FTEXT("7zPasswordBook.7zpb");
}

static BOOL WINAPI LoadEnumCallback(void *callbackParam, LPCSTR md5, LPCWSTR password)
{
  CDatabase *db = (CDatabase *)callbackParam;
  if (!db || !md5 || !password)
    return FALSE;
  db->SetPassword(AString(md5), UString(password));
  return TRUE;
}

int CDatabase::FindMd5(const AString &md5, unsigned &insertPos) const
{
  unsigned left = 0;
  unsigned right = _items.Size();
  while (left != right)
  {
    const unsigned mid = (left + right) / 2;
    const int compare = strcmp(md5, _items[mid].Md5);
    if (compare == 0)
    {
      insertPos = mid;
      return (int)mid;
    }
    if (compare < 0)
      right = mid;
    else
      left = mid + 1;
  }
  insertPos = left;
  return -1;
}

bool CDatabase::FindPassword(const AString &md5, UString &password) const
{
  password.Empty();
  unsigned insertPos = 0;
  const int index = FindMd5(md5, insertPos);
  if (index < 0)
    return false;
  password = _items[(unsigned)index].Password;
  return true;
}

void CDatabase::SetPassword(const AString &md5, const UString &password, bool *updated)
{
  if (updated)
    *updated = false;

  AString normalizedMd5 = md5;
  normalizedMd5.Trim();
  normalizedMd5.MakeLower_Ascii();
  if (normalizedMd5.Len() != MD5_DIGEST_SIZE * 2)
    return;

  unsigned insertPos = 0;
  const int index = FindMd5(normalizedMd5, insertPos);
  if (index >= 0)
  {
    _items[(unsigned)index].Password = password;
    if (updated)
      *updated = true;
    return;
  }

  CEntry &entry = _items.InsertNew(insertPos);
  entry.Md5 = normalizedMd5;
  entry.Password = password;
}

bool CDatabase::DeletePassword(const AString &md5)
{
  AString normalizedMd5 = md5;
  normalizedMd5.Trim();
  normalizedMd5.MakeLower_Ascii();

  unsigned insertPos = 0;
  const int index = FindMd5(normalizedMd5, insertPos);
  if (index < 0)
    return false;
  _items.Delete((unsigned)index);
  return true;
}

bool CDatabase::Load(const FString &path, bool allowMissing, CLoadStats *stats, UString &errorMessage)
{
  Clear();
  if (stats)
    *stats = CLoadStats();

  CPlugin *plugin = NULL;
  if (!GetPlugin(plugin, errorMessage))
    return false;

  UInt32 invalid = 0;
  BSTR errorBstr = NULL;
  const HRESULT res = plugin->LoadDatabase(fs2us(path), BoolToBOOL(allowMissing), LoadEnumCallback, this, &invalid, &errorBstr);
  if (!SetErrorFromPluginCall(res, errorBstr, errorMessage))
    return false;

  if (stats)
  {
    stats->Added = _items.Size();
    stats->Invalid = invalid;
  }
  errorMessage.Empty();
  return true;
}

bool CDatabase::Save(const FString &path, UString &errorMessage) const
{
  CPlugin *plugin = NULL;
  if (!GetPlugin(plugin, errorMessage))
    return false;

  CRecordVector<Z7PasswordPlugin_EntryW> entries;
  entries.Reserve(_items.Size());
  FOR_VECTOR (i, _items)
  {
    const CEntry &entry = _items[i];
    Z7PasswordPlugin_EntryW pluginEntry;
    pluginEntry.Md5 = entry.Md5;
    pluginEntry.Password = entry.Password;
    entries.AddInReserved(pluginEntry);
  }

  BSTR errorBstr = NULL;
  const HRESULT res = plugin->SaveDatabase(fs2us(path), entries.ConstData(), entries.Size(), &errorBstr);
  return SetErrorFromPluginCall(res, errorBstr, errorMessage);
}

bool EnsureDatabaseExists(UString &errorMessage)
{
  CPlugin *plugin = NULL;
  if (!GetPlugin(plugin, errorMessage))
    return false;

  BSTR errorBstr = NULL;
  const HRESULT res = plugin->EnsureDatabase(fs2us(GetDatabasePath()), &errorBstr);
  return SetErrorFromPluginCall(res, errorBstr, errorMessage);
}

bool LookupPassword(const AString &md5, UString &password)
{
  password.Empty();

  CPlugin *plugin = NULL;
  UString errorMessage;
  if (!GetPlugin(plugin, errorMessage))
    return false;

  BSTR passwordBstr = NULL;
  BSTR errorBstr = NULL;
  const HRESULT res = plugin->LookupPassword(fs2us(GetDatabasePath()), md5, &passwordBstr, &errorBstr);
  CPlugin::BstrToString_And_Free(errorBstr, errorMessage);
  if (res != S_OK)
  {
    if (passwordBstr)
      ::SysFreeString(passwordBstr);
    return false;
  }

  CPlugin::BstrToString_And_Free(passwordBstr, password);
  return !password.IsEmpty();
}

bool StorePassword(const AString &md5, const UString &password)
{
  CPlugin *plugin = NULL;
  UString errorMessage;
  if (!GetPlugin(plugin, errorMessage))
    return false;

  BSTR errorBstr = NULL;
  const HRESULT res = plugin->StorePassword(fs2us(GetDatabasePath()), md5, password, &errorBstr);
  return SetErrorFromPluginCall(res, errorBstr, errorMessage);
}

bool ComputeFileMd5(const FString &path, AString &md5Hex)
{
  md5Hex.Empty();

  CFileInfo fi;
  if (!fi.Find_FollowLink(path) || fi.IsDir())
    return false;

  CInFile file;
  if (!file.OpenShared(path, true))
    return false;

  CMd5 md5;
  Md5_Init(&md5);

  Byte buf[1 << 15];
  for (;;)
  {
    UInt32 processed = 0;
    if (!file.Read(buf, sizeof(buf), processed))
      return false;
    if (processed == 0)
      break;
    Md5_Update(&md5, buf, processed);
  }

  Byte digest[MD5_DIGEST_SIZE];
  Md5_Final(&md5, digest);

  char *dest = md5Hex.GetBuf(MD5_DIGEST_SIZE * 2);
  ConvertDataToHex_Lower(dest, digest, MD5_DIGEST_SIZE);
  md5Hex.ReleaseBuf_SetEnd(MD5_DIGEST_SIZE * 2);
  return true;
}

void CState::BeginArchive(const UString &archivePath)
{
  _enabled = ReadEnabled();
  _md5Defined = false;
  _autoPasswordWasUsed = false;
  _manualPasswordWasUsed = false;
  _savePending = false;
  _wrongPasswordDetected = false;
  _md5Hex.Empty();

  if (!_enabled || archivePath.IsEmpty())
    return;

  _md5Defined = ComputeFileMd5(us2fs(archivePath), _md5Hex);
}

bool CState::TryGetPassword(UString &password)
{
  password.Empty();
  if (!_enabled || !_md5Defined || _autoPasswordWasUsed || _manualPasswordWasUsed)
    return false;
  if (!LookupPassword(_md5Hex, password))
    return false;
  _autoPasswordWasUsed = true;
  return true;
}

void CState::SaveIfNeeded(const UString &password)
{
  if (_savePending && _md5Defined)
    StorePassword(_md5Hex, password);
  _savePending = false;
}

}
