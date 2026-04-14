#ifndef ZIP7_INC_PASSWORD_PLUGIN_API_H
#define ZIP7_INC_PASSWORD_PLUGIN_API_H

#include "../../../Common/MyWindows.h"

#define Z7_PASSWORD_PLUGIN_NAME_W L"7zPasswordPlugins.dll"
#define Z7_PASSWORD_PLUGIN_API_VERSION 1u

typedef BOOL (WINAPI *Z7PasswordPlugin_EnumCallback)(
    void *callbackParam,
    LPCSTR md5,
    LPCWSTR password);

struct Z7PasswordPlugin_EntryW
{
  LPCSTR Md5;
  LPCWSTR Password;
};

extern "C"
{
  UINT WINAPI Z7PasswordPlugin_GetApiVersion();

  HRESULT WINAPI Z7PasswordPlugin_EnsureDatabase(
      LPCWSTR dbPath,
      BSTR *errorMessage);

  HRESULT WINAPI Z7PasswordPlugin_LoadDatabase(
      LPCWSTR dbPath,
      BOOL allowMissing,
      Z7PasswordPlugin_EnumCallback callback,
      void *callbackParam,
      UInt32 *invalidCount,
      BSTR *errorMessage);

  HRESULT WINAPI Z7PasswordPlugin_SaveDatabase(
      LPCWSTR dbPath,
      const Z7PasswordPlugin_EntryW *entries,
      UInt32 numEntries,
      BSTR *errorMessage);

  HRESULT WINAPI Z7PasswordPlugin_LookupPassword(
      LPCWSTR dbPath,
      LPCSTR md5,
      BSTR *password,
      BSTR *errorMessage);

  HRESULT WINAPI Z7PasswordPlugin_StorePassword(
      LPCWSTR dbPath,
      LPCSTR md5,
      LPCWSTR password,
      BSTR *errorMessage);

  HRESULT WINAPI Z7PasswordPlugin_QueryOnlineByMd5(
      LPCSTR md5,
      BSTR *password,
      BSTR *errorMessage);
}

#endif
