// CodePageUtils.h

#ifndef ZIP7_INC_UI_COMMON_CODE_PAGE_UTILS_H
#define ZIP7_INC_UI_COMMON_CODE_PAGE_UTILS_H

#include "../../../Common/IntToString.h"

#include "../../../Windows/PropVariant.h"

#include "../../Archive/IArchive.h"

namespace NCodePageUtils {

static const char * const kForceCodePageEnvVar = "Z7_FORCE_CODEC";

static inline bool GetForcedCodePage(UInt32 &codePage)
{
  codePage = 0;

  char *value = NULL;
  size_t len = 0;
  if (_dupenv_s(&value, &len, kForceCodePageEnvVar) != 0 || value == NULL)
    return false;

  char *end = NULL;
  const unsigned long parsed = strtoul(value, &end, 10);
  const bool isValid =
      end != value
      && *end == 0
      && parsed != 0
      && parsed <= (unsigned long)(UInt32)0xFFFFFFFF;

  free(value);

  if (!isValid)
    return false;

  codePage = (UInt32)parsed;
  return true;
}

static inline void SetForcedCodePage(UInt32 codePage)
{
  char value[16];
  ConvertUInt32ToString(codePage, value);
  _putenv_s(kForceCodePageEnvVar, value);
}

static inline void ClearForcedCodePage()
{
  _putenv_s(kForceCodePageEnvVar, "");
}

static inline void ApplyForcedCodePage(IUnknown *archive)
{
  UInt32 codePage;
  if (!archive || !GetForcedCodePage(codePage))
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
  NWindows::NCOM::CPropVariant values[kNumProps] =
  {
    codePage
  };

  setProperties->SetProperties(names, values, kNumProps);
}

}

#endif
