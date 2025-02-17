// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCHBAR_SEARCHBOX_EDIT_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEARCHBAR_SEARCHBOX_EDIT_CONTROLLER_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class ToolbarModel;

namespace gfx {
class Image;
}

class SearchboxEditController {
 public:
  virtual void OnAutocompleteAccept(const GURL& destination_url,
                                    WindowOpenDisposition disposition,
                                    ui::PageTransition transition);

  virtual void OnInputInProgress(bool in_progress) = 0;

  // Called when anything has changed that might affect the layout or contents
  // of the views around the edit, including the text of the edit and the
  // status of any keyword- or hint-related state.
  virtual void OnChanged() = 0;

  // Called whenever the autocomplete edit gets focused.
  virtual void OnSetFocus() = 0;

  virtual void OnKillFocus() = 0;

  // Shows the URL.
  virtual void ShowURL()  {};

  virtual ToolbarModel* GetToolbarModel() = 0;
  virtual const ToolbarModel* GetToolbarModel() const = 0;

 protected:
  SearchboxEditController();
  virtual ~SearchboxEditController();

  GURL destination_url() const { return destination_url_; }
  WindowOpenDisposition disposition() const { return disposition_; }
  ui::PageTransition transition() const { return transition_; }

 private:
  // The details necessary to open the user's desired omnibox match.
  GURL destination_url_;
  WindowOpenDisposition disposition_;
  ui::PageTransition transition_;

  DISALLOW_COPY_AND_ASSIGN(SearchboxEditController);
};

#endif  // CHROME_BROWSER_UI_SEARCHBAR_SEARCHBOX_EDIT_CONTROLLER_H_
