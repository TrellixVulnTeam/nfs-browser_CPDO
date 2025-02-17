// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright (c) 2016-2018 CPU and Fundamental Software Research Center, Chinese Academy of Sciences.

#include "chrome/browser/plugins/plugin_finder.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/browser_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(ENABLE_PLUGIN_INSTALLATION)
#include "chrome/browser/plugins/plugin_installer.h"
#endif

using base::DictionaryValue;
using content::PluginService;

namespace {

typedef std::map<std::string, PluginMetadata*> PluginMap;

// Do not change these values, as they are used in UMA.
enum class PluginListError {
  // NO_ERROR is defined by Windows headers.
  PLUGIN_LIST_NO_ERROR = 0,
  JSON_INVALID_ESCAPE = 1,
  JSON_SYNTAX_ERROR = 2,
  JSON_UNEXPECTED_TOKEN = 3,
  JSON_TRAILING_COMMA = 4,
  JSON_TOO_MUCH_NESTING = 5,
  JSON_UNEXPECTED_DATA_AFTER_ROOT = 5,
  JSON_UNSUPPORTED_ENCODING = 6,
  JSON_UNQUOTED_DICTIONARY_KEY = 7,
  SCHEMA_ERROR = 8,
  NUM_VALUES
};

// Gets the full path of the plugin file as the identifier.
std::string GetLongIdentifier(const content::WebPluginInfo& plugin) {
  return plugin.path.AsUTF8Unsafe();
}

// Gets the base name of the file path as the identifier.
std::string GetIdentifier(const content::WebPluginInfo& plugin) {
  return plugin.path.BaseName().AsUTF8Unsafe();
}

// Gets the plugin group name as the plugin name if it is not empty or
// the filename without extension if the name is empty.
static base::string16 GetGroupName(const content::WebPluginInfo& plugin) {
  if (!plugin.name.empty())
    return plugin.name;

  return plugin.path.BaseName().RemoveExtension().AsUTF16Unsafe();
}

void LoadMimeTypes(bool matching_mime_types,
                   const base::DictionaryValue* plugin_dict,
                   PluginMetadata* plugin) {
  const base::ListValue* mime_types = NULL;
  std::string list_key =
      matching_mime_types ? "matching_mime_types" : "mime_types";
  if (!plugin_dict->GetList(list_key, &mime_types))
    return;

  bool success = false;
  for (base::ListValue::const_iterator mime_type_it = mime_types->begin();
       mime_type_it != mime_types->end(); ++mime_type_it) {
    std::string mime_type_str;
    success = (*mime_type_it)->GetAsString(&mime_type_str);
    DCHECK(success);
    if (matching_mime_types) {
      plugin->AddMatchingMimeType(mime_type_str);
    } else {
      plugin->AddMimeType(mime_type_str);
    }
  }
}

PluginMetadata* CreatePluginMetadata(
    const std::string& identifier,
    const base::DictionaryValue* plugin_dict) {
  std::string url;
  bool success = plugin_dict->GetString("url", &url);
  std::string help_url;
  plugin_dict->GetString("help_url", &help_url);
  base::string16 name;
  success = plugin_dict->GetString("name", &name);
  DCHECK(success);
  bool display_url = false;
  plugin_dict->GetBoolean("displayurl", &display_url);
  base::string16 group_name_matcher;
  success = plugin_dict->GetString("group_name_matcher", &group_name_matcher);
  DCHECK(success);
  std::string language_str;
  plugin_dict->GetString("lang", &language_str);

  PluginMetadata* plugin = new PluginMetadata(identifier,
                                              name,
                                              display_url,
                                              GURL(url),
                                              GURL(help_url),
                                              group_name_matcher,
                                              language_str);
  const base::ListValue* versions = NULL;
  if (plugin_dict->GetList("versions", &versions)) {
    for (base::ListValue::const_iterator it = versions->begin();
         it != versions->end(); ++it) {
      base::DictionaryValue* version_dict = NULL;
      if (!(*it)->GetAsDictionary(&version_dict)) {
        NOTREACHED();
        continue;
      }
      std::string version;
      success = version_dict->GetString("version", &version);
      DCHECK(success);
      std::string status_str;
      success = version_dict->GetString("status", &status_str);
      DCHECK(success);
      PluginMetadata::SecurityStatus status =
          PluginMetadata::SECURITY_STATUS_UP_TO_DATE;
      success = PluginMetadata::ParseSecurityStatus(status_str, &status);
      DCHECK(success);
      plugin->AddVersion(base::Version(version), status);
    }
  }

  LoadMimeTypes(false, plugin_dict, plugin);
  LoadMimeTypes(true, plugin_dict, plugin);
  return plugin;
}

void RecordBuiltInPluginListError(PluginListError error_code) {
  UMA_HISTOGRAM_ENUMERATION("PluginFinder.BuiltInPluginList.ErrorCode",
                            static_cast<int>(error_code),
                            static_cast<int>(PluginListError::NUM_VALUES));
}

}  // namespace

// static
void PluginFinder::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisablePluginFinder, false);
}

// static
PluginFinder* PluginFinder::GetInstance() {
  // PluginFinder::GetInstance() is the only method that's allowed to call
  // base::Singleton<PluginFinder>::get().
  return base::Singleton<PluginFinder>::get();
}

PluginFinder::PluginFinder() : version_(-1) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void PluginFinder::Init() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Load the built-in plugin list first. If we have a newer version stored
  // locally or download one, we will replace this one with it.
  std::unique_ptr<base::DictionaryValue> plugin_list(LoadBuiltInPluginList());

  // Gracefully handle the case where we couldn't parse the built-in plugin list
  // for some reason (https://crbug.com/388560). TODO(bauerb): Change back to a
  // DCHECK once we have gathered more data about the underlying problem.
  if (!plugin_list)
    return;

  ReinitializePlugins(plugin_list.get());
}

// static
base::DictionaryValue* PluginFinder::LoadBuiltInPluginList() {
  base::StringPiece json_resource(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PLUGIN_DB_JSON));
  std::string error_str;
  int error_code = base::JSONReader::JSON_NO_ERROR;
  std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
      json_resource, base::JSON_PARSE_RFC, &error_code, &error_str);
  if (!value) {
    DLOG(ERROR) << error_str;
    switch (error_code) {
      case base::JSONReader::JSON_INVALID_ESCAPE:
        RecordBuiltInPluginListError(PluginListError::JSON_INVALID_ESCAPE);
        break;
      case base::JSONReader::JSON_SYNTAX_ERROR:
        RecordBuiltInPluginListError(PluginListError::JSON_SYNTAX_ERROR);
        break;
      case base::JSONReader::JSON_UNEXPECTED_TOKEN:
        RecordBuiltInPluginListError(PluginListError::JSON_UNEXPECTED_TOKEN);
        break;
      case base::JSONReader::JSON_TRAILING_COMMA:
        RecordBuiltInPluginListError(PluginListError::JSON_TRAILING_COMMA);
        break;
      case base::JSONReader::JSON_TOO_MUCH_NESTING:
        RecordBuiltInPluginListError(PluginListError::JSON_TOO_MUCH_NESTING);
        break;
      case base::JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT:
        RecordBuiltInPluginListError(
            PluginListError::JSON_UNEXPECTED_DATA_AFTER_ROOT);
        break;
      case base::JSONReader::JSON_UNSUPPORTED_ENCODING:
        RecordBuiltInPluginListError(
            PluginListError::JSON_UNSUPPORTED_ENCODING);
        break;
      case base::JSONReader::JSON_UNQUOTED_DICTIONARY_KEY:
        RecordBuiltInPluginListError(
            PluginListError::JSON_UNQUOTED_DICTIONARY_KEY);
        break;
      case base::JSONReader::JSON_NO_ERROR:
      case base::JSONReader::JSON_PARSE_ERROR_COUNT:
        NOTREACHED();
        break;
    }
    return nullptr;
  }

  if (value->GetType() != base::Value::TYPE_DICTIONARY) {
    // JSONReader::JSON_PARSE_ERROR_COUNT is used for the case where the JSON
    // value has the wrong type.
    RecordBuiltInPluginListError(PluginListError::SCHEMA_ERROR);
    return nullptr;
  }

  DCHECK_EQ(base::JSONReader::JSON_NO_ERROR, error_code);
  RecordBuiltInPluginListError(PluginListError::PLUGIN_LIST_NO_ERROR);
  return static_cast<base::DictionaryValue*>(value.release());
}

PluginFinder::~PluginFinder() {
#if defined(ENABLE_PLUGIN_INSTALLATION)
  base::STLDeleteValues(&installers_);
#endif
  base::STLDeleteValues(&identifier_plugin_);
}

base::string16 PluginFinder::FindPluginName(const std::string& mime_type,
                                            const std::string& language) {
  base::AutoLock lock(mutex_);

  for (auto plugin : identifier_plugin_) {
    if (language == plugin.second->language() &&
        plugin.second->HasMimeType(mime_type)) {
      return plugin.second->name();
    }
  }

  return base::UTF8ToUTF16(mime_type);
}

#if defined(ENABLE_PLUGIN_INSTALLATION)
bool PluginFinder::FindPlugin(
    const std::string& mime_type,
    const std::string& language,
    PluginInstaller** installer,
    std::unique_ptr<PluginMetadata>* plugin_metadata) {
  if (g_browser_process->local_state()->GetBoolean(prefs::kDisablePluginFinder))
    return false;

  base::AutoLock lock(mutex_);
  PluginMap::const_iterator metadata_it = identifier_plugin_.begin();
  for (; metadata_it != identifier_plugin_.end(); ++metadata_it) {
    if (language == metadata_it->second->language() &&
        metadata_it->second->HasMimeType(mime_type)) {
      *plugin_metadata = metadata_it->second->Clone();

      std::map<std::string, PluginInstaller*>::const_iterator installer_it =
          installers_.find(metadata_it->second->identifier());
      DCHECK(installer_it != installers_.end());
      *installer = installer_it->second;
      return true;
    }
  }
  return false;
}

bool PluginFinder::FindPluginWithIdentifier(
    const std::string& identifier,
    PluginInstaller** installer,
    std::unique_ptr<PluginMetadata>* plugin_metadata) {
  base::AutoLock lock(mutex_);
  PluginMap::const_iterator metadata_it = identifier_plugin_.find(identifier);
  if (metadata_it == identifier_plugin_.end())
    return false;
  *plugin_metadata = metadata_it->second->Clone();

  if (installer) {
    std::map<std::string, PluginInstaller*>::const_iterator installer_it =
        installers_.find(identifier);
    if (installer_it == installers_.end())
      return false;
    *installer = installer_it->second;
  }
  return true;
}
#endif

void PluginFinder::ReinitializePlugins(
    const base::DictionaryValue* plugin_list) {
  base::AutoLock lock(mutex_);
  int version = 0;  // If no version is defined, we default to 0.
  const char kVersionKey[] = "x-version";
  plugin_list->GetInteger(kVersionKey, &version);
  if (version <= version_)
    return;

  version_ = version;

  base::STLDeleteValues(&identifier_plugin_);

  for (base::DictionaryValue::Iterator plugin_it(*plugin_list);
      !plugin_it.IsAtEnd(); plugin_it.Advance()) {
    const base::DictionaryValue* plugin = NULL;
    const std::string& identifier = plugin_it.key();
    if (plugin_list->GetDictionaryWithoutPathExpansion(identifier, &plugin)) {
      DCHECK(!identifier_plugin_[identifier]);
      identifier_plugin_[identifier] = CreatePluginMetadata(identifier, plugin);

#if defined(ENABLE_PLUGIN_INSTALLATION)
      if (installers_.find(identifier) == installers_.end())
        installers_[identifier] = new PluginInstaller();
#endif
    }
  }
}

base::string16 PluginFinder::FindPluginNameWithIdentifier(
    const std::string& identifier) {
  base::AutoLock lock(mutex_);
  PluginMap::const_iterator it = identifier_plugin_.find(identifier);
  base::string16 name;
  if (it != identifier_plugin_.end())
    name = it->second->name();

  return name.empty() ? base::UTF8ToUTF16(identifier) : name;
}

std::unique_ptr<PluginMetadata> PluginFinder::GetPluginMetadata(
    const content::WebPluginInfo& plugin) {
  base::AutoLock lock(mutex_);
  for (PluginMap::const_iterator it = identifier_plugin_.begin();
       it != identifier_plugin_.end(); ++it) {
    if (!it->second->MatchesPlugin(plugin))
      continue;

    return it->second->Clone();
  }

  // The plugin metadata was not found, create a dummy one holding
  // the name, identifier and group name only.
  std::string identifier = GetIdentifier(plugin);
  PluginMetadata* metadata = new PluginMetadata(identifier,
                                                GetGroupName(plugin),
                                                false,
                                                GURL(),
                                                GURL(),
                                                plugin.name,
                                                std::string());
  for (size_t i = 0; i < plugin.mime_types.size(); ++i)
    metadata->AddMatchingMimeType(plugin.mime_types[i].mime_type);

  DCHECK(metadata->MatchesPlugin(plugin));
  if (identifier_plugin_.find(identifier) != identifier_plugin_.end())
    identifier = GetLongIdentifier(plugin);

  DCHECK(identifier_plugin_.find(identifier) == identifier_plugin_.end());
  identifier_plugin_[identifier] = metadata;
  return metadata->Clone();
}
