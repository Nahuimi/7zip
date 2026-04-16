#ifndef ZIP7_INC_PASSWORD_BOOK_H
#define ZIP7_INC_PASSWORD_BOOK_H

#include "../../../Common/MyString.h"
#include "../../../Common/MyVector.h"

namespace NPasswordBook {

struct CEntry
{
  AString Md5;
  UString Password;
};

struct CLoadStats
{
  unsigned Added;
  unsigned Updated;
  unsigned Invalid;

  CLoadStats():
      Added(0),
      Updated(0),
      Invalid(0)
      {}
};

class CDatabase
{
  CObjectVector<CEntry> _items;

  int FindMd5(const AString &md5, unsigned &insertPos) const;

public:
  void Clear() { _items.Clear(); }
  unsigned Size() const { return _items.Size(); }
  const CObjectVector<CEntry> &Items() const { return _items; }

  bool FindPassword(const AString &md5, UString &password) const;
  void SetPassword(const AString &md5, const UString &password, bool *updated = NULL);
  bool DeletePassword(const AString &md5);

  bool Load(const FString &path, bool allowMissing, CLoadStats *stats, UString &errorMessage);
  bool Save(const FString &path, UString &errorMessage) const;
};

bool ReadEnabled();
void SaveEnabled(bool enabled);

FString GetDatabasePath();
FString GetDefaultExchangePath();

bool EnsureDatabaseExists(UString &errorMessage);
bool LookupPassword(const AString &md5, UString &password);
bool StorePassword(const AString &md5, const UString &password);
bool ComputeFileMd5(const FString &path, AString &md5Hex);
bool LookupPassword_Direct(const FString &path, const AString &md5, UString &password, UString &errorMessage);
bool StorePassword_Direct(const FString &path, const AString &md5, const UString &password, UString &errorMessage);
bool QueryExtensionPassword_Direct(const FString &archivePath, const AString &md5, UString &password, UString &errorMessage);
bool LoadCsv(const FString &path, CDatabase &db, CLoadStats *stats, UString &errorMessage);
bool SaveCsv(const FString &path, const CDatabase &db, UString &errorMessage);

class CState
{
  struct CAutoPasswordCandidate
  {
    UString Password;
    bool FromLocal;
    bool FromOnline;

    CAutoPasswordCandidate():
        FromLocal(false),
        FromOnline(false)
        {}
  };

  bool _enabled;
  bool _md5Defined;
  bool _autoCandidatesLoaded;
  bool _currentPasswordIsAuto;
  bool _manualPasswordWasUsed;
  bool _savePending;
  bool _wrongPasswordDetected;
  AString _md5Hex;
  FString _archivePath;
  UString _archiveFileName;
  UInt64 _archiveSizeBytes;
  bool _archiveSizeDefined;
  unsigned _nextAutoPasswordIndex;
  CObjectVector<CAutoPasswordCandidate> _autoCandidates;

  void AddAutoCandidate(const UString &password, bool fromLocal, bool fromOnline);
  void LoadAutoCandidates();
  bool EnsureMd5();

public:
  CState():
      _enabled(false),
      _md5Defined(false),
      _autoCandidatesLoaded(false),
      _currentPasswordIsAuto(false),
      _manualPasswordWasUsed(false),
      _savePending(false),
      _wrongPasswordDetected(false),
      _archiveSizeBytes(0),
      _archiveSizeDefined(false),
      _nextAutoPasswordIndex(0)
      {}

  void BeginArchive(const UString &archivePath);
  bool TryGetPassword(UString &password);
  void NoteManualPassword();
  void NoteWrongPassword()
  {
    _wrongPasswordDetected = true;
    _currentPasswordIsAuto = false;
    _savePending = false;
  }
  void SaveIfNeeded(const UString &password);
  bool WasAutoPasswordUsed() const { return _currentPasswordIsAuto; }
  bool NeedRetry() const
    { return (_wrongPasswordDetected && !_manualPasswordWasUsed && _nextAutoPasswordIndex < _autoCandidates.Size()); }
};

}

#endif
