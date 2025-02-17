/*
 * Copyright (C) 2004, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.
 * Copyright (C) 2011 Peter Varga (pvarga@webkit.org), University of Szeged
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bindings/core/v8/ScriptRegexp.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8PerIsolateData.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "platform/ScriptForbiddenScope.h"

namespace blink {

ScriptRegexp::ScriptRegexp(const String& pattern,
                           TextCaseSensitivity caseSensitivity,
                           MultilineMode multilineMode,
                           CharacterMode charMode,
                           GlobalMode globalMode) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::from(isolate)->ensureScriptRegexpContext();
  v8::Context::Scope contextScope(context);
  v8::TryCatch tryCatch(isolate);

  unsigned flags = v8::RegExp::kNone;
  if (caseSensitivity == TextCaseInsensitive)
    flags |= v8::RegExp::kIgnoreCase;
  if (multilineMode == MultilineEnabled)
    flags |= v8::RegExp::kMultiline;
  if (charMode == UTF16)
    flags |= v8::RegExp::kUnicode;
  if (globalMode == GlobalEnabled) {
    flags |= v8::RegExp::kGlobal;
  }

  v8::Local<v8::RegExp> regex;
  if (v8::RegExp::New(context, v8String(isolate, pattern),
                      static_cast<v8::RegExp::Flags>(flags))
          .ToLocal(&regex))
    m_regex.set(isolate, regex);
  if (tryCatch.HasCaught() && !tryCatch.Message().IsEmpty())
    m_exceptionMessage =
        toCoreStringWithUndefinedOrNullCheck(tryCatch.Message()->Get());
}

int ScriptRegexp::match(const String& string,
                        int startFrom,
                        int* matchLength) const {
  if (matchLength)
    *matchLength = 0;

  if (m_regex.isEmpty() || string.isNull())
    return -1;

  // v8 strings are limited to int.
  if (string.length() > INT_MAX)
    return -1;

  ScriptForbiddenScope::AllowUserAgentScript allowScript;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::from(isolate)->ensureScriptRegexpContext();
  v8::Context::Scope contextScope(context);
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::RegExp> regex = m_regex.newLocal(isolate);
  v8::Local<v8::Value> exec;
  if (!regex->Get(context, v8AtomicString(isolate, "exec")).ToLocal(&exec))
    return -1;
  v8::Local<v8::Value> argv[] = {
      v8String(isolate, string.substring(startFrom))};
  v8::Local<v8::Value> returnValue;
  if (!V8ScriptRunner::callInternalFunction(exec.As<v8::Function>(), regex,
                                            WTF_ARRAY_LENGTH(argv), argv,
                                            isolate)
           .ToLocal(&returnValue))
    return -1;

  // RegExp#exec returns null if there's no match, otherwise it returns an
  // Array of strings with the first being the whole match string and others
  // being subgroups. The Array also has some random properties tacked on like
  // "index" which is the offset of the match.
  //
  // https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/exec

  ASSERT(!returnValue.IsEmpty());
  if (!returnValue->IsArray())
    return -1;

  v8::Local<v8::Array> result = returnValue.As<v8::Array>();
  v8::Local<v8::Value> matchOffset;
  if (!result->Get(context, v8AtomicString(isolate, "index"))
           .ToLocal(&matchOffset))
    return -1;
  if (matchLength) {
    v8::Local<v8::Value> match;
    if (!result->Get(context, 0).ToLocal(&match))
      return -1;
    *matchLength = match.As<v8::String>()->Length();
  }

  return matchOffset.As<v8::Int32>()->Value() + startFrom;
}

void ScriptRegexp::replace(String& string, const String& repstr) {
  if (m_regex.isEmpty() || string.isNull())
    return;

  // v8 strings are limited to int.
  if (string.length() > INT_MAX)
    return ;

  ScriptForbiddenScope::AllowUserAgentScript allowScript;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context =
      V8PerIsolateData::from(isolate)->ensureScriptRegexpContext();
  v8::Context::Scope contextScope(context);
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::RegExp> regex = m_regex.newLocal(isolate);
  v8::Local<v8::StringObject> strObj =
      v8::StringObject::New(v8String(isolate, string)).As<v8::StringObject>();
  v8::Local<v8::Value> replace;
  if(!strObj->Get(context, v8AtomicString(isolate, "replace"))
           .ToLocal(&replace)) {
    return;
  }

  v8::Local<v8::Value> argv[] = {
      regex, v8String(isolate, repstr)};
  v8::Local<v8::Value> returnValue;
  if (!V8ScriptRunner::callInternalFunction(replace.As<v8::Function>(), strObj,
                                            WTF_ARRAY_LENGTH(argv), argv,
                                            isolate)
           .ToLocal(&returnValue))
    return;

  DCHECK(!returnValue.IsEmpty());

  v8::Local<v8::String> result = returnValue.As<v8::String>();
  string = toCoreString(result);
}

}  // namespace blink
