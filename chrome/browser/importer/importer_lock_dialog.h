// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace importer {

// This function is called by an ImporterHost, and presents the Firefox profile
// warning dialog. After closing the dialog, the ImportHost receives a callback
// with the message either to skip the import, or to continue the process.
void ShowImportLockDialog(gfx::NativeWindow parent,
                          const base::Callback<void(bool)>& callback,
                          base::string16 importer_locktext);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_LOCK_DIALOG_H_
