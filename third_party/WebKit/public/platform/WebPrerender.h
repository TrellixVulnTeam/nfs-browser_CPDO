/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebPrerender_h
#define WebPrerender_h

#include "WebCommon.h"
#include "WebPrivatePtr.h"
#include "WebReferrerPolicy.h"
#include "WebString.h"
#include "WebURL.h"

namespace blink {

class Prerender;

// WebPrerenderRelType is a bitfield since multiple rel attributes can be set on
// the same prerender.
enum WebPrerenderRelType {
  PrerenderRelTypePrerender = 0x1,
  PrerenderRelTypeNext = 0x2,
};

class WebPrerender {
 public:
  class ExtraData {
   public:
    virtual ~ExtraData() {}
  };

  ~WebPrerender() { reset(); }
  WebPrerender() {}
  WebPrerender(const WebPrerender& other) { assign(other); }
  WebPrerender& operator=(const WebPrerender& other) {
    assign(other);
    return *this;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT explicit WebPrerender(Prerender*);

  BLINK_PLATFORM_EXPORT const Prerender* toPrerender() const;
#endif

  BLINK_PLATFORM_EXPORT void reset();
  BLINK_PLATFORM_EXPORT void assign(const WebPrerender&);
  BLINK_PLATFORM_EXPORT bool isNull() const;

  BLINK_PLATFORM_EXPORT WebURL url() const;
  BLINK_PLATFORM_EXPORT WebString referrer() const;
  BLINK_PLATFORM_EXPORT unsigned relTypes() const;
  BLINK_PLATFORM_EXPORT WebReferrerPolicy referrerPolicy() const;

  BLINK_PLATFORM_EXPORT void setExtraData(ExtraData*);
  BLINK_PLATFORM_EXPORT const ExtraData* getExtraData() const;

  BLINK_PLATFORM_EXPORT void didStartPrerender();
  BLINK_PLATFORM_EXPORT void didStopPrerender();
  BLINK_PLATFORM_EXPORT void didSendLoadForPrerender();
  BLINK_PLATFORM_EXPORT void didSendDOMContentLoadedForPrerender();
  BLINK_PLATFORM_EXPORT void didCancelPrerender();

 private:
  WebPrivatePtr<Prerender> m_private;
};

}  // namespace blink

#endif  // WebPrerender_h
