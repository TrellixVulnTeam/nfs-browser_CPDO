// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_source.h"

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resources_util.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache.h"
#include "chrome/browser/ui/webui/ntp/ntp_resource_cache_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "url/gurl.h"

namespace {

GURL GetThemeUrl(const std::string& path) {
  return GURL(std::string(content::kChromeUIScheme) + "://" +
              std::string(chrome::kChromeUIThemeHost) + "/" + path);
}

bool IsNewTabCssPath(const std::string& path) {
  static const char kNewTabCSSPath[] = "css/new_tab.css";
  static const char kIncognitoNewTabCSSPath[] =
      "css/incognito_new_tab_theme.css";
  return (path == kNewTabCSSPath) || (path == kIncognitoNewTabCSSPath);
}

void ProcessImageOnUiThread(const gfx::ImageSkia& image,
                            float scale,
                            scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const gfx::ImageSkiaRep& rep = image.GetRepresentation(scale);
  gfx::PNGCodec::EncodeBGRASkBitmap(
      rep.sk_bitmap(), false /* discard transparency */, &data->data());
}

void ProcessResourceOnUiThread(int resource_id,
                               float scale,
                               scoped_refptr<base::RefCountedBytes> data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ProcessImageOnUiThread(
      *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id),
      scale, data);
}

base::RefCountedMemory* GetNewTabCSSOnUiThread(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  NTPResourceCache::WindowType type =
      NTPResourceCache::GetWindowType(profile, nullptr);
  return NTPResourceCacheFactory::GetForProfile(profile)->GetNewTabCSS(type);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, public:

ThemeSource::ThemeSource(Profile* profile) : profile_(profile) {}

ThemeSource::~ThemeSource() {
}

std::string ThemeSource::GetSource() const {
  return chrome::kChromeUIThemeHost;
}

void ThemeSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  // Default scale factor if not specified.
  float scale = 1.0f;
  std::string parsed_path;
  webui::ParsePathAndScale(GetThemeUrl(path), &parsed_path, &scale);

  if (IsNewTabCssPath(parsed_path)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // NB: it's important that this is |profile_| and not |original_profile_|.
    content::BrowserThread::PostTaskAndReplyWithResult(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&GetNewTabCSSOnUiThread, profile_), callback);
    return;
  }

  int resource_id = -1;
  if (parsed_path == "current-channel-logo") {
    switch (chrome::GetChannel()) {
#if defined(GOOGLE_CHROME_BUILD)
      case version_info::Channel::CANARY:
        resource_id = IDR_PRODUCT_LOGO_32_CANARY;
        break;
      case version_info::Channel::DEV:
        resource_id = IDR_PRODUCT_LOGO_32_DEV;
        break;
      case version_info::Channel::BETA:
        resource_id = IDR_PRODUCT_LOGO_32_BETA;
        break;
      case version_info::Channel::STABLE:
        resource_id = IDR_PRODUCT_LOGO_32;
        break;
#else
      case version_info::Channel::CANARY:
      case version_info::Channel::DEV:
      case version_info::Channel::BETA:
      case version_info::Channel::STABLE:
        NOTREACHED();
#endif
      case version_info::Channel::UNKNOWN:
        resource_id = IDR_PRODUCT_LOGO_32;
        break;
    }
  } else {
    resource_id = ResourcesUtil::GetThemeResourceId(parsed_path);
  }

  // Limit the maximum scale we'll respond to.  Very large scale factors can
  // take significant time to serve or, at worst, crash the browser due to OOM.
  // We don't want to clamp to the max scale factor, though, for devices that
  // use 2x scale without 2x data packs, as well as omnibox requests for larger
  // (but still reasonable) scales (see below).
  const float max_scale = ui::GetScaleForScaleFactor(
      ResourceBundle::GetSharedInstance().GetMaxScaleFactor());
  const float unreasonable_scale = max_scale * 32;
  if ((resource_id == -1) || (scale >= unreasonable_scale)) {
    // Either we have no data to send back, or the requested scale is
    // unreasonably large.  This shouldn't happen normally, as chrome://theme/
    // URLs are only used by WebUI pages and component extensions.  However, the
    // user can also enter these into the omnibox, so we need to fail
    // gracefully.
    callback.Run(nullptr);
  } else if ((GetMimeType(path) == "image/png") && (scale > max_scale)) {
    SendThemeImage(callback, resource_id, scale);
  } else {
    SendThemeBitmap(callback, resource_id, scale);
  }
}

std::string ThemeSource::GetMimeType(const std::string& path) const {
  std::string parsed_path;
  webui::ParsePathAndScale(GetThemeUrl(path), &parsed_path, nullptr);
  return IsNewTabCssPath(parsed_path) ? "text/css" : "image/png";
}

base::MessageLoop* ThemeSource::MessageLoopForRequestPath(
    const std::string& path) const {
  std::string parsed_path;
  webui::ParsePathAndScale(GetThemeUrl(path), &parsed_path, nullptr);

  if (IsNewTabCssPath(parsed_path)) {
    // We generated and cached this when we initialized the object.  We don't
    // have to go back to the UI thread to send the data.
    return nullptr;
  }

  // If it's not a themeable image, we don't need to go to the UI thread.
  int resource_id = ResourcesUtil::GetThemeResourceId(parsed_path);
  return BrowserThemePack::IsPersistentImageID(resource_id) ?
      content::URLDataSource::MessageLoopForRequestPath(path) : nullptr;
}

bool ThemeSource::ShouldServiceRequest(const net::URLRequest* request) const {
#if 0
  return request->url().SchemeIs(chrome::kChromeSearchScheme) ?
      InstantIOContext::ShouldServiceRequest(request) :
      URLDataSource::ShouldServiceRequest(request);
#else
  return URLDataSource::ShouldServiceRequest(request);
#endif
}

////////////////////////////////////////////////////////////////////////////////
// ThemeSource, private:

void ThemeSource::SendThemeBitmap(
    const content::URLDataSource::GotDataCallback& callback,
    int resource_id,
    float scale) {
  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactor(scale);
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    scoped_refptr<base::RefCountedMemory> image_data(
        ThemeService::GetThemeProviderForProfile(profile_->GetOriginalProfile())
            .GetRawData(resource_id, scale_factor));
    callback.Run(image_data.get());
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    callback.Run(rb.LoadDataResourceBytesForScale(resource_id, scale_factor));
  }
}

void ThemeSource::SendThemeImage(
    const content::URLDataSource::GotDataCallback& callback,
    int resource_id,
    float scale) {
  scoped_refptr<base::RefCountedBytes> data(new base::RefCountedBytes());
  if (BrowserThemePack::IsPersistentImageID(resource_id)) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    const ui::ThemeProvider& tp = ThemeService::GetThemeProviderForProfile(
        profile_->GetOriginalProfile());
    ProcessImageOnUiThread(*tp.GetImageSkiaNamed(resource_id), scale, data);
    callback.Run(data.get());
  } else {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Fetching image data in ResourceBundle should happen on the UI thread. See
    // crbug.com/449277
    content::BrowserThread::PostTaskAndReply(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ProcessResourceOnUiThread, resource_id, scale, data),
        base::Bind(callback, data));
  }
}
