// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2016-2018 CPU and Fundamental Software Research Center, Chinese Academy of Sciences.

#ifndef CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
#define CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_

#include <stddef.h>

#include <memory>
#include <set>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "content/public/browser/download_item.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace content {
class DownloadManager;
class WebUI;
}

// A class that tracks all downloads activity and keeps a sorted representation
// of the downloads as chrome://downloads wants to display them.
class DownloadsListTracker : public AllDownloadItemNotifier::Observer {
 public:
  DownloadsListTracker(content::DownloadManager* download_manager,
                       content::WebUI* web_ui);
  ~DownloadsListTracker() override;

  // Clears all downloads on the page if currently sending updates and resets
  // chunk tracking data.
  void Reset();

  // This class only cares about downloads that match |search_terms|.
  // An empty list shows all downloads (the default). Returns true if
  // |search_terms| differ from the current ones.
  bool SetSearchTerms(const base::ListValue& search_terms);

  // Starts sending updates and sends a capped amount of downloads. Tracks which
  // downloads have been sent. Re-call this to send more.
  void StartAndSendChunk();

  // Stops sending updates to the page.
  void Stop();

  content::DownloadManager* GetMainNotifierManager() const;
  content::DownloadManager* GetOriginalNotifierManager() const;

  // AllDownloadItemNotifier::Observer:
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadUpdated(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;
  void OnDownloadRemoved(content::DownloadManager* manager,
                         content::DownloadItem* download_item) override;

 protected:
  // Testing constructor.
  DownloadsListTracker(content::DownloadManager* download_manager,
                       content::WebUI* web_ui,
                       base::Callback<bool(const content::DownloadItem&)>);

  // Creates a dictionary value that's sent to the page as JSON.
  virtual std::unique_ptr<base::DictionaryValue> CreateDownloadItemValue(
      content::DownloadItem* item) const;

  // Creates a normal dictionary value that's sent to the page as JSON.
  virtual std::unique_ptr<base::DictionaryValue> CreateDownloadItemValueNormal(
      content::DownloadItem* item) const;

  // Creates a speed dictionary value that's sent to the page as JSON.
  virtual std::unique_ptr<base::DictionaryValue> CreateDownloadItemValueSpeed(
      content::DownloadItem* item) const;

  // Exposed for testing.
  bool IsIncognito(const content::DownloadItem& item) const;

  const content::DownloadItem* GetItemForTesting(size_t index) const;

  void SetChunkSizeForTesting(size_t chunk_size);

 private:
  struct StartTimeComparator {
    bool operator() (const content::DownloadItem* a,
                     const content::DownloadItem* b) const;
  };
  using SortedSet = std::set<content::DownloadItem*, StartTimeComparator>;

  // Called by both constructors to initialize common state.
  void Init();

  // Clears and re-inserts all downloads items into |sorted_items_|.
  void RebuildSortedItems();

  // Whether |item| should show on the current page.
  bool ShouldShow(const content::DownloadItem& item) const;

  // Returns the index of |item| in |sorted_items_|.
  size_t GetIndex(const SortedSet::iterator& item) const;

  // Calls "insertItems" if sending updates and the page knows about |insert|.
  void InsertItem(const SortedSet::iterator& insert);

  // Calls "updateItem" if sending updates and the page knows about |update|.
  void UpdateItem(const SortedSet::iterator& update);

  // Removes the item that corresponds to |remove| and sends "removeItems"
  // if sending updates.
  void RemoveItem(const SortedSet::iterator& remove);

  AllDownloadItemNotifier main_notifier_;
  std::unique_ptr<AllDownloadItemNotifier> original_notifier_;

  // The WebUI object corresponding to the page we care about.
  content::WebUI* const web_ui_;

  // Callback used to determine if an item should show on the page. Set to
  // |ShouldShow()| in default constructor, passed in while testing.
  base::Callback<bool(const content::DownloadItem&)> should_show_;

  // When this is true, all changes to downloads that affect the page are sent
  // via JavaScript.
  bool sending_updates_ = false;

  SortedSet sorted_items_;

  // The number of items sent to the page so far.
  size_t sent_to_page_ = 0u;

  // The maximum number of items sent to the page at a time.
  size_t chunk_size_ = 20u;

  // Current search terms.
  std::vector<base::string16> search_terms_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsListTracker);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MD_DOWNLOADS_DOWNLOADS_LIST_TRACKER_H_
