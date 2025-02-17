// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_

#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "ui/gfx/image/image.h"

using DownloadVector = std::vector<content::DownloadItem*>;

class DownloadCommands {
 public:
  enum Command {
    SHOW_IN_FOLDER = 1,   // Open a folder view window with the item selected.
    OPEN_WHEN_COMPLETE,   // Open the download when it's finished.
    ALWAYS_OPEN_TYPE,     // Default this file extension to always open.
    PLATFORM_OPEN,        // Open using platform handler.
    CANCEL,               // Cancel the download.
    PAUSE,                // Pause a download.
    RESUME,               // Resume a download.
    DISCARD,              // Discard the malicious download.
    KEEP,                 // Keep the malicious download.
    LEARN_MORE_SCANNING,  // Show information about download scanning.
    LEARN_MORE_INTERRUPTED,  // Show information about interrupted downloads.
    COPY_TO_CLIPBOARD,    // Copy the contents to the clipboard.
    ANNOTATE,             // Open an app to annotate the image.
    DELETE_TASK, //delete task iterm
    DELETE_TASK_DOWNLOAD, //delete task iterm and download file
    MOVE_FOLDER, //move download to other folder
    COPY_DOWNLOAD_LINK, //copy download link
    DOWNLOAD_AGAIN //download again
  };

  // |download_item| must outlive DownloadCommands.
  explicit DownloadCommands(content::DownloadItem* download_item, content::DownloadManager* download_manager);
  virtual ~DownloadCommands() {}

  gfx::Image GetCommandIcon(Command command);

  bool IsCommandEnabled(Command command) const;
  bool IsCommandChecked(Command command) const;
  bool IsCommandVisible(Command command) const;
  void ExecuteCommand(Command command);

#if defined(OS_WIN) || defined(OS_LINUX) || \
    (defined(OS_MACOSX) && !defined(OS_IOS))
  bool IsDownloadPdf() const;
  bool CanOpenPdfInSystemViewer() const;
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(
      DownloadCommandsTest,
      GetLearnMoreURLForInterruptedDownload_ContainsContext);

  Browser* GetBrowser() const;
  int GetCommandIconId(Command command) const;
  GURL GetLearnMoreURLForInterruptedDownload() const;
  void CopyFileAsImageToClipboard() const;

  content::DownloadItem* const download_item_;
  content::DownloadManager* const download_manager_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_COMMANDS_H_
