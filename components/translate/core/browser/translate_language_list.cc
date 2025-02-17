// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/core/browser/translate_language_list.h"

#include <stddef.h>

#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/translate/core/browser/translate_browser_metrics.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_event_details.h"
#include "components/translate/core/browser/translate_url_fetcher.h"
#include "components/translate/core/browser/translate_url_util.h"
#include "components/translate/core/common/translate_util.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace translate {

namespace {

// The default list of languages the Google translation server supports.
// We use this list until we receive the list that the server exposes.
// Server also supports "hmm" (Hmong) and "jw" (Javanese), but these are
// excluded because Chrome l10n library does not support it.
const char* const kDefaultSupportedLanguages[] = {
  "af",     // Afrikaans
  "am",     // Amharic
  "ar",     // Arabic
  "az",     // Azerbaijani
  "be",     // Belarusian
  "bg",     // Bulgarian
  "bn",     // Bengali
  "bs",     // Bosnian
  "ca",     // Catalan
  "ceb",    // Cebuano
  "co",     // Corsican
  "cs",     // Czech
  "cy",     // Welsh
  "da",     // Danish
  "de",     // German
  "el",     // Greek
  "en",     // English
  "eo",     // Esperanto
  "es",     // Spanish
  "et",     // Estonian
  "eu",     // Basque
  "fa",     // Persian
  "fi",     // Finnish
  "fy",     // Frisian
  "fr",     // French
  "ga",     // Irish
  "gd",     // Scots Gaelic
  "gl",     // Galician
  "gu",     // Gujarati
  "ha",     // Hausa
  "haw",    // Hawaiian
  "hi",     // Hindi
  "hr",     // Croatian
  "ht",     // Haitian Creole
  "hu",     // Hungarian
  "hy",     // Armenian
  "id",     // Indonesian
  "ig",     // Igbo
  "is",     // Icelandic
  "it",     // Italian
  "iw",     // Hebrew
  "ja",     // Japanese
  "ka",     // Georgian
  "kk",     // Kazakh
  "km",     // Khmer
  "kn",     // Kannada
  "ko",     // Korean
  "ku",     // Kurdish
  "ky",     // Kyrgyz
  "la",     // Latin
  "lb",     // Luxembourgish
  "lo",     // Lao
  "lt",     // Lithuanian
  "lv",     // Latvian
  "mg",     // Malagasy
  "mi",     // Maori
  "mk",     // Macedonian
  "ml",     // Malayalam
  "mn",     // Mongolian
  "mr",     // Marathi
  "ms",     // Malay
  "mt",     // Maltese
  "my",     // Burmese
  "ne",     // Nepali
  "nl",     // Dutch
  "no",     // Norwegian
  "ny",     // Nyanja
  "pa",     // Punjabi
  "pl",     // Polish
  "ps",     // Pashto
  "pt",     // Portuguese
  "ro",     // Romanian
  "ru",     // Russian
  "sd",     // Sindhi
  "si",     // Sinhala
  "sk",     // Slovak
  "sl",     // Slovenian
  "sm",     // Samoan
  "sn",     // Shona
  "so",     // Somali
  "sq",     // Albanian
  "sr",     // Serbian
  "st",     // Southern Sotho
  "su",     // Sundanese
  "sv",     // Swedish
  "sw",     // Swahili
  "ta",     // Tamil
  "te",     // Telugu
  "tg",     // Tajik
  "th",     // Thai
  "tl",     // Tagalog
  "tr",     // Turkish
  "uk",     // Ukrainian
  "ur",     // Urdu
  "uz",     // Uzbek
  "vi",     // Vietnamese
  "yi",     // Yiddish
  "xh",     // Xhosa
  "yo",     // Yoruba
  "zh-CN",  // Chinese (Simplified)
  "zh-TW",  // Chinese (Traditional)
  "zu",     // Zulu
};

// Constant URL string to fetch server supporting language list.
const char kLanguageListFetchPath[] = "translate_a/l?client=chrome";

// Used in kTranslateScriptURL to request supporting languages list including
// "alpha languages".
// const char kAlphaLanguageQueryName[] = "alpha";
// const char kAlphaLanguageQueryValue[] = "1";

// Represent if the language list updater is disabled.
bool update_is_disabled = false;

// Retry parameter for fetching.
const int kMaxRetryOn5xx = 5;

}  // namespace

const char TranslateLanguageList::kTargetLanguagesKey[] = "tl";
const char TranslateLanguageList::kAlphaLanguagesKey[] = "al";

TranslateLanguageList::TranslateLanguageList()
    : resource_requests_allowed_(false), request_pending_(false) {
  // We default to our hard coded list of languages in
  // |kDefaultSupportedLanguages|. This list will be overriden by a server
  // providing supported langauges list.
  for (size_t i = 0; i < arraysize(kDefaultSupportedLanguages); ++i)
    all_supported_languages_.insert(kDefaultSupportedLanguages[i]);

  if (update_is_disabled)
    return;

  language_list_fetcher_.reset(new TranslateURLFetcher(kFetcherId));
  language_list_fetcher_->set_max_retry_on_5xx(kMaxRetryOn5xx);
}

TranslateLanguageList::~TranslateLanguageList() {}

void TranslateLanguageList::GetSupportedLanguages(
    std::vector<std::string>* languages) {
  DCHECK(languages && languages->empty());
  std::set<std::string>::const_iterator iter = all_supported_languages_.begin();
  for (; iter != all_supported_languages_.end(); ++iter)
    languages->push_back(*iter);

  // Update language lists if they are not updated after Chrome was launched
  // for later requests.
  if (!update_is_disabled && language_list_fetcher_.get())
    RequestLanguageList();
}

std::string TranslateLanguageList::GetLanguageCode(
    const std::string& language) {
  // Only remove the country code for country specific languages we don't
  // support specifically yet.
  if (IsSupportedLanguage(language))
    return language;

  size_t hypen_index = language.find('-');
  if (hypen_index == std::string::npos)
    return language;
  return language.substr(0, hypen_index);
}

bool TranslateLanguageList::IsSupportedLanguage(const std::string& language) {
  return all_supported_languages_.count(language) != 0;
}

bool TranslateLanguageList::IsAlphaLanguage(const std::string& language) {
  return alpha_languages_.count(language) != 0;
}

GURL TranslateLanguageList::TranslateLanguageUrl() {
  std::string url = translate::GetTranslateSecurityOrigin().spec() +
      kLanguageListFetchPath;
  return GURL(url);
}

void TranslateLanguageList::RequestLanguageList() {
}

void TranslateLanguageList::SetResourceRequestsAllowed(bool allowed) {
  resource_requests_allowed_ = allowed;
  if (resource_requests_allowed_ && request_pending_) {
    RequestLanguageList();
    DCHECK(!request_pending_);
  }
}

std::unique_ptr<TranslateLanguageList::EventCallbackList::Subscription>
TranslateLanguageList::RegisterEventCallback(const EventCallback& callback) {
  return callback_list_.Add(callback);
}

// static
void TranslateLanguageList::DisableUpdate() {
  update_is_disabled = true;
}

void TranslateLanguageList::OnLanguageListFetchComplete(
    int id,
    bool success,
    const std::string& data) {
  if (!success) {
    // Since it fails just now, omit to schedule resource requests if
    // ResourceRequestAllowedNotifier think it's ready. Otherwise, a callback
    // will be invoked later to request resources again.
    // The TranslateURLFetcher has a limit for retried requests and aborts
    // re-try not to invoke OnLanguageListFetchComplete anymore if it's asked to
    // re-try too many times.
    NotifyEvent(__LINE__, "Failed to fetch languages");
    return;
  }

  NotifyEvent(__LINE__, "Language list is updated");

  DCHECK_EQ(kFetcherId, id);

  bool parsed_correctly = SetSupportedLanguages(data);
  language_list_fetcher_.reset();

  if (parsed_correctly)
    last_updated_ = base::Time::Now();
}

void TranslateLanguageList::NotifyEvent(int line, const std::string& message) {
  TranslateEventDetails details(__FILE__, line, message);
  callback_list_.Notify(details);
}

bool TranslateLanguageList::SetSupportedLanguages(
    const std::string& language_list) {
  // The format is in JSON as:
  // {
  //   "sl": {"XX": "LanguageName", ...},
  //   "tl": {"XX": "LanguageName", ...},
  //   "al": {"XX": 1, ...}
  // }
  // Where "tl" and "al" are set in kTargetLanguagesKey and kAlphaLanguagesKey.
  std::unique_ptr<base::Value> json_value =
      base::JSONReader::Read(language_list, base::JSON_ALLOW_TRAILING_COMMAS);

  if (json_value == NULL || !json_value->IsType(base::Value::TYPE_DICTIONARY)) {
    NotifyEvent(__LINE__, "Language list is invalid");
    NOTREACHED();
    return false;
  }
  // The first level dictionary contains three sub-dict, first for source
  // languages and second for target languages, we want to use the target
  // languages. The last is for alpha languages.
  base::DictionaryValue* language_dict =
      static_cast<base::DictionaryValue*>(json_value.get());
  base::DictionaryValue* target_languages = NULL;
  if (!language_dict->GetDictionary(TranslateLanguageList::kTargetLanguagesKey,
                                    &target_languages) ||
      target_languages == NULL) {
    NotifyEvent(__LINE__, "Target languages are not found in the response");
    NOTREACHED();
    return false;
  }

  const std::string& locale =
      TranslateDownloadManager::GetInstance()->application_locale();

  // Now we can clear language list.
  all_supported_languages_.clear();
  std::string message;
  // ... and replace it with the values we just fetched from the server.
  for (base::DictionaryValue::Iterator iter(*target_languages);
       !iter.IsAtEnd();
       iter.Advance()) {
    const std::string& lang = iter.key();
    if (!l10n_util::IsLocaleNameTranslated(lang.c_str(), locale)) {
      TranslateBrowserMetrics::ReportUndisplayableLanguage(lang);
      continue;
    }
    all_supported_languages_.insert(lang);
    if (message.empty())
      message += lang;
    else
      message += ", " + lang;
  }
  NotifyEvent(__LINE__, message);

  // Get the alpha languages. The "al" parameter could be abandoned.
  base::DictionaryValue* alpha_languages = NULL;
  if (!language_dict->GetDictionary(TranslateLanguageList::kAlphaLanguagesKey,
                                    &alpha_languages) ||
      alpha_languages == NULL) {
    // Return true since alpha language part is optional.
    return true;
  }

  // We assume that the alpha languages are included in the above target
  // languages, and don't use UMA or NotifyEvent.
  alpha_languages_.clear();
  for (base::DictionaryValue::Iterator iter(*alpha_languages);
       !iter.IsAtEnd(); iter.Advance()) {
    const std::string& lang = iter.key();
    if (!l10n_util::IsLocaleNameTranslated(lang.c_str(), locale))
      continue;
    alpha_languages_.insert(lang);
  }
  return true;
}

}  // namespace translate
