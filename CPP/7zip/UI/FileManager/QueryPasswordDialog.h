#ifndef ZIP7_INC_QUERY_PASSWORD_DIALOG_H
#define ZIP7_INC_QUERY_PASSWORD_DIALOG_H

#include "../../../Windows/Control/Dialog.h"
#include "../../../Windows/Control/Edit.h"

#include "QueryPasswordDialogRes.h"

class CQueryPasswordDialog: public NWindows::NControl::CModalDialog
{
  NWindows::NControl::CEdit _passwordEdit;

  virtual bool OnInit() Z7_override;
  virtual bool OnButtonClicked(unsigned buttonID, HWND buttonHWND) Z7_override;

public:
  UString Password;
  INT_PTR Create(HWND wndParent = NULL) { return CModalDialog::Create(IDD_QUERY_PASSWORD_DLG, wndParent); }
};

#endif
