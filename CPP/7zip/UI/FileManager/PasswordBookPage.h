#ifndef ZIP7_INC_PASSWORD_BOOK_PAGE_H
#define ZIP7_INC_PASSWORD_BOOK_PAGE_H

#include "../../../Windows/Control/ListView.h"
#include "../../../Windows/Control/PropertyPage.h"

#include "PasswordBook.h"

class CPasswordBookPage: public NWindows::NControl::CPropertyPage
{
  bool _initMode;
  bool _settingsChanged;
  bool _dataChanged;

  NWindows::NControl::CListView _listView;
  NPasswordBook::CDatabase _db;
  NPasswordBook::CLoadStats _loadStats;

  bool MatchesFilter(const NPasswordBook::CEntry &entry, const UString &filter) const;
  void RefreshList();
  void RefreshStatus();
  void ImportData();
  void ExportData();
  void DeleteSelected();

  virtual bool OnInit() Z7_override;
  virtual LONG OnApply() Z7_override;
  virtual void OnNotifyHelp() Z7_override;
  virtual bool OnCommand(unsigned code, unsigned itemID, LPARAM param) Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;
};

#endif
