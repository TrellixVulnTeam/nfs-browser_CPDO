// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_helpers.h"

#include "build/build_config.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/command_line.h"
#include "chromeos/chromeos_switches.h"
#endif

#if !defined(OS_CHROMEOS)
#include "google_apis/gaia/gaia_urls.h"
#endif

namespace policy {

bool OverrideBlacklistForURL(const GURL& url, bool* block, int* reason) {
#if defined(OS_CHROMEOS)
  // Don't block if OOBE has completed.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kOobeGuestSession)) {
    return false;
  }

  // Don't block internal pages and extensions.
  if (url.SchemeIs("nfsbrowser") || url.SchemeIs("chrome-extension")) {
    return false;
  }

  // Don't block Google's support web site.
  if (url.SchemeIs(url::kHttpsScheme) && url.DomainIs("support.google.com")) {
    return false;
  }

  // Block the rest.
  *reason = net::ERR_BLOCKED_ENROLLMENT_CHECK_PENDING;
  *block = true;
  return true;
#else
  static const char kServiceLoginAuth[] = "/ServiceLoginAuth";

  *block = false;
  // Additionally whitelist /ServiceLoginAuth.
  if (url.GetOrigin() != GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  return url.path() == kServiceLoginAuth;
#endif
}

}  // namespace policy
