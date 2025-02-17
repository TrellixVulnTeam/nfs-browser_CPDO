// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}

// ImportLockDialogView asks the user to shut down Firefox before starting the
// profile import.
class ImportLockDialogView : public views::DialogDelegateView {
 public:
  static void Show(gfx::NativeWindow parent,
                   const base::Callback<void(bool)>& callback,
                   base::string16 importer_locktext);

 private:
  explicit ImportLockDialogView(const base::Callback<void(bool)>& callback,
                                base::string16 importer_locktext);
  ~ImportLockDialogView() override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;

  // views::DialogDelegate:
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  base::string16 GetWindowTitle() const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  views::Label* description_label_;

  // Called with the result of the dialog.
  base::Callback<void(bool)> callback_;

  DISALLOW_COPY_AND_ASSIGN(ImportLockDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_LOCK_DIALOG_VIEW_H_
