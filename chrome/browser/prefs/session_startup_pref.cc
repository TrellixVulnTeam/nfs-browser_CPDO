// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/session_startup_pref.h"

#include <stddef.h>

#include <string>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/url_formatter/url_fixer.h"

namespace {

// Converts a SessionStartupPref::Type to an integer written to prefs.
int TypeToPrefValue(SessionStartupPref::Type type) {
  switch (type) {
    case SessionStartupPref::LAST: return SessionStartupPref::kPrefValueLast;
    case SessionStartupPref::URLS: return SessionStartupPref::kPrefValueURLs;
    case SessionStartupPref::LAST_URLS_LIST:     return SessionStartupPref::kPrefValueLastURLsList;
    default:                       return SessionStartupPref::kPrefValueNewTab;
  }
}

void URLListToPref(const base::ListValue* url_list, SessionStartupPref* pref) {
  pref->urls.clear();
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    std::string url_text;
    if (url_list->GetString(i, &url_text)) {
      GURL fixed_url = url_formatter::FixupURL(url_text, std::string());
      pref->urls.push_back(fixed_url);
    }
  }
}

}  // namespace

// static
void SessionStartupPref::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if defined(OS_IOS) || defined(OS_ANDROID)
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;
#else
  uint32_t flags = user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;
#endif
  registry->RegisterIntegerPref(prefs::kRestoreOnStartup,
                                TypeToPrefValue(GetDefaultStartupType()),
                                flags);
  registry->RegisterListPref(prefs::kURLsToRestoreOnStartup, flags);
}

// static
SessionStartupPref::Type SessionStartupPref::GetDefaultStartupType() {
#if defined(OS_CHROMEOS)
  return SessionStartupPref::LAST;
#else
  return SessionStartupPref::DEFAULT;
#endif
}

// static
void SessionStartupPref::SetStartupPref(
    Profile* profile,
    const SessionStartupPref& pref) {
  DCHECK(profile);
  SetStartupPref(profile->GetPrefs(), pref);
}

// static
void SessionStartupPref::SetStartupPref(PrefService* prefs,
                                        const SessionStartupPref& pref) {
  DCHECK(prefs);

  if (!SessionStartupPref::TypeIsManaged(prefs))
    prefs->SetInteger(prefs::kRestoreOnStartup, TypeToPrefValue(pref.type));

  if (!SessionStartupPref::URLsAreManaged(prefs)) {
    // Always save the URLs, that way the UI can remain consistent even if the
    // user changes the startup type pref.
    // Ownership of the ListValue retains with the pref service.
    ListPrefUpdate update(prefs, prefs::kURLsToRestoreOnStartup);
    base::ListValue* url_pref_list = update.Get();
    DCHECK(url_pref_list);
    url_pref_list->Clear();
    for (size_t i = 0; i < pref.urls.size(); ++i) {
      url_pref_list->Set(static_cast<int>(i),
                         new base::StringValue(pref.urls[i].spec()));
    }
  }
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(Profile* profile) {
  DCHECK(profile);
  return GetStartupPref(profile->GetPrefs());
}

// static
SessionStartupPref SessionStartupPref::GetStartupPref(PrefService* prefs) {
  DCHECK(prefs);

  SessionStartupPref pref(
      PrefValueToType(prefs->GetInteger(prefs::kRestoreOnStartup)));

  // Always load the urls, even if the pref type isn't URLS. This way the
  // preferences panels can show the user their last choice.
  const base::ListValue* url_list =
      prefs->GetList(prefs::kURLsToRestoreOnStartup);
  URLListToPref(url_list, &pref);

  return pref;
}

// static
bool SessionStartupPref::TypeIsManaged(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsManaged();
}

// static
bool SessionStartupPref::URLsAreManaged(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_urls =
      prefs->FindPreference(prefs::kURLsToRestoreOnStartup);
  DCHECK(pref_urls);
  return pref_urls->IsManaged();
}

// static
bool SessionStartupPref::TypeIsDefault(PrefService* prefs) {
  DCHECK(prefs);
  const PrefService::Preference* pref_restore =
      prefs->FindPreference(prefs::kRestoreOnStartup);
  DCHECK(pref_restore);
  return pref_restore->IsDefaultValue();
}

// static
SessionStartupPref::Type SessionStartupPref::PrefValueToType(int pref_value) {
  switch (pref_value) {
    case kPrefValueLast:     return SessionStartupPref::LAST;
    case kPrefValueURLs:     return SessionStartupPref::URLS;
    case kPrefValueLastURLsList:     return SessionStartupPref::LAST_URLS_LIST;
    default:                 return SessionStartupPref::DEFAULT;
  }
}

SessionStartupPref::SessionStartupPref(Type type) : type(type) {}

SessionStartupPref::SessionStartupPref(const SessionStartupPref& other) =
    default;

SessionStartupPref::~SessionStartupPref() {}
