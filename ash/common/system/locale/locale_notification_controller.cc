// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/locale/locale_notification_controller.h"

#include <utility>

#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/system_tray_notifier.h"
#include "ash/common/wm_shell.h"
#include "base/strings/string16.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"

using message_center::Notification;

namespace ash {
namespace {

const char kLocaleChangeNotificationId[] = "nfsbrowser://settings/locale";

class LocaleNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit LocaleNotificationDelegate(LocaleObserver::Delegate* delegate);

 protected:
  ~LocaleNotificationDelegate() override;

  // message_center::NotificationDelegate overrides:
  void Close(bool by_user) override;
  bool HasClickedListener() override;
  void Click() override;
  void ButtonClick(int button_index) override;

 private:
  LocaleObserver::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LocaleNotificationDelegate);
};

LocaleNotificationDelegate::LocaleNotificationDelegate(
    LocaleObserver::Delegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
}

LocaleNotificationDelegate::~LocaleNotificationDelegate() {}

void LocaleNotificationDelegate::Close(bool by_user) {
  delegate_->AcceptLocaleChange();
}

bool LocaleNotificationDelegate::HasClickedListener() {
  return true;
}

void LocaleNotificationDelegate::Click() {
  delegate_->AcceptLocaleChange();
}

void LocaleNotificationDelegate::ButtonClick(int button_index) {
  DCHECK_EQ(0, button_index);
  delegate_->RevertLocaleChange();
}

}  // namespace

LocaleNotificationController::LocaleNotificationController() {
  WmShell::Get()->system_tray_notifier()->AddLocaleObserver(this);
}

LocaleNotificationController::~LocaleNotificationController() {
  WmShell::Get()->system_tray_notifier()->RemoveLocaleObserver(this);
}

void LocaleNotificationController::OnLocaleChanged(
    LocaleObserver::Delegate* delegate,
    const std::string& cur_locale,
    const std::string& from_locale,
    const std::string& to_locale) {
  if (!delegate)
    return;

  base::string16 from =
      l10n_util::GetDisplayNameForLocale(from_locale, cur_locale, true);
  base::string16 to =
      l10n_util::GetDisplayNameForLocale(to_locale, cur_locale, true);

  message_center::RichNotificationData optional;
  optional.buttons.push_back(
      message_center::ButtonInfo(l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_LOCALE_REVERT_MESSAGE, from)));
  optional.never_timeout = true;

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, kLocaleChangeNotificationId,
      base::string16() /* title */,
      l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_LOCALE_CHANGE_MESSAGE,
                                 from, to),
      bundle.GetImageNamed(IDR_AURA_UBER_TRAY_LOCALE),
      base::string16() /* display_source */, GURL(),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 system_notifier::kNotifierLocale),
      optional, new LocaleNotificationDelegate(delegate)));
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

}  // namespace ash
