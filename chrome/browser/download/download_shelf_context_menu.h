// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/download/download_commands.h"
#include "content/public/browser/download_item.h"
#include "ui/base/models/simple_menu_model.h"

namespace content {
class PageNavigator;
}

// This class is responsible for the download shelf context menu. Platform
// specific subclasses are responsible for creating and running the menu.
//
// The DownloadItem corresponding to the context menu is observed for removal or
// destruction.
class DownloadShelfContextMenu : public ui::SimpleMenuModel::Delegate,
                                 public content::DownloadItem::Observer {
 public:
  ~DownloadShelfContextMenu() override;

  content::DownloadItem* download_item() const { return download_item_; }

 protected:
  explicit DownloadShelfContextMenu(content::DownloadItem* download_item, content::DownloadManager* download_manager);

  // Returns the correct menu model depending on the state of the download item.
  // Returns NULL if the download was destroyed.
  ui::SimpleMenuModel* GetMenuModel();

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;
  bool IsItemForCommandIdDynamic(int command_id) const override;
  base::string16 GetLabelForCommandId(int command_id) const override;

 private:
  // Detaches self from |download_item_|. Called when the DownloadItem is
  // destroyed or when this object is being destroyed.
  void DetachFromDownloadItem();

  // content::DownloadItem::Observer
  void OnDownloadDestroyed(content::DownloadItem* download) override;

  ui::SimpleMenuModel* GetInProgressMenuModel();
  ui::SimpleMenuModel* GetInProgressPausedMenuModel();
  ui::SimpleMenuModel* GetFinishedMenuModel();
  ui::SimpleMenuModel* GetInterruptedMenuModel();
  ui::SimpleMenuModel* GetMaybeMaliciousMenuModel();
  ui::SimpleMenuModel* GetMaliciousMenuModel();

  // We show slightly different menus if the download is in progress vs. if the
  // download has finished.
  std::unique_ptr<ui::SimpleMenuModel> in_progress_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> in_progress_download_paused_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> finished_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> interrupted_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> maybe_malicious_download_menu_model_;
  std::unique_ptr<ui::SimpleMenuModel> malicious_download_menu_model_;

  // Information source.
  content::DownloadItem* download_item_;
  std::unique_ptr<DownloadCommands> download_commands_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenu);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_H_
