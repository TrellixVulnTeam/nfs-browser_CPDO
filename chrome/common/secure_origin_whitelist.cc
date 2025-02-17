// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/secure_origin_whitelist.h"

#include <vector>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/constants.h"

void GetSecureOriginWhitelist(std::set<GURL>* origins) {
  // If kUnsafelyTreatInsecureOriginAsSecure option is given and
  // kUserDataDir is present, add the given origins as trustworthy
  // for whitelisting.
   base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
        //   add by wdg for the whitelist
    command_line.AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      " http://fanyi.youdao.com/web2/styles/all-packed.css?572877"); //有道翻译
        command_line.AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      " http://fanyi.youdao.com/web2/scripts/all-packed-utf-8.js?242748M&1470360520000"); //有道翻译
  command_line.AppendSwitch(switches::kUserDataDir);
 // end by wdg
  if (command_line.HasSwitch(switches::kUnsafelyTreatInsecureOriginAsSecure) &&
      command_line.HasSwitch(switches::kUserDataDir)) {
    std::string origins_str = command_line.GetSwitchValueASCII(
        switches::kUnsafelyTreatInsecureOriginAsSecure);
    for (const std::string& origin : base::SplitString(
             origins_str, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL))
      origins->insert(GURL(origin));
  }
}

void GetSchemesBypassingSecureContextCheckWhitelist(
    std::set<std::string>* schemes) {
  schemes->insert(extensions::kExtensionScheme);
}
