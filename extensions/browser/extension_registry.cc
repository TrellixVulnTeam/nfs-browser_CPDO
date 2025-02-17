// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_registry.h"

#include "base/strings/string_util.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {

ExtensionRegistry::ExtensionRegistry(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}
ExtensionRegistry::~ExtensionRegistry() {}

// static
ExtensionRegistry* ExtensionRegistry::Get(content::BrowserContext* context) {
  return ExtensionRegistryFactory::GetForBrowserContext(context);
}

std::unique_ptr<ExtensionSet>
ExtensionRegistry::GenerateInstalledExtensionsSet() const {
  return GenerateInstalledExtensionsSet(EVERYTHING);
}

std::unique_ptr<ExtensionSet> ExtensionRegistry::GenerateInstalledExtensionsSet(
    int include_mask) const {
  std::unique_ptr<ExtensionSet> installed_extensions(new ExtensionSet);
  if (include_mask & IncludeFlag::ENABLED)
    installed_extensions->InsertAll(enabled_extensions_);
  if (include_mask & IncludeFlag::DISABLED)
    installed_extensions->InsertAll(disabled_extensions_);
  if (include_mask & IncludeFlag::TERMINATED)
    installed_extensions->InsertAll(terminated_extensions_);
  if (include_mask & IncludeFlag::BLACKLISTED)
    installed_extensions->InsertAll(blacklisted_extensions_);
  if (include_mask & IncludeFlag::BLOCKED)
    installed_extensions->InsertAll(blocked_extensions_);
  return installed_extensions;
}

void ExtensionRegistry::AddObserver(ExtensionRegistryObserver* observer) {
  observers_.AddObserver(observer);
}

void ExtensionRegistry::RemoveObserver(ExtensionRegistryObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionRegistry::TriggerOnLoaded(const Extension* extension) {
  CHECK(extension);
  DCHECK(enabled_extensions_.Contains(extension->id()));
  FOR_EACH_OBSERVER(ExtensionRegistryObserver,
                    observers_,
                    OnExtensionLoaded(browser_context_, extension));
}

void ExtensionRegistry::TriggerOnReady(const Extension* extension) {
  CHECK(extension);
  DCHECK(enabled_extensions_.Contains(extension->id()));
  FOR_EACH_OBSERVER(ExtensionRegistryObserver, observers_,
                    OnExtensionReady(browser_context_, extension));
}

void ExtensionRegistry::TriggerOnUnloaded(
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  CHECK(extension);
  DCHECK(!enabled_extensions_.Contains(extension->id()));
  FOR_EACH_OBSERVER(ExtensionRegistryObserver,
                    observers_,
                    OnExtensionUnloaded(browser_context_, extension, reason));
}

void ExtensionRegistry::TriggerOnWillBeInstalled(const Extension* extension,
                                                 bool is_update,
                                                 const std::string& old_name) {
  CHECK(extension);
  DCHECK_EQ(is_update,
            GenerateInstalledExtensionsSet()->Contains(extension->id()));
  DCHECK_EQ(is_update, !old_name.empty());
  FOR_EACH_OBSERVER(ExtensionRegistryObserver, observers_,
                    OnExtensionWillBeInstalled(browser_context_, extension,
                                               is_update, old_name));
}

void ExtensionRegistry::TriggerOnInstalled(const Extension* extension,
                                           bool is_update) {
  CHECK(extension);
  DCHECK(GenerateInstalledExtensionsSet()->Contains(extension->id()));
  FOR_EACH_OBSERVER(ExtensionRegistryObserver,
                    observers_,
                    OnExtensionInstalled(
                        browser_context_, extension, is_update));
}

void ExtensionRegistry::TriggerOnUninstalled(const Extension* extension,
                                             UninstallReason reason) {
  CHECK(extension);
  DCHECK(!GenerateInstalledExtensionsSet()->Contains(extension->id()));
  FOR_EACH_OBSERVER(
      ExtensionRegistryObserver,
      observers_,
      OnExtensionUninstalled(browser_context_, extension, reason));
}

void ExtensionRegistry::TriggerOnCancelled(const Extension* extension) {
  CHECK(extension);
  DCHECK(!GenerateInstalledExtensionsSet()->Contains(extension->id()));
  FOR_EACH_OBSERVER(
      ExtensionRegistryObserver,
      observers_,
      OnExtensionCancelled(browser_context_, extension));
}

const Extension* ExtensionRegistry::GetExtensionById(const std::string& id,
                                                     int include_mask) const {
  std::string lowercase_id = base::ToLowerASCII(id);
  if (include_mask & ENABLED) {
    const Extension* extension = enabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & DISABLED) {
    const Extension* extension = disabled_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & TERMINATED) {
    const Extension* extension = terminated_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & BLACKLISTED) {
    const Extension* extension = blacklisted_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  if (include_mask & BLOCKED) {
    const Extension* extension = blocked_extensions_.GetByID(lowercase_id);
    if (extension)
      return extension;
  }
  return NULL;
}

const Extension* ExtensionRegistry::GetInstalledExtension(
    const std::string& id) const {
  return GetExtensionById(id, ExtensionRegistry::EVERYTHING);
}

bool ExtensionRegistry::AddEnabled(
    const scoped_refptr<const Extension>& extension) {
  return enabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveEnabled(const std::string& id) {
  // Only enabled extensions can be ready, so removing an enabled extension
  // should also remove from the ready set if possible.
  if (ready_extensions_.Contains(id))
    RemoveReady(id);
  return enabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddDisabled(
    const scoped_refptr<const Extension>& extension) {
  return disabled_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveDisabled(const std::string& id) {
  return disabled_extensions_.Remove(id);
}

bool ExtensionRegistry::AddTerminated(
    const scoped_refptr<const Extension>& extension) {
  return terminated_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveTerminated(const std::string& id) {
  return terminated_extensions_.Remove(id);
}

bool ExtensionRegistry::AddBlacklisted(
    const scoped_refptr<const Extension>& extension) {
  return blacklisted_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveBlacklisted(const std::string& id) {
  return blacklisted_extensions_.Remove(id);
}

bool ExtensionRegistry::AddBlocked(
    const scoped_refptr<const Extension>& extension) {
  return blocked_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveBlocked(const std::string& id) {
  return blocked_extensions_.Remove(id);
}

bool ExtensionRegistry::AddReady(
    const scoped_refptr<const Extension>& extension) {
  return ready_extensions_.Insert(extension);
}

bool ExtensionRegistry::RemoveReady(const std::string& id) {
  return ready_extensions_.Remove(id);
}

void ExtensionRegistry::ClearAll() {
  enabled_extensions_.Clear();
  disabled_extensions_.Clear();
  terminated_extensions_.Clear();
  blacklisted_extensions_.Clear();
  blocked_extensions_.Clear();
  ready_extensions_.Clear();
}

void ExtensionRegistry::Shutdown() {
  // Release references to all Extension objects in the sets.
  ClearAll();
  FOR_EACH_OBSERVER(ExtensionRegistryObserver, observers_, OnShutdown(this));
}

}  // namespace extensions
