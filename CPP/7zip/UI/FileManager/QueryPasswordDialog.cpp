#include "StdAfx.h"

#include "../../../Windows/Clipboard.h"

#include "../GUI/ExtractDialogRes.h"

#include "LangUtils.h"
#include "QueryPasswordDialog.h"
#include "resource.h"

using namespace NWindows;

bool CQueryPasswordDialog::OnInit()
{
#ifdef Z7_LANG
  LangSetWindowText(*this, IDM_QUERY_PASSWORD);
  SetItemText(IDT_QUERY_PASSWORD_VALUE, LangString(IDG_PASSWORD) + L":");
  SetItemText(IDB_QUERY_PASSWORD_COPY, LangString(IDS_BUTTON_COPY));
#endif
  _passwordEdit.Attach(GetItem(IDE_QUERY_PASSWORD_VALUE));
  _passwordEdit.SetText(Password);
  return CModalDialog::OnInit();
}

bool CQueryPasswordDialog::OnButtonClicked(unsigned buttonID, HWND buttonHWND)
{
  if (buttonID == IDB_QUERY_PASSWORD_COPY)
  {
    ClipboardSetText(*this, Password);
    return true;
  }
  return CModalDialog::OnButtonClicked(buttonID, buttonHWND);
}
