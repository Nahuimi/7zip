#include "StdAfx.h"

#include "CompactEncDet.h"

#include "CompactEncDet/compact_enc_det.h"

using namespace CompactEncDet;

namespace NCompactEncDet {

static bool IsCjkCodePage(UInt32 codePage)
{
  switch (codePage)
  {
    case 932:
    case 936:
    case 949:
    case 950:
    case 51932:
    case 50220:
    case 50225:
    case 54936:
      return true;
    default:
      return false;
  }
}

static UInt32 ConvertEncodingToCodePage(Encoding encoding)
{
  switch ((unsigned)encoding)
  {
    case UTF8:
      return CP_UTF8;

    case JAPANESE_SHIFT_JIS:
    case JAPANESE_CP932:
    case KDDI_SHIFT_JIS:
    case DOCOMO_SHIFT_JIS:
    case SOFTBANK_SHIFT_JIS:
      return 932;

    case JAPANESE_EUC_JP:
      return 51932;

    case JAPANESE_JIS:
    case KDDI_ISO_2022_JP:
    case SOFTBANK_ISO_2022_JP:
      return 50220;

    case CHINESE_GB:
    case GBK:
      return 936;

    case GB18030:
      return 54936;

    case CHINESE_BIG5:
    case CHINESE_BIG5_CP950:
    case BIG5_HKSCS:
      return 950;

    case KOREAN_EUC_KR:
      return 949;

    case ISO_2022_KR:
      return 50225;

    case RUSSIAN_CP1251:
      return 1251;

    case RUSSIAN_CP866:
      return 866;

    case MSFT_CP1252:
      return 1252;

    case MSFT_CP1250:
      return 1250;

    case MSFT_CP1253:
      return 1253;

    case MSFT_CP1254:
      return 1254;

    case MSFT_CP1255:
      return 1255;

    case MSFT_CP1256:
      return 1256;

    case MSFT_CP1257:
      return 1257;

    case ISO_8859_11:
    case MSFT_CP874:
      return 874;

    case CZECH_CP852:
      return 852;

    default:
      return 0;
  }
}

bool DetectCodePage(const CObjectVector<AString> &samples,
    UInt32 &codePage, bool &isReliable)
{
  codePage = 0;
  isReliable = false;

  AString data;
  UInt64 totalBytes = 0;
  UInt64 highBytes = 0;

  FOR_VECTOR (i, samples)
  {
    const AString &s = samples[i];
    if (s.IsEmpty())
      continue;

    if (!data.IsEmpty())
      data.Add_Char('\n');

    data += s;
    totalBytes += s.Len();

    for (unsigned k = 0; k < s.Len(); k++)
      if ((Byte)s[k] >= 0x80)
        highBytes++;

    if (data.Len() >= (1 << 15))
      break;
  }

  if (data.IsEmpty() || highBytes == 0)
    return false;

  int bytesConsumed = 0;
  bool reliable = false;
  const Encoding encoding = CompactEncDet::DetectEncoding(
      data, (int)data.Len(),
      NULL, NULL, NULL,
      UNKNOWN_ENCODING,
      UNKNOWN_LANGUAGE,
      CompactEncDet::QUERY_CORPUS,
      true,
      &bytesConsumed,
      &reliable);

  const UInt32 detectedCodePage = ConvertEncodingToCodePage(encoding);
  if (detectedCodePage == 0)
    return false;

  if (!reliable)
  {
    if (!IsCjkCodePage(detectedCodePage))
      return false;

    if (totalBytes < 16 || highBytes < 4)
      return false;
  }

  codePage = detectedCodePage;
  isReliable = reliable;
  return true;
}

}


#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable : 4018)
  #pragma warning(disable : 4061)
  #pragma warning(disable : 4062)
  #pragma warning(disable : 4127)
  #pragma warning(disable : 4244)
  #pragma warning(disable : 4267)
  #pragma warning(disable : 4310)
  #pragma warning(disable : 4334)
  #pragma warning(disable : 4996)
  #pragma warning(disable : 4701)
  #pragma warning(disable : 4706)
#endif

#include "CompactEncDet/util/encodings/encodings.cc"
#include "CompactEncDet/util/languages/languages.cc"
#include "CompactEncDet/compact_enc_det_hint_code.cc"
#include "CompactEncDet/compact_enc_det.cc"

#if defined(_MSC_VER)
  #pragma warning(pop)
#endif
