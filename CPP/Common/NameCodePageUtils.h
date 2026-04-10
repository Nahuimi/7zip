#ifndef ZIP7_INC_COMMON_NAME_CODE_PAGE_UTILS_H
#define ZIP7_INC_COMMON_NAME_CODE_PAGE_UTILS_H

#include "MyString.h"
#include "MyVector.h"
#include "CompactEncDet.h"
#include "StringConvert.h"
#include "UTFConvert.h"

#include "../Windows/PropVariant.h"

namespace NNameCodePage {

enum EMode
{
  kDefault,
  kSpecified,
  kAuto
};

static inline UInt32 ResolveCodePage(UInt32 codePage)
{
#ifdef _WIN32
  if (codePage == CP_ACP)
    return (UInt32)GetACP();
  if (codePage == CP_OEMCP)
    return (UInt32)GetOEMCP();
#endif
  return codePage;
}

static inline bool ParseCodePageName(const UString &name, EMode &mode, UInt32 &codePage)
{
  mode = kDefault;
  codePage = 0;

  if (name.IsEmpty())
    return false;

  UString s(name);
  s.MakeLower_Ascii();

  if (s.IsEqualTo_Ascii_NoCase("auto"))
  {
    mode = kAuto;
    return true;
  }

  if (s.IsEqualTo_Ascii_NoCase("utf-8") || s.IsEqualTo_Ascii_NoCase("utf8"))
  {
    mode = kSpecified;
    codePage = CP_UTF8;
    return true;
  }

  if (s.IsEqualTo_Ascii_NoCase("win") ||
      s.IsEqualTo_Ascii_NoCase("acp") ||
      s.IsEqualTo_Ascii_NoCase("ansi"))
  {
    mode = kSpecified;
    codePage = CP_ACP;
    return true;
  }

  if (s.IsEqualTo_Ascii_NoCase("dos") ||
      s.IsEqualTo_Ascii_NoCase("oem") ||
      s.IsEqualTo_Ascii_NoCase("oemcp"))
  {
    mode = kSpecified;
    codePage = CP_OEMCP;
    return true;
  }

  UInt32 parsed = 0;
  bool isNumber = !s.IsEmpty();
  for (unsigned i = 0; i < s.Len(); i++)
  {
    const wchar_t c = s[i];
    if (c < L'0' || c > L'9')
    {
      isNumber = false;
      break;
    }

    const UInt32 digit = (UInt32)(c - L'0');
    if (parsed > ((UInt32)0xFFFFFFFF / 10))
    {
      isNumber = false;
      break;
    }
    parsed *= 10;
    if (parsed > (UInt32)0xFFFFFFFF - digit)
    {
      isNumber = false;
      break;
    }
    parsed += digit;
  }

  if (isNumber && parsed != 0)
  {
    mode = kSpecified;
    codePage = parsed;
    return true;
  }

  return false;
}

static inline HRESULT ParseCodePageProp(const PROPVARIANT &prop,
    UInt32 defaultCodePage, EMode &mode, UInt32 &codePage)
{
  mode = kSpecified;
  codePage = defaultCodePage;

  switch (prop.vt)
  {
    case VT_EMPTY:
      return S_OK;
    case VT_UI4:
      if (prop.ulVal == 0)
        return E_INVALIDARG;
      codePage = prop.ulVal;
      return S_OK;
    case VT_I4:
      if (prop.lVal <= 0)
        return E_INVALIDARG;
      codePage = (UInt32)prop.lVal;
      return S_OK;
    case VT_BSTR:
      if (!prop.bstrVal)
        return E_INVALIDARG;
      return ParseCodePageName(prop.bstrVal, mode, codePage) ? S_OK : E_INVALIDARG;
    default:
      return E_INVALIDARG;
  }
}

static inline bool HasNonAsciiBytes(const AString &s)
{
  for (unsigned i = 0; i < s.Len(); i++)
    if ((Byte)s[i] >= 0x80)
      return true;
  return false;
}

static inline unsigned CountHighBytes(const AString &s)
{
  unsigned num = 0;
  for (unsigned i = 0; i < s.Len(); i++)
    if ((Byte)s[i] >= 0x80)
      num++;
  return num;
}

static inline bool IsDbcsCodePage(UInt32 codePage)
{
  switch (ResolveCodePage(codePage))
  {
    case 932:
    case 936:
    case 949:
    case 950:
      return true;
    default:
      return false;
  }
}

static inline bool IsLikelyDbcsTrail(Byte c)
{
  return (c >= 0x40 && c != 0x7f);
}

static inline int GetUnicodeCharScore(UInt32 c)
{
  if (c < 0x20 || (c >= 0x7f && c < 0xa0))
    return -14;

  if (c == 0xfffd)
    return -40;

  if (c >= 0xd800 && c < 0xe000)
    return -18;

  if ((c >= 0xe000 && c <= 0xf8ff) || c == 0xfffe || c == 0xffff)
    return -16;

  if (c < 0x80)
    return 0;

  if ((c >= 0x3400 && c <= 0x4dbf) ||
      (c >= 0x4e00 && c <= 0x9fff) ||
      (c >= 0xf900 && c <= 0xfaff) ||
      (c >= 0x3040 && c <= 0x30ff) ||
      (c >= 0x3100 && c <= 0x312f) ||
      (c >= 0x3130 && c <= 0x318f) ||
      (c >= 0xac00 && c <= 0xd7af) ||
      (c >= 0xff66 && c <= 0xff9d))
    return 8;

  if ((c >= 0x0400 && c <= 0x052f) ||
      (c >= 0x0370 && c <= 0x03ff) ||
      (c >= 0x0590 && c <= 0x06ff) ||
      (c >= 0x0e00 && c <= 0x0e7f))
    return 5;

  if ((c >= 0x00a0 && c <= 0x024f) ||
      (c >= 0x1e00 && c <= 0x1eff) ||
      (c >= 0xff01 && c <= 0xff60))
    return 3;

  if ((c >= 0x2190 && c <= 0x21ff) ||
      (c >= 0x2300 && c <= 0x23ff) ||
      (c >= 0x2500 && c <= 0x259f) ||
      (c >= 0x25a0 && c <= 0x27bf) ||
      (c >= 0x2800 && c <= 0x28ff))
    return -6;

  return 1;
}

struct CScore
{
  Int64 Value;
  UInt32 StrongChars;

  void Clear()
  {
    Value = 0;
    StrongChars = 0;
  }

  CScore() { Clear(); }
};

static inline void UpdateScoreForString(CScore &score, UInt32 codePage, const AString &src)
{
  if (!HasNonAsciiBytes(src))
    return;

  const unsigned highBytes = CountHighBytes(src);
  const UInt32 resolvedCodePage = ResolveCodePage(codePage);

  if (resolvedCodePage == CP_UTF8)
  {
    CUtf8Check utfCheck;
    utfCheck.Check_AString(src);
    if (utfCheck.IsOK())
      score.Value += 80 + (Int64)highBytes * 2;
    else
    {
      if (utfCheck.NonUtf) score.Value -= 120;
      if (utfCheck.Truncated) score.Value -= 30;
      if (utfCheck.ZeroChar) score.Value -= 80;
      if (utfCheck.SingleSurrogate) score.Value -= 80;
    }
  }

#ifdef _WIN32
  if (IsDbcsCodePage(resolvedCodePage))
  {
    UInt32 goodPairs = 0;
    UInt32 badLeads = 0;
    for (unsigned i = 0; i < src.Len(); i++)
    {
      const Byte b = (Byte)src[i];
      if (!IsDBCSLeadByteEx(resolvedCodePage, b))
        continue;
      if (i + 1 < src.Len() && IsLikelyDbcsTrail((Byte)src[i + 1]))
      {
        goodPairs++;
        i++;
      }
      else
        badLeads++;
    }
    score.Value += (Int64)goodPairs * 12;
    score.Value -= (Int64)badLeads * 18;
  }
#endif

  UString decoded;
  try
  {
    MultiByteToUnicodeString2(decoded, src, resolvedCodePage);
  }
  catch (...)
  {
    score.Value -= 200;
    return;
  }

  for (unsigned i = 0; i < decoded.Len(); i++)
  {
    const int charScore = GetUnicodeCharScore((UInt32)decoded[i]);
    score.Value += charScore;
    if (charScore >= 5)
      score.StrongChars++;
  }

  try
  {
    bool defaultCharWasUsed = false;
    const AString roundTrip = UnicodeStringToMultiByte(decoded, resolvedCodePage, '_', defaultCharWasUsed);
    if (!defaultCharWasUsed && roundTrip == src)
      score.Value += 18;
    else
      score.Value -= 28;
  }
  catch (...)
  {
    score.Value -= 80;
  }
}

static inline bool DetectCodePage_Heuristic(const CObjectVector<AString> &samples,
    UInt32 defaultCodePage, UInt32 &detectedCodePage)
{
  detectedCodePage = 0;

  bool hasNonAscii = false;
  UInt64 totalBytes = 0;
  UInt64 highBytes = 0;

  FOR_VECTOR (i, samples)
  {
    const AString &s = samples[i];
    totalBytes += s.Len();
    const unsigned numHighBytes = CountHighBytes(s);
    highBytes += numHighBytes;
    if (numHighBytes != 0)
      hasNonAscii = true;
  }

  if (!hasNonAscii)
    return false;

  static const UInt32 kCandidateCodePages[] =
  {
    CP_UTF8,
    CP_ACP,
    CP_OEMCP,
    932,
    936,
    949,
    950,
    1252,
    437,
    850,
    852,
    866,
    874,
    1250,
    1251,
    1253,
    1254,
    1255,
    1256,
    1257,
    1258
  };

  UInt32 candidates[Z7_ARRAY_SIZE(kCandidateCodePages) + 1];
  unsigned numCandidates = 0;

  const UInt32 resolvedDefault = ResolveCodePage(defaultCodePage);
  candidates[numCandidates++] = resolvedDefault;

  for (unsigned i = 0; i < Z7_ARRAY_SIZE(kCandidateCodePages); i++)
  {
    const UInt32 cp = ResolveCodePage(kCandidateCodePages[i]);
    bool exists = false;
    for (unsigned j = 0; j < numCandidates; j++)
      if (candidates[j] == cp)
      {
        exists = true;
        break;
      }
    if (!exists)
      candidates[numCandidates++] = cp;
  }

  CScore scores[Z7_ARRAY_SIZE(kCandidateCodePages) + 1];

  for (unsigned i = 0; i < numCandidates; i++)
    scores[i].Clear();

  FOR_VECTOR (sampleIndex, samples)
  {
    const AString &sample = samples[sampleIndex];
    for (unsigned cpIndex = 0; cpIndex < numCandidates; cpIndex++)
      UpdateScoreForString(scores[cpIndex], candidates[cpIndex], sample);
  }

  const bool sparseHighBytes = (highBytes * 100 < totalBytes * 12);
  for (unsigned i = 0; i < numCandidates; i++)
  {
    if (IsDbcsCodePage(candidates[i]) && sparseHighBytes)
      scores[i].Value -= 24;
    if (scores[i].StrongChars == 0)
      scores[i].Value -= 6;
  }

  unsigned defaultIndex = 0;
  unsigned bestIndex = 0;
  unsigned secondIndex = 0;
  for (unsigned i = 0; i < numCandidates; i++)
  {
    if (scores[bestIndex].Value < scores[i].Value)
    {
      secondIndex = bestIndex;
      bestIndex = i;
    }
    else if (i != bestIndex && (secondIndex == bestIndex || scores[secondIndex].Value < scores[i].Value))
      secondIndex = i;
    if (candidates[i] == resolvedDefault)
      defaultIndex = i;
  }

  if (candidates[bestIndex] == resolvedDefault)
    return false;

  const Int64 defaultScore = scores[defaultIndex].Value;
  const Int64 bestScore = scores[bestIndex].Value;
  const Int64 secondScore =
      (secondIndex == bestIndex) ? -((Int64)1 << 60) : scores[secondIndex].Value;

  if (bestScore < defaultScore + 24)
    return false;
  if (secondScore > bestScore - 8)
    return false;
  if (scores[bestIndex].StrongChars == 0 && bestScore < defaultScore + 40)
    return false;

  detectedCodePage = candidates[bestIndex];
  return true;
}

static inline bool DetectCodePage(const CObjectVector<AString> &samples,
    UInt32 defaultCodePage, UInt32 &detectedCodePage)
{
  detectedCodePage = 0;

  bool isReliable = false;
  UInt32 codePage = 0;
  if (NCompactEncDet::DetectCodePage(samples, codePage, isReliable))
  {
    if (ResolveCodePage(codePage) != ResolveCodePage(defaultCodePage))
    {
      detectedCodePage = codePage;
      return true;
    }
  }

  return DetectCodePage_Heuristic(samples, defaultCodePage, detectedCodePage);
}

}

#endif
