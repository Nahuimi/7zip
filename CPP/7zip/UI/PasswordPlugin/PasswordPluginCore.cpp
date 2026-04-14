#include "StdAfx.h"

#include "../../../../C/Md5.h"
#include "../../../../C/sqlite3.h"

#include "../../../Common/IntToString.h"
#include "../../../Common/StringConvert.h"
#include "../../../Common/StringToInt.h"
#include "../../../Common/UTFConvert.h"

#include "../../../Windows/ErrorMsg.h"
#include "../../../Windows/FileDir.h"
#include "../../../Windows/FileFind.h"
#include "../../../Windows/FileIO.h"
#include "../../../Windows/Registry.h"

#include "../FileManager/PasswordBook.h"

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
static const char kSqliteHeader[] = "SQLite format 3";

enum EDbKind
{
  kDbKind_Missing,
  kDbKind_Sqlite,
  kDbKind_Other
};

static void Set_FileError(UString &errorMessage, const char *message, const FString &path)
{
  errorMessage = GetUnicodeString(message);
  errorMessage += L": ";
  errorMessage += NError::MyFormatMessage(GetLastError_noZero_HRESULT());
  errorMessage += L": ";
  errorMessage += fs2us(path);
}

static void Set_NotSqliteError(UString &errorMessage, const FString &path)
{
  errorMessage = L"Password book file is not a SQLite database: ";
  errorMessage += fs2us(path);
}

static void Set_SqliteError(UString &errorMessage, sqlite3 *db, const char *message, const FString *path)
{
  errorMessage = GetUnicodeString(message);

  if (db)
  {
    UString dbMessage;
    const char *s = sqlite3_errmsg(db);
    if (s && *s)
    {
      AString a(s);
      if (!ConvertUTF8ToUnicode(a, dbMessage))
        dbMessage = GetUnicodeString(s);
      if (!dbMessage.IsEmpty())
      {
        errorMessage += L": ";
        errorMessage += dbMessage;
      }
    }
  }

  if (path)
  {
    errorMessage += L": ";
    errorMessage += fs2us(*path);
  }
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

static void ConvertUtf8ToUnicode_Safe(const char *src, UString &dest)
{
  dest.Empty();
  if (!src)
    return;

  AString a(src);
  if (!ConvertUTF8ToUnicode(a, dest))
    dest = GetUnicodeString(src);
}

static EDbKind DetectDbKind(const FString &path, UString &errorMessage)
{
  errorMessage.Empty();

  CInFile file;
  if (!file.OpenShared(path, true))
  {
    const DWORD error = ::GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
      return kDbKind_Missing;
    Set_FileError(errorMessage, "Cannot open password book database", path);
    return kDbKind_Other;
  }

  char header[16];
  size_t processed = 0;
  if (!file.ReadFull(header, sizeof(header), processed))
  {
    Set_FileError(errorMessage, "Cannot read password book database", path);
    return kDbKind_Other;
  }

  if (processed >= sizeof(kSqliteHeader))
    if (memcmp(header, kSqliteHeader, sizeof(kSqliteHeader)) == 0)
      return kDbKind_Sqlite;

  return kDbKind_Other;
}

static bool SqliteExec(sqlite3 *db, const char *sql, const char *message, const FString *path, UString &errorMessage)
{
  char *errMsg = NULL;
  const int rc = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
  if (rc == SQLITE_OK)
    return true;

  errorMessage = GetUnicodeString(message);
  if (errMsg && *errMsg)
  {
    UString dbMessage;
    ConvertUtf8ToUnicode_Safe(errMsg, dbMessage);
    if (!dbMessage.IsEmpty())
    {
      errorMessage += L": ";
      errorMessage += dbMessage;
    }
  }
  else
    Set_SqliteError(errorMessage, db, message, path);

  if (path)
  {
    errorMessage += L": ";
    errorMessage += fs2us(*path);
  }

  if (errMsg)
    sqlite3_free(errMsg);
  return false;
}

static bool OpenDatabase(const FString &path, bool createSchema, sqlite3 **db, UString &errorMessage)
{
  *db = NULL;
  const UString pathU = fs2us(path);
  const int rc = sqlite3_open16((const void *)(const wchar_t *)pathU, db);
  if (rc != SQLITE_OK)
  {
    Set_SqliteError(errorMessage, *db, "Cannot open password book database", &path);
    if (*db)
    {
      sqlite3_close(*db);
      *db = NULL;
    }
    return false;
  }

  sqlite3_busy_timeout(*db, 3000);

  if (createSchema)
  {
    if (!SqliteExec(*db,
        "CREATE TABLE IF NOT EXISTS password_book ("
        "md5 TEXT NOT NULL PRIMARY KEY,"
        "password TEXT NOT NULL"
        ");",
        "Cannot initialize password book database",
        &path,
        errorMessage))
    {
      sqlite3_close(*db);
      *db = NULL;
      return false;
    }
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

  const EDbKind kind = DetectDbKind(path, errorMessage);
  if (kind == kDbKind_Missing)
  {
    errorMessage.Empty();
    return allowMissing;
  }

  if (kind != kDbKind_Sqlite)
  {
    if (errorMessage.IsEmpty())
      Set_NotSqliteError(errorMessage, path);
    return false;
  }

  sqlite3 *db = NULL;
  if (!OpenDatabase(path, true, &db, errorMessage))
    return false;

  sqlite3_stmt *stmt = NULL;
  const int rc = sqlite3_prepare_v2(
      db,
      "SELECT md5, password FROM password_book ORDER BY md5 COLLATE NOCASE;",
      -1,
      &stmt,
      NULL);
  if (rc != SQLITE_OK)
  {
    Set_SqliteError(errorMessage, db, "Cannot read password book database", &path);
    sqlite3_close(db);
    return false;
  }

  bool ok = true;
  for (;;)
  {
    const int stepRc = sqlite3_step(stmt);
    if (stepRc == SQLITE_DONE)
      break;
    if (stepRc != SQLITE_ROW)
    {
      Set_SqliteError(errorMessage, db, "Cannot read password book database", &path);
      ok = false;
      break;
    }

    const unsigned char *md5Text = sqlite3_column_text(stmt, 0);
    const unsigned char *passwordText = sqlite3_column_text(stmt, 1);
    if (!md5Text || !passwordText)
    {
      if (stats)
        stats->Invalid++;
      continue;
    }

    AString md5((const char *)(const void *)md5Text);
    UString password;
    ConvertUtf8ToUnicode_Safe((const char *)(const void *)passwordText, password);

    if (!NormalizeMd5(md5, md5))
    {
      if (stats)
        stats->Invalid++;
      continue;
    }

    SetPassword(md5, password);
    if (stats)
      stats->Added++;
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);
  return ok;
}

bool CDatabase::Save(const FString &path, UString &errorMessage) const
{
  UString dummy;
  const EDbKind kind = DetectDbKind(path, dummy);
  if (kind == kDbKind_Other)
    NDir::DeleteFileAlways(path);

  sqlite3 *db = NULL;
  if (!OpenDatabase(path, true, &db, errorMessage))
    return false;

  bool ok = SqliteExec(db, "BEGIN IMMEDIATE;", "Cannot update password book database", &path, errorMessage);

  if (ok)
    ok = SqliteExec(db, "DELETE FROM password_book;", "Cannot update password book database", &path, errorMessage);

  sqlite3_stmt *stmt = NULL;
  if (ok)
  {
    const int rc = sqlite3_prepare_v2(
        db,
        "INSERT INTO password_book(md5, password) VALUES(?1, ?2);",
        -1,
        &stmt,
        NULL);
    if (rc != SQLITE_OK)
    {
      Set_SqliteError(errorMessage, db, "Cannot update password book database", &path);
      ok = false;
    }
  }

  if (ok)
  {
    FOR_VECTOR (i, _items)
    {
      const CEntry &entry = _items[i];
      AString passwordUtf8;
      ConvertUnicodeToUTF8(entry.Password, passwordUtf8);

      sqlite3_reset(stmt);
      sqlite3_clear_bindings(stmt);

      if (sqlite3_bind_text(stmt, 1, entry.Md5, entry.Md5.Len(), SQLITE_TRANSIENT) != SQLITE_OK
          || sqlite3_bind_text(stmt, 2, passwordUtf8, passwordUtf8.Len(), SQLITE_TRANSIENT) != SQLITE_OK)
      {
        Set_SqliteError(errorMessage, db, "Cannot update password book database", &path);
        ok = false;
        break;
      }

      if (sqlite3_step(stmt) != SQLITE_DONE)
      {
        Set_SqliteError(errorMessage, db, "Cannot update password book database", &path);
        ok = false;
        break;
      }
    }
  }

  if (stmt)
    sqlite3_finalize(stmt);

  if (ok)
    ok = SqliteExec(db, "COMMIT;", "Cannot update password book database", &path, errorMessage);
  else
    SqliteExec(db, "ROLLBACK;", "Cannot update password book database", &path, dummy);

  sqlite3_close(db);
  return ok;
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
  return FString();
}

FString GetDefaultExchangePath()
{
  return FString();
}

bool EnsureDatabaseExists(UString &errorMessage)
{
  CDatabase db;
  return db.Save(GetDatabasePath(), errorMessage);
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
  db.Load(GetDatabasePath(), true, NULL, errorMessage);
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
