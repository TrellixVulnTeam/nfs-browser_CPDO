// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_create_info.h"

#include <string>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"

namespace content {

DownloadCreateInfo::DownloadCreateInfo(
    const base::Time& start_time,
    const net::NetLogWithSource& net_log,
    std::unique_ptr<DownloadSaveInfo> save_info)
    : download_id(DownloadItem::kInvalidId),
      start_time(start_time),
      total_bytes(0),
      has_user_gesture(false),
      transition_type(ui::PAGE_TRANSITION_LINK),
      result(DOWNLOAD_INTERRUPT_REASON_NONE),
      save_info(std::move(save_info)),
      request_net_log(net_log),
      force_speedy(false) {}

DownloadCreateInfo::DownloadCreateInfo()
    : DownloadCreateInfo(base::Time(),
                         net::NetLogWithSource(),
                         base::WrapUnique(new DownloadSaveInfo)) {}

DownloadCreateInfo::~DownloadCreateInfo() {}

const GURL& DownloadCreateInfo::url() const {
  return url_chain.empty() ? GURL::EmptyGURL() : url_chain.back();
}

}  // namespace content
