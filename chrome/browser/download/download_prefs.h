// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_

#include <set>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/prefs/pref_member.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
class DownloadManager;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Stores all download-related preferences.
class DownloadPrefs {
 public:
  explicit DownloadPrefs(Profile* profile);
  ~DownloadPrefs();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the default download directory.
  static const base::FilePath& GetDefaultDownloadDirectory();

  // Returns the default download directory for the current profile.
  base::FilePath GetDefaultDownloadDirectoryForProfile() const;

  // Returns the DownloadPrefs corresponding to the given DownloadManager
  // or BrowserContext.
  static DownloadPrefs* FromDownloadManager(
      content::DownloadManager* download_manager);
  static DownloadPrefs* FromBrowserContext(
      content::BrowserContext* browser_context);

  base::FilePath DownloadPath() const;
  void SetDownloadPath(const base::FilePath& path);
  base::FilePath SaveFilePath() const;
  void SetSaveFilePath(const base::FilePath& path);
  int save_file_type() const { return *save_file_type_; }
  void SetSaveFileType(int type);

  // Returns true if the prompt_for_download preference has been set and the
  // download location is not managed (which means the user shouldn't be able
  // to choose another download location).
  bool PromptForDownload() const;

  // Returns true if the download path preference is managed.
  bool IsDownloadPathManaged() const;

  // Returns true if there is at least one file extension registered
  // for auto-open.
  bool IsAutoOpenUsed() const;

  // Returns true if |path| should be opened automatically based on
  // |path.Extension()|.
  bool IsAutoOpenEnabledBasedOnExtension(const base::FilePath& path) const;

  // Enables automatically opening all downloads with the same file type as
  // |file_name|. Returns true on success. The call may fail if |file_name|
  // either doesn't have an extension (hence the file type cannot be
  // determined), or if the file type is one that is disallowed from being
  // opened automatically. See IsAllowedToOpenAutomatically() for details on the
  // latter.
  bool EnableAutoOpenBasedOnExtension(const base::FilePath& file_name);

  // Disables auto-open based on file extension.
  void DisableAutoOpenBasedOnExtension(const base::FilePath& file_name);

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  // Store the user preference to disk. If |should_open| is true, also disable
  // the built-in PDF plugin. If |should_open| is false, enable the PDF plugin.
  void SetShouldOpenPdfInSystemReader(bool should_open);

  // Return whether the user prefers to open PDF downloads in the platform's
  // default reader.
  bool ShouldOpenPdfInSystemReader() const;

  // Used by tests to disable version checks for Adobe.
  void DisableAdobeVersionCheckForTests();
#endif


  void ResetAutoOpen();

  bool PromptDownload() const;
  void SetPromptDownload(bool prompt);

 private:
  void SaveAutoOpenState();

  Profile* profile_;

  BooleanPrefMember prompt_for_download_;
  FilePathPrefMember download_path_;
  FilePathPrefMember  original_download_path_;
  FilePathPrefMember save_file_path_;
  IntegerPrefMember save_file_type_;

  BooleanPrefMember prompt_download_;

  // Set of file extensions to open at download completion.
  struct AutoOpenCompareFunctor {
    bool operator()(const base::FilePath::StringType& a,
                    const base::FilePath::StringType& b) const;
  };
  typedef std::set<base::FilePath::StringType,
                   AutoOpenCompareFunctor> AutoOpenSet;
  AutoOpenSet auto_open_;

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  bool should_open_pdf_in_system_reader_;
  bool disable_adobe_version_check_for_tests_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DownloadPrefs);
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_PREFS_H_
