/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef CollectionType_h
#define CollectionType_h

namespace blink {

enum CollectionType {
  // Unnamed HTMLCollection types cached in the document.
  DocImages,   // all <img> elements in the document
  DocApplets,  // all <object> and <applet> elements
  DocEmbeds,   // all <embed> elements
  DocForms,    // all <form> elements
  DocLinks,    // all <a> _and_ <area> elements with a value for href
  DocAnchors,  // all <a> elements with a value for name
  DocScripts,  // all <script> elements
  DocAll,      // "all" elements (IE)

  // Unnamed HTMLCollection types cached in elements.
  NodeChildren,  // first-level children (ParentNode DOM interface)
  TableTBodies,  // all <tbody> elements in this table
  TSectionRows,  // all row elements in this table section
  TableRows,
  TRCells,  // all cells in this row
  TCells,
  SelectOptions,
  SelectedOptions,
  DataListOptions,
  MapAreas,
  FormControls,

  // Named HTMLCollection types cached in the document.
  WindowNamedItems,
  DocumentNamedItems,

  // Named HTMLCollection types cached in elements.
  ClassCollectionType,
  TagCollectionType,
  HTMLTagCollectionType,

  // Live NodeList.
  NameNodeListType,
  RadioNodeListType,
  RadioImgNodeListType,
  LabelsNodeListType,
};

static const CollectionType FirstNamedCollectionType = WindowNamedItems;
static const CollectionType FirstLiveNodeListType = NameNodeListType;

inline bool isUnnamedHTMLCollectionType(CollectionType type) {
  return type < FirstNamedCollectionType;
}

inline bool isHTMLCollectionType(CollectionType type) {
  return type < FirstLiveNodeListType;
}

inline bool isLiveNodeListType(CollectionType type) {
  return type >= FirstLiveNodeListType;
}

}  // namespace blink

#endif
