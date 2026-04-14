#include "StdAfx.h"

#include "../../../Common/MyCom.h"
#include "../../../Common/StringConvert.h"

#include "../FileManager/PasswordBook.h"
#include "../FileManager/PasswordPluginApi.h"

using namespace NPasswordBook;

static HRESULT SetBstrResult(const UString &s, BSTR *dest)
{
  if (!dest)
    return S_OK;
  *dest = NULL;
  return StringToBstr(s, dest);
}

static void SetEmpty(BSTR *value)
{
  if (value)
    *value = NULL;
}

extern "C"
UINT WINAPI Z7PasswordPlugin_GetApiVersion()
{
  return Z7_PASSWORD_PLUGIN_API_VERSION;
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_EnsureDatabase(
    LPCWSTR dbPath,
    BSTR *errorMessage)
{
  SetEmpty(errorMessage);
  if (!dbPath)
    return E_INVALIDARG;

  CDatabase db;
  UString error;
  if (!db.Save(us2fs(dbPath), error))
  {
    SetBstrResult(error, errorMessage);
    return E_FAIL;
  }
  return S_OK;
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_LoadDatabase(
    LPCWSTR dbPath,
    BOOL allowMissing,
    Z7PasswordPlugin_EnumCallback callback,
    void *callbackParam,
    UInt32 *invalidCount,
    BSTR *errorMessage)
{
  SetEmpty(errorMessage);
  if (invalidCount)
    *invalidCount = 0;
  if (!dbPath)
    return E_INVALIDARG;

  CDatabase db;
  CLoadStats stats;
  UString error;
  if (!db.Load(us2fs(dbPath), allowMissing != FALSE, &stats, error))
  {
    SetBstrResult(error, errorMessage);
    return E_FAIL;
  }

  if (invalidCount)
    *invalidCount = stats.Invalid;

  if (callback)
  {
    FOR_VECTOR (i, db.Items())
    {
      const CEntry &entry = db.Items()[i];
      if (!callback(callbackParam, entry.Md5, entry.Password))
        return E_ABORT;
    }
  }

  return S_OK;
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_SaveDatabase(
    LPCWSTR dbPath,
    const Z7PasswordPlugin_EntryW *entries,
    UInt32 numEntries,
    BSTR *errorMessage)
{
  SetEmpty(errorMessage);
  if (!dbPath)
    return E_INVALIDARG;
  if (numEntries != 0 && !entries)
    return E_INVALIDARG;

  CDatabase db;
  for (UInt32 i = 0; i < numEntries; i++)
  {
    const Z7PasswordPlugin_EntryW &entry = entries[i];
    if (!entry.Md5 || !entry.Password)
      continue;
    db.SetPassword(AString(entry.Md5), UString(entry.Password));
  }

  UString error;
  if (!db.Save(us2fs(dbPath), error))
  {
    SetBstrResult(error, errorMessage);
    return E_FAIL;
  }

  return S_OK;
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_LookupPassword(
    LPCWSTR dbPath,
    LPCSTR md5,
    BSTR *password,
    BSTR *errorMessage)
{
  SetEmpty(password);
  SetEmpty(errorMessage);
  if (!dbPath || !md5)
    return E_INVALIDARG;

  CDatabase db;
  UString error;
  if (!db.Load(us2fs(dbPath), TRUE, NULL, error))
  {
    SetBstrResult(error, errorMessage);
    return E_FAIL;
  }

  UString result;
  if (!db.FindPassword(AString(md5), result))
    return S_FALSE;

  return SetBstrResult(result, password);
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_StorePassword(
    LPCWSTR dbPath,
    LPCSTR md5,
    LPCWSTR password,
    BSTR *errorMessage)
{
  SetEmpty(errorMessage);
  if (!dbPath || !md5 || !password)
    return E_INVALIDARG;

  CDatabase db;
  UString error;
  db.Load(us2fs(dbPath), TRUE, NULL, error);
  db.SetPassword(AString(md5), UString(password));
  if (!db.Save(us2fs(dbPath), error))
  {
    SetBstrResult(error, errorMessage);
    return E_FAIL;
  }

  return S_OK;
}

extern "C"
HRESULT WINAPI Z7PasswordPlugin_QueryOnlineByMd5(
    LPCSTR md5,
    BSTR *password,
    BSTR *errorMessage)
{
  UNUSED_VAR(md5)
  SetEmpty(password);
  SetEmpty(errorMessage);
  return E_NOTIMPL;
}
