// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/system_menu_model_delegate.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/grit/generated_resources.h"
#include "components/sessions/core/tab_restore_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "components/bookmarks/common/bookmark_pref_names.h"

#if (defined(OS_LINUX) || defined(OS_WIN)) && !defined(OS_CHROMEOS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#endif

SystemMenuModelDelegate::SystemMenuModelDelegate(
    ui::AcceleratorProvider* provider,
    Browser* browser)
    : provider_(provider),
      browser_(browser) {
}

SystemMenuModelDelegate::~SystemMenuModelDelegate() {
}

bool SystemMenuModelDelegate::IsCommandIdChecked(int command_id) const {
  PrefService* prefs = browser_->profile()->GetPrefs();

  switch (command_id) {
    case IDC_USE_SYSTEM_TITLE_BAR: 
      return !prefs->GetBoolean(prefs::kUseCustomChromeFrame);
    case IDC_SHOW_BOOKMARKBAR: 
      return prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
    case IDC_SHOW_TOOLBAR: 
      return prefs->GetBoolean(prefs::kShowToolbar);
    case IDC_SHOW_SEARCHBAR: 
      return prefs->GetBoolean(prefs::kShowSearchBar);

    default:
      return false;
  }
}

bool SystemMenuModelDelegate::IsCommandIdEnabled(int command_id) const {
  if (!chrome::IsCommandEnabled(browser_, command_id))
    return false;

  if (command_id != IDC_RESTORE_TAB)
    return true;

  // chrome::IsCommandEnabled(IDC_RESTORE_TAB) returns true if TabRestoreService
  // hasn't been loaded yet. Return false if this is the case as we don't have
  // a good way to dynamically update the menu when TabRestoreService finishes
  // loading.
  // TODO(sky): add a way to update menu.
  sessions::TabRestoreService* trs =
      TabRestoreServiceFactory::GetForProfile(browser_->profile());
  if (!trs->IsLoaded()) {
    trs->LoadTabsFromLastSession();
    return false;
  }
  return true;
}

bool SystemMenuModelDelegate::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  return provider_->GetAcceleratorForCommandId(command_id, accelerator);
}

bool SystemMenuModelDelegate::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDC_RESTORE_TAB;
}

base::string16 SystemMenuModelDelegate::GetLabelForCommandId(
    int command_id) const {
  DCHECK_EQ(command_id, IDC_RESTORE_TAB);

  int string_id = IDS_RESTORE_TAB;
  if (IsCommandIdEnabled(command_id)) {
    sessions::TabRestoreService* trs =
        TabRestoreServiceFactory::GetForProfile(browser_->profile());
    trs->LoadTabsFromLastSession();
    if (trs && !trs->entries().empty() &&
        trs->entries().front()->type == sessions::TabRestoreService::WINDOW)
      string_id = IDS_RESTORE_WINDOW;
  }
  return l10n_util::GetStringUTF16(string_id);
}

void SystemMenuModelDelegate::ExecuteCommand(int command_id, int event_flags) {
  chrome::ExecuteCommand(browser_, command_id);
}
