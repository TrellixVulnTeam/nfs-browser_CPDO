// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/web_resource_service.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/google/core/browser/google_util.h"
#include "components/prefs/pref_service.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

// No anonymous namespace, because const variables automatically get internal
// linkage.
const char kInvalidDataTypeError[] =
    "Data from web resource server is missing or not valid JSON.";

const char kUnexpectedJSONFormatError[] =
    "Data from web resource server does not have expected format.";

namespace web_resource {

WebResourceService::WebResourceService(
    PrefService* prefs,
    const GURL& web_resource_server,
    const std::string& application_locale,
    const char* last_update_time_pref_name,
    int start_fetch_delay_ms,
    int cache_update_delay_ms,
    net::URLRequestContextGetter* request_context,
    const char* disable_network_switch,
    const ParseJSONCallback& parse_json_callback)
    : prefs_(prefs),
      resource_request_allowed_notifier_(prefs, disable_network_switch),
      in_fetch_(false),
      web_resource_server_(web_resource_server),
      application_locale_(application_locale),
      last_update_time_pref_name_(last_update_time_pref_name),
      start_fetch_delay_ms_(start_fetch_delay_ms),
      cache_update_delay_ms_(cache_update_delay_ms),
      request_context_(request_context),
      parse_json_callback_(parse_json_callback),
      weak_ptr_factory_(this) {
  resource_request_allowed_notifier_.Init(this);
  DCHECK(prefs);
}

void WebResourceService::StartAfterDelay() {
  // If resource requests are not allowed, we'll get a callback when they are.
  if (resource_request_allowed_notifier_.ResourceRequestsAllowed())
    OnResourceRequestsAllowed();
}

WebResourceService::~WebResourceService() {
}

void WebResourceService::OnURLFetchComplete(const net::URLFetcher* source) {
  // Delete the URLFetcher when this function exits.
  std::unique_ptr<net::URLFetcher> clean_up_fetcher(url_fetcher_.release());

  if (source->GetStatus().is_success() && source->GetResponseCode() == 200) {
    std::string data;
    source->GetResponseAsString(&data);
    // Calls EndFetch() on completion.
    // Full JSON parsing might spawn a utility process (for security).
    // To limit the the number of simultaneously active processes
    // (on Android in particular) we short-cut the full parsing in the case of
    // trivially "empty" JSONs.
    if (data.empty() || data == "{}") {
      OnUnpackFinished(base::MakeUnique<base::DictionaryValue>());
    } else {
      parse_json_callback_.Run(data,
                               base::Bind(&WebResourceService::OnUnpackFinished,
                                          weak_ptr_factory_.GetWeakPtr()),
                               base::Bind(&WebResourceService::OnUnpackError,
                                          weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    // Don't parse data if attempt to download was unsuccessful.
    // Stop loading new web resource data, and silently exit.
    // We do not call parse_json_callback_, so we need to call EndFetch()
    // ourselves.
    EndFetch();
  }
}

// Delay initial load of resource data into cache so as not to interfere
// with startup time.
void WebResourceService::ScheduleFetch(int64_t delay_ms) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&WebResourceService::StartFetch,
                            weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay_ms));
}

// Initializes the fetching of data from the resource server.  Data
// load calls OnURLFetchComplete.
void WebResourceService::StartFetch() {
  // First, put our next cache load on the MessageLoop.
  ScheduleFetch(cache_update_delay_ms_);

  // Set cache update time in preferences.
  prefs_->SetString(last_update_time_pref_name_,
                    base::DoubleToString(base::Time::Now().ToDoubleT()));

  in_fetch_ = false;
  return;
}

void WebResourceService::EndFetch() {
  in_fetch_ = false;
}

void WebResourceService::OnUnpackFinished(std::unique_ptr<base::Value> value) {
  if (!value) {
    // Page information not properly read, or corrupted.
    OnUnpackError(kInvalidDataTypeError);
    return;
  }
  const base::DictionaryValue* dict = nullptr;
  if (!value->GetAsDictionary(&dict)) {
    OnUnpackError(kUnexpectedJSONFormatError);
    return;
  }
  Unpack(*dict);

  EndFetch();
}

void WebResourceService::OnUnpackError(const std::string& error_message) {
  LOG(ERROR) << error_message;
  EndFetch();
}

void WebResourceService::OnResourceRequestsAllowed() {
  int64_t delay = start_fetch_delay_ms_;
  // Check whether we have ever put a value in the web resource cache;
  // if so, pull it out and see if it's time to update again.
  if (prefs_->HasPrefPath(last_update_time_pref_name_)) {
    std::string last_update_pref =
        prefs_->GetString(last_update_time_pref_name_);
    if (!last_update_pref.empty()) {
      double last_update_value;
      base::StringToDouble(last_update_pref, &last_update_value);
      int64_t ms_until_update =
          cache_update_delay_ms_ -
          static_cast<int64_t>(
              (base::Time::Now() - base::Time::FromDoubleT(last_update_value))
                  .InMilliseconds());
      // Wait at least |start_fetch_delay_ms_|.
      if (ms_until_update > start_fetch_delay_ms_)
        delay = ms_until_update;
    }
  }
  // Start fetch and wait for UpdateResourceCache.
  ScheduleFetch(delay);
}

}  // namespace web_resource
