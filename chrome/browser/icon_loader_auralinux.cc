// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/icon_loader.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/nix/mime_util_xdg.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

// static
IconGroupID IconLoader::ReadGroupIDFromFilepath(
    const base::FilePath& filepath) {
  return base::nix::GetFileMimeType(filepath);
}

// static
bool IconLoader::IsIconMutableFromFilepath(const base::FilePath&) {
  return false;
}

// static
content::BrowserThread::ID IconLoader::ReadIconThreadID() {
  // ReadIcon() calls into views::LinuxUI and GTK2 code, so it must be on the UI
  // thread.
  return content::BrowserThread::UI;
}

void IconLoader::ReadIcon() {
  int size_pixels = 0;
  switch (icon_size_) {
    case IconLoader::SMALL:
      size_pixels = 16;
      break;
    case IconLoader::NORMAL:
      size_pixels = 32;
      break;
    case IconLoader::LARGE:
      size_pixels = 48;
      break;
    default:
      NOTREACHED();
  }

  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui) {
    gfx::Image image = ui->GetIconForContentType(group_, size_pixels);
    if (!image.IsEmpty())
      image_.reset(new gfx::Image(image));
    else {
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
       gfx::ImageSkia* picture = NULL;
      if(size_pixels == 16) {
        picture = rb.GetImageSkiaNamed(IDR_OCTET_STREAM_LITTLE);
      } else if (size_pixels == 32) {
        picture = rb.GetImageSkiaNamed(IDR_OCTET_STREAM_MIDDLE);
      } else if  (size_pixels == 48) {
        picture = rb.GetImageSkiaNamed(IDR_OCTET_STREAM_LARGE);
      }
      if (picture && !(gfx::Image(*picture)).IsEmpty())
        image_.reset(new gfx::Image(*picture));
    }
  }
  target_task_runner_->PostTask(
      FROM_HERE, base::Bind(&IconLoader::NotifyDelegate, this));
}
