// CodePageUtils.h

#ifndef ZIP7_INC_UI_COMMON_CODE_PAGE_UTILS_H
#define ZIP7_INC_UI_COMMON_CODE_PAGE_UTILS_H

#include <stdlib.h>

#include "../../../Common/NameCodePageUtils.h"
#include "../../../Common/IntToString.h"
#include "../../../Common/MyCom.h"

#include "../../../Windows/PropVariant.h"

#include "../../Archive/IArchive.h"
#include "ZipRegistry.h"

namespace NCodePageUtils {

static const char * const kForceCodePageEnvVar = "Z7_FORCE_CODEC";

static inline void GetCodePageSettingString(NNameCodePage::EMode mode,
    UInt32 codePage, UString &value)
{
  value.Empty();

  if (mode == NNameCodePage::kAuto)
  {
    value = L"auto";
    return;
  }

  if (mode == NNameCodePage::kSpecified)
  {
    wchar_t temp[16];
    ConvertUInt32ToString(codePage, temp);
    value = temp;
  }
}

static inline bool GetSavedCodePageInfo(NNameCodePage::EMode &mode, UInt32 &codePage)
{
  mode = NNameCodePage::kDefault;
  codePage = 0;

  UString value;
  if (!NNameCodePageRegistry::Load(value))
    return false;

  return NNameCodePage::ParseCodePageName(value, mode, codePage);
}

static inline void SaveSavedCodePageInfo(NNameCodePage::EMode mode, UInt32 codePage)
{
  UString value;
  GetCodePageSettingString(mode, codePage, value);
  NNameCodePageRegistry::Save(value);
}

static inline bool GetForcedCodePageInfo(NNameCodePage::EMode &mode, UInt32 &codePage)
{
  mode = NNameCodePage::kDefault;
  codePage = 0;

  char *value = NULL;
  size_t len = 0;
  if (_dupenv_s(&value, &len, kForceCodePageEnvVar) != 0 || value == NULL)
    return GetSavedCodePageInfo(mode, codePage);

  const bool isValid = NNameCodePage::ParseCodePageName(GetUnicodeString(value), mode, codePage);

  free(value);
  if (isValid)
    return true;

  return GetSavedCodePageInfo(mode, codePage);
}

static inline void SetForcedCodePage(UInt32 codePage)
{
  char value[16];
  ConvertUInt32ToString(codePage, value);
  _putenv_s(kForceCodePageEnvVar, value);
}

static inline void SetForcedCodePageAuto()
{
  _putenv_s(kForceCodePageEnvVar, "auto");
}

static inline void ClearForcedCodePage()
{
  _putenv_s(kForceCodePageEnvVar, "");
}

static inline void SetCurrentProcessCodePageInfo(NNameCodePage::EMode mode, UInt32 codePage)
{
  if (mode == NNameCodePage::kAuto)
    SetForcedCodePageAuto();
  else if (mode == NNameCodePage::kSpecified)
    SetForcedCodePage(codePage);
  else
    ClearForcedCodePage();
}

static inline bool GetForcedCodePage(UInt32 &codePage)
{
  NNameCodePage::EMode mode;
  if (!GetForcedCodePageInfo(mode, codePage))
    return false;
  return mode == NNameCodePage::kSpecified;
}

static inline void ApplyForcedCodePage(IUnknown *archive)
{
  NNameCodePage::EMode mode;
  UInt32 codePage;
  if (!archive || !GetForcedCodePageInfo(mode, codePage))
    return;

  CMyComPtr<ISetProperties> setProperties;
  archive->QueryInterface(IID_ISetProperties, (void **)&setProperties);
  if (!setProperties)
    return;

  const wchar_t *names[] =
  {
    L"cp"
  };
  const unsigned kNumProps = Z7_ARRAY_SIZE(names);
  NWindows::NCOM::CPropVariant values[kNumProps];
  if (mode == NNameCodePage::kAuto)
    values[0] = L"auto";
  else
    values[0] = codePage;

  setProperties->SetProperties(names, values, kNumProps);
}

}

#endif
