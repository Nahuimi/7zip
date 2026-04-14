#include "StdAfx.h"

#include "../../../Common/StringConvert.h"

#include "../../../Windows/FileFind.h"

#include "../GUI/ExtractDialogRes.h"

#include "BrowseDialog.h"
#include "FormatUtils.h"
#include "HelpUtils.h"
#include "LangUtils.h"
#include "PasswordBookPage.h"
#include "PasswordBookPageRes.h"
#include "resource.h"

using namespace NWindows;
using namespace NWindows::NFile::NFind;

static const char * const kPasswordBookTopic = "fm/options.htm#passwordbook";

#ifdef Z7_LANG
static const UInt32 kLangIDs[] =
{
  IDX_PASSWORD_BOOK_ENABLE,
  IDT_PASSWORD_BOOK_SEARCH,
  IDB_PASSWORD_BOOK_DELETE,
  IDB_PASSWORD_BOOK_IMPORT,
  IDB_PASSWORD_BOOK_EXPORT
};
#endif

static bool ContainsText_AsciiNoCase(UString text, UString filter)
{
  text.MakeLower_Ascii();
  filter.MakeLower_Ascii();
  return text.Find(filter) >= 0;
}

bool CPasswordBookPage::MatchesFilter(const NPasswordBook::CEntry &entry, const UString &filter) const
{
  if (filter.IsEmpty())
    return true;
  return ContainsText_AsciiNoCase(GetUnicodeString(entry.Md5), filter)
      || ContainsText_AsciiNoCase(entry.Password, filter);
}

void CPasswordBookPage::RefreshStatus()
{
  UString value;
  value.Add_UInt32(_db.Size());
  UString s = MyFormatNew(IDS_PASSWORD_BOOK_RECORDS, value);
  if (_loadStats.Invalid != 0)
  {
    value.Empty();
    value.Add_UInt32(_loadStats.Invalid);
    if (!s.IsEmpty())
      s += L"   ";
    s += MyFormatNew(IDS_PASSWORD_BOOK_INVALID_IGNORED, value);
  }
  SetItemText(IDT_PASSWORD_BOOK_STATUS, s);
}

void CPasswordBookPage::RefreshList()
{
  UString filter;
  GetItemText(IDE_PASSWORD_BOOK_SEARCH, filter);
  filter.Trim();

  _listView.SetRedraw(false);
  _listView.DeleteAllItems();

  unsigned row = 0;
  FOR_VECTOR (i, _db.Items())
  {
    const NPasswordBook::CEntry &entry = _db.Items()[i];
    if (!MatchesFilter(entry, filter))
      continue;

    UString md5 = GetUnicodeString(entry.Md5);
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = (int)row;
    item.iSubItem = 0;
    item.lParam = (LPARAM)i;
    item.pszText = (LPWSTR)(void *)(const wchar_t *)md5;
    _listView.InsertItem(&item);
    _listView.SetSubItem(row, 1, entry.Password);
    row++;
  }

  if (row != 0)
    _listView.SetItemState_FocusedSelected(0);

  _listView.SetRedraw(true);
  _listView.InvalidateRect(NULL, true);
  RefreshStatus();
}

void CPasswordBookPage::ImportData()
{
  CObjectVector<CBrowseFilterInfo> filters;
  {
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(IDS_PASSWORD_BOOK_FILTER);
    f.Description += L" (*.7zpb;*.db)";
    f.Masks.Add(UString(L"*.7zpb"));
    f.Masks.Add(UString(L"*.db"));
  }
  {
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(4071);
    f.Description += L" (*.*)";
    f.Masks.Add(UString(L"*.*"));
  }

  CBrowseInfo browseInfo;
  browseInfo.hwndOwner = *this;
  browseInfo.FilePath = fs2us(NPasswordBook::GetDefaultExchangePath());

  if (!browseInfo.BrowseForFile(filters))
    return;

  NPasswordBook::CDatabase importedDb;
  NPasswordBook::CLoadStats loadStats;
  UString errorMessage;
  if (!importedDb.Load(us2fs(browseInfo.FilePath), false, &loadStats, errorMessage))
  {
    MessageBoxW(*this, errorMessage, L"7-Zip", MB_OK | MB_ICONERROR);
    return;
  }

  unsigned added = 0;
  unsigned updated = 0;
  FOR_VECTOR (i, importedDb.Items())
  {
    bool wasUpdated = false;
    const NPasswordBook::CEntry &entry = importedDb.Items()[i];
    _db.SetPassword(entry.Md5, entry.Password, &wasUpdated);
    if (wasUpdated)
      updated++;
    else
      added++;
  }

  _dataChanged = true;
  Changed();
  RefreshList();

  UString value;
  UString message;
  value.Add_UInt32(added);
  message = MyFormatNew(IDS_PASSWORD_BOOK_IMPORTED_NEW, value);
  value.Empty();
  value.Add_UInt32(updated);
  message.Add_LF();
  message += MyFormatNew(IDS_PASSWORD_BOOK_IMPORTED_UPDATED, value);
  if (loadStats.Invalid != 0)
  {
    value.Empty();
    value.Add_UInt32(loadStats.Invalid);
    message.Add_LF();
    message += MyFormatNew(IDS_PASSWORD_BOOK_IMPORTED_INVALID, value);
  }
  MessageBoxW(*this, message, L"7-Zip", MB_OK | MB_ICONINFORMATION);
}

void CPasswordBookPage::ExportData()
{
  CObjectVector<CBrowseFilterInfo> filters;
  {
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(IDS_PASSWORD_BOOK_FILTER);
    f.Description += L" (*.7zpb)";
    f.Masks.Add(UString(L"*.7zpb"));
  }
  {
    CBrowseFilterInfo &f = filters.AddNew();
    f.Description = LangString(4071);
    f.Description += L" (*.*)";
    f.Masks.Add(UString(L"*.*"));
  }

  CBrowseInfo browseInfo;
  browseInfo.hwndOwner = *this;
  browseInfo.SaveMode = true;
  browseInfo.FilePath = fs2us(NPasswordBook::GetDefaultExchangePath());

  if (!browseInfo.BrowseForFile(filters))
    return;

  UString errorMessage;
  if (!_db.Save(us2fs(browseInfo.FilePath), errorMessage))
  {
    MessageBoxW(*this, errorMessage, L"7-Zip", MB_OK | MB_ICONERROR);
    return;
  }

  UString value;
  value.Add_UInt32(_db.Size());
  UString message = MyFormatNew(IDS_PASSWORD_BOOK_EXPORTED, value);
  MessageBoxW(*this, message, L"7-Zip", MB_OK | MB_ICONINFORMATION);
}

void CPasswordBookPage::DeleteSelected()
{
  const int index = _listView.GetNextSelectedItem(-1);
  if (index < 0)
    return;

  LPARAM param = 0;
  if (!_listView.GetItemParam((unsigned)index, param))
    return;
  if ((unsigned)param >= _db.Size())
    return;

  const AString md5 = _db.Items()[(unsigned)param].Md5;
  if (_db.DeletePassword(md5))
  {
    _dataChanged = true;
    Changed();
    RefreshList();
  }
}

bool CPasswordBookPage::OnInit()
{
  _initMode = true;
  _settingsChanged = false;
  _dataChanged = false;
  _loadStats = NPasswordBook::CLoadStats();

#ifdef Z7_LANG
  LangSetDlgItems(*this, kLangIDs, Z7_ARRAY_SIZE(kLangIDs));
#endif

  _listView.Attach(GetItem(IDL_PASSWORD_BOOK_LIST));
  #ifndef UNDER_CE
  _listView.SetUnicodeFormat();
  #endif

  CheckButton(IDX_PASSWORD_BOOK_ENABLE, NPasswordBook::ReadEnabled());

  const DWORD exStyle = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES;
  _listView.SetExtendedListViewStyle(exStyle, exStyle);
  _listView.InsertColumn(0, L"MD5", 130);
  _listView.InsertColumn(1, LangString(IDG_PASSWORD), 150);

  UString errorMessage;
  if (!_db.Load(NPasswordBook::GetDatabasePath(), true, &_loadStats, errorMessage))
    MessageBoxW(*this, errorMessage, L"7-Zip", MB_OK | MB_ICONERROR);

  RefreshList();

  _initMode = false;
  return CPropertyPage::OnInit();
}

LONG CPasswordBookPage::OnApply()
{
  const bool enabled = IsButtonCheckedBool(IDX_PASSWORD_BOOK_ENABLE);
  const FString dbPath = NPasswordBook::GetDatabasePath();

  if (_dataChanged || (enabled && !DoesFileExist_Raw(dbPath)))
  {
    UString errorMessage;
    if (!_db.Save(dbPath, errorMessage))
    {
      MessageBoxW(*this, errorMessage, L"7-Zip", MB_OK | MB_ICONERROR);
      return PSNRET_INVALID;
    }
    _dataChanged = false;
  }

  if (_settingsChanged)
  {
    NPasswordBook::SaveEnabled(enabled);
    _settingsChanged = false;
  }

  return PSNRET_NOERROR;
}

void CPasswordBookPage::OnNotifyHelp()
{
  ShowHelpWindow(kPasswordBookTopic);
}

bool CPasswordBookPage::OnCommand(unsigned code, unsigned itemID, LPARAM param)
{
  if (!_initMode && code == EN_CHANGE && itemID == IDE_PASSWORD_BOOK_SEARCH)
  {
    RefreshList();
    return true;
  }
  return CPropertyPage::OnCommand(code, itemID, param);
}

bool CPasswordBookPage::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  switch (buttonID)
  {
    case IDX_PASSWORD_BOOK_ENABLE:
      _settingsChanged = true;
      Changed();
      return true;
    case IDB_PASSWORD_BOOK_DELETE:
      DeleteSelected();
      return true;
    case IDB_PASSWORD_BOOK_IMPORT:
      ImportData();
      return true;
    case IDB_PASSWORD_BOOK_EXPORT:
      ExportData();
      return true;
    default:
      return CPropertyPage::OnButtonClicked(buttonID, buttonHWND);
  }
}
