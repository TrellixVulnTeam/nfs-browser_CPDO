// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CDOS_WEB_PUSH_H_
#define CHROME_BROWSER_UI_WEBUI_CDOS_WEB_PUSH_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class RenderViewHost;
}

class CDOSWebPushUI : public content::WebUIController {
 public:
  explicit CDOSWebPushUI(content::WebUI* web_ui);

 private:
   
  DISALLOW_COPY_AND_ASSIGN(CDOSWebPushUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CDOS_WEB_PUSH_H_
