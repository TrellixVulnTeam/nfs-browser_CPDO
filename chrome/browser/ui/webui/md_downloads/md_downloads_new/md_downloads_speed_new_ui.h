// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_SPEED_NEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_SPEED_NEW_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class RenderViewHost;
}

class MdDownloadsDOMNewHandler;

class MdDownloadsSpeedNewUI : public content::WebUIController {
 public:
  explicit MdDownloadsSpeedNewUI(content::WebUI* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

  // content::WebUIController:
  void RenderViewReused(content::RenderViewHost* render_view_host);

 private:
  MdDownloadsDOMNewHandler* handler_;  // Weak.

  DISALLOW_COPY_AND_ASSIGN(MdDownloadsSpeedNewUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_MD_DOWNLOADS_SPEED_NEW_UI_H_
