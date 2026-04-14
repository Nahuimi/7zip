#include "StdAfx.h"

#include "../../../../C/Md5.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/DLL.h"
#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/Registry.h"

#include "PasswordBook.h"

using namespace NWindows;
using namespace NWindows::NFile;
using namespace NWindows::NFile::NFind;
using namespace NWindows::NFile::NIO;
using namespace NWindows::NRegistry;

namespace NPasswordBook {

static LPCTSTR const kPasswordBookRegPath =
    TEXT("Software") TEXT(STRING_PATH_SEPARATOR) TEXT("7-Zip");
static LPCTSTR const kPasswordBookEnabled = TEXT("UsePasswordBook");
static const unsigned kPasswordBookMaxSize = 1 << 20;
static const Byte kUtf8Bom[3] = { 0xEF, 0xBB, 0xBF };

static void Set_FileError(UString &errorMessage, const char *message, const FString &path)
{
  errorMessage = GetUnicodeString(message);
  errorMessage += L": ";
  errorMessage += NError::MyFormatMessage(GetLastError_noZero_HRESULT());
  errorMessage += L": ";
  errorMessage += fs2us(path);
}

static bool IsValidMd5(const AString &md5)
{
  return md5.Len() == MD5_DIGEST_SIZE * 2 && FindNonHexChar(md5) == md5.Ptr(md5.Len());
}

static bool NormalizeMd5(const AString &src, AString &dest)
{
  dest = src;
  dest.Trim();
  dest.MakeLower_Ascii();
  return IsValidMd5(dest);
}

static bool NormalizeMd5(const UString &src, AString &dest)
{
  AString temp;
  temp.SetFromWStr_if_Ascii(src);
  if (temp.Len() != src.Len())
    return false;
  return NormalizeMd5(temp, dest);
}

static void EncodePassword(const UString &password, AString &encoded)
{
  AString utf8;
  ConvertUnicodeToUTF8(password, utf8);
  char *dest = encoded.GetBuf(utf8.Len() * 2);
  ConvertDataToHex_Lower(dest, (const Byte *)(const void *)utf8.Ptr(), utf8.Len());
  encoded.ReleaseBuf_SetEnd(utf8.Len() * 2);
}

static bool DecodePassword(const UString &encodedString, UString &password)
{
  password.Empty();

  AString encoded;
  encoded.SetFromWStr_if_Ascii(encodedString);
  if (encoded.Len() != encodedString.Len())
    return false;
  encoded.Trim();
  if ((encoded.Len() & 1) != 0)
    return false;
  if (FindNonHexChar(encoded) != encoded.Ptr(encoded.Len()))
    return false;

  const unsigned size = encoded.Len() / 2;
  CByteBuffer utf8Bytes;
  utf8Bytes.Alloc(size);
  if ((size_t)(ParseHexString(encoded, utf8Bytes) - (Byte *)utf8Bytes) != size)
    return false;

  AString utf8;
  if (size != 0)
  {
    char *buf = utf8.GetBuf(size);
    memcpy(buf, (const Byte *)utf8Bytes, size);
    utf8.ReleaseBuf_SetEnd(size);
  }
  return ConvertUTF8ToUnicode(utf8, password);
}

static bool ReadFile_Utf8(const FString &path, UString &text, bool allowMissing, UString &errorMessage)
{
  text.Empty();
  errorMessage.Empty();

  CInFile file;
  if (!file.OpenShared(path, true))
  {
    const DWORD error = ::GetLastError();
    if (allowMissing && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND))
      return true;
    Set_FileError(errorMessage, "Cannot open password book", path);
    return false;
  }

  UInt64 length64 = 0;
  if (!file.GetLength(length64))
  {
    Set_FileError(errorMessage, "Cannot get password book size", path);
    return false;
  }
  if (length64 > kPasswordBookMaxSize)
  {
    errorMessage = L"Password book file is too large: ";
    errorMessage += fs2us(path);
    return false;
  }

  const unsigned length = (unsigned)length64;
  AString utf8;
  size_t processed = 0;
  if (length != 0)
  {
    char *buf = utf8.GetBuf(length);
    if (!file.ReadFull(buf, length, processed) || processed != length)
    {
      Set_FileError(errorMessage, "Cannot read password book", path);
      return false;
    }
    utf8.ReleaseBuf_SetEnd(length);
  }

  if (utf8.Len() >= 3
      && (Byte)utf8[0] == 0xEF
      && (Byte)utf8[1] == 0xBB
      && (Byte)utf8[2] == 0xBF)
    utf8.DeleteFrontal(3);

  if (!ConvertUTF8ToUnicode(utf8, text))
  {
    errorMessage = L"Password book is not valid UTF-8: ";
    errorMessage += fs2us(path);
    return false;
  }
  return true;
}

static bool WriteFile_Utf8(const FString &path, const UString &text, UString &errorMessage)
{
  errorMessage.Empty();

  AString utf8;
  ConvertUnicodeToUTF8(text, utf8);

  COutFile file;
  if (!file.Create_ALWAYS(path))
  {
    Set_FileError(errorMessage, "Cannot create password book", path);
    return false;
  }

  if (!file.WriteFull(kUtf8Bom, sizeof(kUtf8Bom)))
  {
    Set_FileError(errorMessage, "Cannot write password book", path);
    return false;
  }
  if (utf8.Len() != 0)
    if (!file.WriteFull(utf8, utf8.Len()))
    {
      Set_FileError(errorMessage, "Cannot write password book", path);
      return false;
    }
  if (!file.Close())
  {
    Set_FileError(errorMessage, "Cannot close password book", path);
    return false;
  }
  return true;
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

  AString normalizedMd5;
  if (!NormalizeMd5(md5, normalizedMd5))
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
  AString normalizedMd5;
  if (!NormalizeMd5(md5, normalizedMd5))
    return false;

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

  UString text;
  if (!ReadFile_Utf8(path, text, allowMissing, errorMessage))
    return false;

  unsigned start = 0;
  while (start < text.Len())
  {
    int end = text.Find(L'\n', start);
    if (end < 0)
      end = (int)text.Len();

    UString line = text.Mid(start, (unsigned)end - start);
    line.Trim();

    if (!line.IsEmpty() && line[0] != '#')
    {
      int sep = line.Find(L' ');
      if (sep < 0)
        sep = line.Find(L'\t');

      bool ok = false;
      if (sep >= 0)
      {
        AString md5;
        UString password;
        if (NormalizeMd5(line.Left(sep), md5) && DecodePassword(line.Ptr((unsigned)sep + 1), password))
        {
          bool updated = false;
          SetPassword(md5, password, &updated);
          if (stats)
          {
            if (updated)
              stats->Updated++;
            else
              stats->Added++;
          }
          ok = true;
        }
      }
      if (!ok && stats)
        stats->Invalid++;
    }

    start = (unsigned)end;
    if (start < text.Len() && text[start] == L'\n')
      start++;
  }

  errorMessage.Empty();
  return true;
}

bool CDatabase::Save(const FString &path, UString &errorMessage) const
{
  UString text;
  FOR_VECTOR (i, _items)
  {
    const CEntry &entry = _items[i];
    AString encodedPassword;
    EncodePassword(entry.Password, encodedPassword);

    text += GetUnicodeString(entry.Md5);
    text.Add_Space();
    text += GetUnicodeString(encodedPassword);
    text += L"\r\n";
  }
  return WriteFile_Utf8(path, text, errorMessage);
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

bool EnsureDatabaseExists(UString &errorMessage)
{
  const FString path = GetDatabasePath();
  if (DoesFileExist_Raw(path))
  {
    errorMessage.Empty();
    return true;
  }

  CDatabase db;
  return db.Save(path, errorMessage);
}

bool LookupPassword(const AString &md5, UString &password)
{
  password.Empty();

  CDatabase db;
  UString errorMessage;
  if (!db.Load(GetDatabasePath(), true, NULL, errorMessage))
    return false;

  AString normalizedMd5;
  if (!NormalizeMd5(md5, normalizedMd5))
    return false;

  return db.FindPassword(normalizedMd5, password);
}

bool StorePassword(const AString &md5, const UString &password)
{
  CDatabase db;
  UString errorMessage;
  if (!db.Load(GetDatabasePath(), true, NULL, errorMessage))
    return false;
  db.SetPassword(md5, password);
  return db.Save(GetDatabasePath(), errorMessage);
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
