// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLTimeElement_h
#define HTMLTimeElement_h

#include "core/html/HTMLElement.h"

namespace blink {

class HTMLTimeElement final : public HTMLElement {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_NODE_FACTORY(HTMLTimeElement);

 private:
  explicit HTMLTimeElement(Document&);
};

}  // namespace blink

#endif  // HTMLTimeElement_h
