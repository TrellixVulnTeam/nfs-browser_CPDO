// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/singleton_tabs.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_url_handler.h"
#include "content/public/browser/web_contents.h"

namespace chrome {
namespace {

// Returns true if two URLs are equal after taking |replacements| into account.
bool CompareURLsWithReplacements(const GURL& url,
                                 const GURL& other,
                                 const url::Replacements<char>& replacements) {
  if (url == other)
    return true;

  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL other_replaced = other.ReplaceComponents(replacements);
  return url_replaced == other_replaced;
}

}  // namespace

void ShowSingletonTab(Browser* browser, const GURL& url) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  Navigate(&params);
}

// Returns true if two URLs are equal.
bool CompareURLs(const GURL& url, const GURL& other) {
  if (url == other)
    return true;
  return false;
}

bool IsSettingUrl(const GURL& url, const url::Replacements<char>& replacements) {
  GURL setting_url = GURL(chrome::kChromeUIDownloadSettingsURL);
  if(url == setting_url)
    return true;

  GURL url_replaced = url.ReplaceComponents(replacements);
  GURL setting_url_replaced = setting_url.ReplaceComponents(replacements);
  return url_replaced == setting_url_replaced;
}

void ShowSingletonTabRespectRef(Browser* browser, const GURL& url) {
  NavigateParams params(GetSingletonTabNavigateParams(browser, url));
  params.ref_behavior = NavigateParams::RESPECT_REF;
  Navigate(&params);
}

void ShowSingletonTabOverwritingNTP(Browser* browser,
                                    const NavigateParams& params) {
  DCHECK(browser);
  NavigateParams local_params(params);
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (contents) {
    const GURL& contents_url = contents->GetURL();
    if ((contents_url == GURL(kChromeUINewTabURL) ||
         search::IsInstantNTP(contents) ||
         contents_url == GURL(url::kAboutBlankURL)) &&
        GetIndexOfSingletonTab(&local_params) < 0) {
      local_params.disposition = WindowOpenDisposition::CURRENT_TAB;
    }
  }

  Navigate(&local_params);
}

NavigateParams GetSingletonTabNavigateParams(Browser* browser,
                                             const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  params.window_action = NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;
  params.tabstrip_add_types |= TabStripModel::ADD_INHERIT_OPENER;
  return params;
}

// Returns the index of an existing singleton tab in |params->browser| matching
// the URL specified in |params|.
int GetIndexOfSingletonTab(NavigateParams* params) {
  if (params->disposition != WindowOpenDisposition::SINGLETON_TAB)
    return -1;

  // In case the URL was rewritten by the BrowserURLHandler we need to ensure
  // that we do not open another URL that will get redirected to the rewritten
  // URL.
  GURL rewritten_url(params->url);
  bool reverse_on_redirect = false;
  content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_url,
      params->browser->profile(),
      &reverse_on_redirect);

  // If there are several matches: prefer the active tab by starting there.
  int start_index =
      std::max(0, params->browser->tab_strip_model()->active_index());
  int tab_count = params->browser->tab_strip_model()->count();
  for (int i = 0; i < tab_count; ++i) {
    int tab_index = (start_index + i) % tab_count;
    content::WebContents* tab =
        params->browser->tab_strip_model()->GetWebContentsAt(tab_index);

    GURL tab_url = tab->GetURL();

    // Skip view-source tabs. This is needed because RewriteURLIfNecessary
    // removes the "view-source:" scheme which leads to incorrect matching.
    if (tab_url.SchemeIs(content::kViewSourceScheme))
      continue;

    GURL rewritten_tab_url = tab_url;
    content::BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &rewritten_tab_url,
      params->browser->profile(),
      &reverse_on_redirect);

    url::Replacements<char> replacements;
    if (params->ref_behavior == NavigateParams::IGNORE_REF)
      replacements.ClearRef();
    if (params->path_behavior == NavigateParams::IGNORE_AND_NAVIGATE ||
        params->path_behavior == NavigateParams::IGNORE_AND_STAY_PUT) {
      replacements.ClearPath();
      replacements.ClearQuery();
    }

    if (CompareURLsWithReplacements(tab_url, params->url, replacements) ||
        CompareURLsWithReplacements(rewritten_tab_url,
                                    rewritten_url,
                                    replacements)) {
      params->target_contents = tab;
      return tab_index;
    }
  }

  return -1;
}

}  // namespace chrome
