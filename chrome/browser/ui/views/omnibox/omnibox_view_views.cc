// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"

#include <set>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_contents_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_dropdown_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_url_floating_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/omnibox_edit_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/toolbar/toolbar_model.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/selection_model.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/border.h"
#include "ui/views/button_drag_utils.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/browser_process.h"
#endif

using bookmarks::BookmarkNodeData;

namespace {

// OmniboxState ---------------------------------------------------------------

// Stores omnibox state for each tab.
struct OmniboxState : public base::SupportsUserData::Data {
  static const char kKey[];

  OmniboxState(const OmniboxEditModel::State& model_state,
               const gfx::Range& selection,
               const gfx::Range& saved_selection_for_focus_change);
  ~OmniboxState() override;

  const OmniboxEditModel::State model_state;

  // We store both the actual selection and any saved selection (for when the
  // omnibox is not focused).  This allows us to properly handle cases like
  // selecting text, tabbing out of the omnibox, switching tabs away and back,
  // and tabbing back into the omnibox.
  const gfx::Range selection;
  const gfx::Range saved_selection_for_focus_change;
};

// static
const char OmniboxState::kKey[] = "OmniboxState";

OmniboxState::OmniboxState(const OmniboxEditModel::State& model_state,
                           const gfx::Range& selection,
                           const gfx::Range& saved_selection_for_focus_change)
    : model_state(model_state),
      selection(selection),
      saved_selection_for_focus_change(saved_selection_for_focus_change) {
}

OmniboxState::~OmniboxState() {
}

std::string GetShortUrl(GURL& url) {
  std::string short_url = url.GetOrigin().spec();
  if (!url.path().empty()) {
    if (url.path().at(0) == '/') {
      return short_url + url.path().substr(1);
    }
  }
  return short_url + url.path();
}

}  // namespace


// OmniboxViewViews -----------------------------------------------------------

// static
const char OmniboxViewViews::kViewClassName[] = "OmniboxViewViews";

OmniboxViewViews::OmniboxViewViews(OmniboxEditController* controller,
                                   Profile* profile,
                                   CommandUpdater* command_updater,
                                   bool popup_window_mode,
                                   LocationBarView* location_bar,
                                   const gfx::FontList& font_list)
    : OmniboxView(
          controller,
          base::WrapUnique(new ChromeOmniboxClient(controller, profile))),
      profile_(profile),
      popup_window_mode_(popup_window_mode),
      security_level_(security_state::SecurityStateModel::NONE),
      saved_selection_for_focus_change_(gfx::Range::InvalidRange()),
      ime_composing_before_change_(false),
      delete_at_end_pressed_(false),
      location_bar_view_(location_bar),
      ime_candidate_window_open_(false),
      select_all_on_mouse_release_(false),
      select_all_on_gesture_tap_(false),
      weak_ptr_factory_(this) {
  SetBorder(views::Border::NullBorder());
  set_id(VIEW_ID_OMNIBOX);
  SetFontList(font_list);

  // no elide in omnibox
  GetRenderText()->SetElideBehavior(gfx::NO_ELIDE);
}

OmniboxViewViews::~OmniboxViewViews() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      RemoveCandidateWindowObserver(this);
#endif

  // Explicitly teardown members which have a reference to us.  Just to be safe
  // we want them to be destroyed before destroying any other internal state.
  popup_view_.reset();
  dropdown_view_.reset();
}

void OmniboxViewViews::Init() {
  set_controller(this);
  SetTextInputType(ui::TEXT_INPUT_TYPE_URL);

  if (popup_window_mode_)
    SetReadOnly(true);

  if (location_bar_view_) {
    // Initialize the popup view using the same font.
    popup_view_.reset(OmniboxPopupContentsView::Create(
        GetFontList(), this, model(), location_bar_view_));
    dropdown_view_.reset(OmniboxDropdownView::Create(
        GetFontList(), this, model(), location_bar_view_));
     url_floating_view_.reset(OmniboxUrlFloatingView::Create(this,
        base::Bind(&OmniboxViewViews::OnUrlFloatingViewClosed, base::Unretained(this))));
  }

  // huk: set place holder
  if(profile_->GetProfileType() == Profile::INCOGNITO_PROFILE)
    set_placeholder_text(l10n_util::GetStringUTF16(IDS_PLACEHOLDER_LOCATION_BAR_PRIVACY));
  else
    set_placeholder_text(l10n_util::GetStringUTF16(IDS_PLACEHOLDER_LOCATION_BAR));

#if defined(OS_CHROMEOS)
  chromeos::input_method::InputMethodManager::Get()->
      AddCandidateWindowObserver(this);
#endif
}

void OmniboxViewViews::SaveStateToTab(content::WebContents* tab) {
  DCHECK(tab);

  // We don't want to keep the IME status, so force quit the current
  // session here.  It may affect the selection status, so order is
  // also important.
  if (IsIMEComposing()) {
    ConfirmCompositionText();
    GetInputMethod()->CancelComposition(this);
  }

  // NOTE: GetStateForTabSwitch() may affect GetSelectedRange(), so order is
  // important.
  OmniboxEditModel::State state = model()->GetStateForTabSwitch();
  tab->SetUserData(OmniboxState::kKey, new OmniboxState(
      state, GetSelectedRange(), saved_selection_for_focus_change_));
}

void OmniboxViewViews::OnTabChanged(const content::WebContents* web_contents) {
  UpdateSecurityLevel();
  const OmniboxState* state = static_cast<OmniboxState*>(
      web_contents->GetUserData(&OmniboxState::kKey));
  model()->RestoreState(state ? &state->model_state : NULL);
  if (state) {
    // This assumes that the omnibox has already been focused or blurred as
    // appropriate; otherwise, a subsequent OnFocus() or OnBlur() call could
    // goof up the selection.  See comments at the end of
    // BrowserView::ActiveTabChanged().
    SelectRange(state->selection);
    saved_selection_for_focus_change_ = state->saved_selection_for_focus_change;
  }

  // TODO(msw|oshima): Consider saving/restoring edit history.
  ClearEditHistory();
  if (dropdown_view_)  {
    dropdown_view_->ClosePopup();
  }

  CloseUrlFloatingView();
}

void OmniboxViewViews::ResetTabState(content::WebContents* web_contents) {
  web_contents->SetUserData(OmniboxState::kKey, nullptr);
}

void OmniboxViewViews::Update() {
  const security_state::SecurityStateModel::SecurityLevel old_security_level =
      security_level_;
  UpdateSecurityLevel();
  if (model()->UpdatePermanentText()) {
    // Select all the new text if the user had all the old text selected, or if
    // there was no previous text (for new tab page URL replacement extensions).
    // This makes one particular case better: the user clicks in the box to
    // change it right before the permanent URL is changed.  Since the new URL
    // is still fully selected, the user's typing will replace the edit contents
    // as they'd intended.
    const bool was_select_all = IsSelectAll();
    const bool was_reversed = GetSelectedRange().is_reversed();

    RevertAll();

    if (!model()->has_focus()) {
      GURL url = model()->PermanentURL();
      const base::string16& text = base::UTF8ToUTF16(GetShortUrl(url));
      SetWindowTextAndCaretPos(text, text.length(), false, true);
    }

    // Only select all when we have focus.  If we don't have focus, selecting
    // all is unnecessary since the selection will change on regaining focus,
    // and can in fact cause artifacts, e.g. if the user is on the NTP and
    // clicks a link to navigate, causing |was_select_all| to be vacuously true
    // for the empty omnibox, and we then select all here, leading to the
    // trailing portion of a long URL being scrolled into view.  We could try
    // and address cases like this, but it seems better to just not muck with
    // things when the omnibox isn't focused to begin with.
    if (was_select_all && model()->has_focus())
      SelectAll(was_reversed);
  } else if (old_security_level != security_level_) {
    EmphasizeURLComponents();
  }
}

base::string16 OmniboxViewViews::GetText() const {
  // TODO(oshima): IME support
  return text();
}

void OmniboxViewViews::SetUserText(const base::string16& text,
                                   bool update_popup) {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::SetUserText(text, update_popup);
}

void OmniboxViewViews::EnterKeywordModeForDefaultSearchProvider() {
  // Transition the user into keyword mode using their default search provider.
  // Select their query if they typed one.
  model()->EnterKeywordModeForDefaultSearchProvider(
      KeywordModeEntryMethod::KEYBOARD_SHORTCUT);
  SelectRange(gfx::Range(text().size(), 0));
}

void OmniboxViewViews::GetSelectionBounds(
    base::string16::size_type* start,
    base::string16::size_type* end) const {
  const gfx::Range range = GetSelectedRange();
  *start = static_cast<size_t>(range.start());
  *end = static_cast<size_t>(range.end());
}

void OmniboxViewViews::SelectAll(bool reversed) {
  views::Textfield::SelectAll(reversed);
}

void OmniboxViewViews::RevertAll() {
  saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  OmniboxView::RevertAll();
}

void OmniboxViewViews::SetFocus() {
  RequestFocus();
  // Restore caret visibility if focus is explicitly requested. This is
  // necessary because if we already have invisible focus, the RequestFocus()
  // call above will short-circuit, preventing us from reaching
  // OmniboxEditModel::OnSetFocus(), which handles restoring visibility when the
  // omnibox regains focus after losing focus.
  model()->SetCaretVisibility(true);
}

int OmniboxViewViews::GetTextWidth() const {
  // Returns the width necessary to display the current text, including any
  // necessary space for the cursor or border/margin.
  return GetRenderText()->GetContentWidth() + GetInsets().width();
}

bool OmniboxViewViews::IsImeComposing() const {
  return IsIMEComposing();
}

gfx::Size OmniboxViewViews::GetMinimumSize() const {
  const int kMinCharacters = 10;
  return gfx::Size(
      GetFontList().GetExpectedTextWidth(kMinCharacters) + GetInsets().width(),
      GetPreferredSize().height());
}

void OmniboxViewViews::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  views::Textfield::OnNativeThemeChanged(theme);
  if (location_bar_view_) {
    //[zhangyq] use transparent background if has custom theme.
    // SetBackgroundColor(
    //     location_bar_view_->GetColor(LocationBarView::BACKGROUND));
    if (GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
      set_placeholder_text_color(SK_ColorGRAY);
      SetBackgroundColor(SK_ColorTRANSPARENT);
    } else {
      set_placeholder_text_color(SK_ColorLTGRAY);
      SetBackgroundColor(
            location_bar_view_->GetColor(LocationBarView::BACKGROUND));
    }
  }
  EmphasizeURLComponents();
}

void OmniboxViewViews::OnThemeChanged() {
  if (location_bar_view_) {
    if (GetThemeProvider()->HasCustomImage(IDR_THEME_TOOLBAR)) {
      set_placeholder_text_color(SK_ColorGRAY);
      SetBackgroundColor(SK_ColorTRANSPARENT);
    } else {
      set_placeholder_text_color(SK_ColorLTGRAY);
      SetBackgroundColor(
            location_bar_view_->GetColor(LocationBarView::BACKGROUND));
    }
  }
}

void OmniboxViewViews::OnPaint(gfx::Canvas* canvas) {
  Textfield::OnPaint(canvas);
  if (!insert_char_time_.is_null()) {
    UMA_HISTOGRAM_TIMES("Omnibox.CharTypedToRepaintLatency",
                        base::TimeTicks::Now() - insert_char_time_);
    insert_char_time_ = base::TimeTicks();
  }
}

void OmniboxViewViews::ExecuteCommand(int command_id, int event_flags) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();
  switch (command_id) {
    // These commands don't invoke the popup via OnBefore/AfterPossibleChange().
    case IDS_PASTE_AND_GO:
      model()->PasteAndGo(GetClipboardText());
      return;
    case IDC_EDIT_SEARCH_ENGINES:
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      return;

    // These commands do invoke the popup.
    case IDS_APP_PASTE:
      ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
      return;
    default:
      if (Textfield::IsCommandIdEnabled(command_id)) {
        // The Textfield code will invoke OnBefore/AfterPossibleChange() itself
        // as necessary.
        Textfield::ExecuteCommand(command_id, event_flags);
        return;
      }
      OnBeforePossibleChange();
      location_bar_view_->command_updater()->ExecuteCommand(command_id);
      OnAfterPossibleChange(true);
      return;
  }
}

ui::TextInputType OmniboxViewViews::GetTextInputType() const {
  ui::TextInputType input_type = views::Textfield::GetTextInputType();
  // We'd like to set the text input type to TEXT_INPUT_TYPE_URL, because this
  // triggers URL-specific layout in software keyboards, e.g. adding top-level
  // "/" and ".com" keys for English.  However, this also causes IMEs to default
  // to Latin character mode, which makes entering search queries difficult for
  // IME users. Therefore, we try to guess whether an IME will be used based on
  // the input language, and set the input type accordingly.
#if defined(OS_WIN)
  if (input_type != ui::TEXT_INPUT_TYPE_NONE && location_bar_view_) {
    ui::InputMethod* input_method =
        location_bar_view_->GetWidget()->GetInputMethod();
    if (input_method && input_method->IsInputLocaleCJK())
      return ui::TEXT_INPUT_TYPE_SEARCH;
  }
#endif
  return input_type;
}


void OmniboxViewViews::SetTextAndSelectedRange(const base::string16& text,
                                               const gfx::Range& range) {
  base::string16 temp(text);
  base::ReplaceFirstSubstringAfterOffset(
      &temp, 0,
      base::ASCIIToUTF16(chrome::kNfsSchemes),
      base::ASCIIToUTF16("about"));
  SetText(temp);
  SelectRange(range);
}

base::string16 OmniboxViewViews::GetSelectedText() const {
  // TODO(oshima): Support IME.
  return views::Textfield::GetSelectedText();
}

void OmniboxViewViews::OnPaste() {
  const base::string16 text(GetClipboardText());
  if (!text.empty()) {
    OnBeforePossibleChange();
    // Record this paste, so we can do different behavior.
    model()->OnPaste();
    // Force a Paste operation to trigger the text_changed code in
    // OnAfterPossibleChange(), even if identical contents are pasted.
    state_before_change_.text.clear();
    InsertOrReplaceText(text);
    OnAfterPossibleChange(true);
  }
}

bool OmniboxViewViews::HandleEarlyTabActions(const ui::KeyEvent& event) {
  // This must run before accelerator handling invokes a focus change on tab.
  // Note the parallel with SkipDefaultKeyEventProcessing above.
  if (!views::FocusManager::IsTabTraversalKeyEvent(event))
    return false;

#if 0 // huk disable this
  if (model()->is_keyword_hint() && !event.IsShiftDown())
    return model()->AcceptKeyword(KeywordModeEntryMethod::TAB);
#endif

  if (!model()->popup_model()->IsOpen())
    return false;

   // huk disable the keyword hint
  if (false && event.IsShiftDown() &&
      (model()->popup_model()->selected_line_state() ==
          OmniboxPopupModel::KEYWORD))
    model()->ClearKeyword();
  else
    model()->OnUpOrDownKeyPressed(event.IsShiftDown() ? -1 : 1);

  return true;
}

void OmniboxViewViews::AccessibilitySetValue(const base::string16& new_value) {
  SetUserText(new_value, true);
}

void OmniboxViewViews::UpdateSecurityLevel() {
  security_level_ = controller()->GetToolbarModel()->GetSecurityLevel(false);
}

void OmniboxViewViews::SetWindowTextAndCaretPos(const base::string16& text,
                                                size_t caret_pos,
                                                bool update_popup,
                                                bool notify_text_changed) {
  const gfx::Range range(caret_pos, caret_pos);
  SetTextAndSelectedRange(text, range);

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed)
    TextChanged();
}

bool OmniboxViewViews::IsSelectAll() const {
  // TODO(oshima): IME support.
  return text() == GetSelectedText();
}

bool OmniboxViewViews::DeleteAtEndPressed() {
  return delete_at_end_pressed_;
}

void OmniboxViewViews::UpdatePopup() {
  model()->SetInputInProgress(true);
  if (!model()->has_focus())
    return;

  // Prevent inline autocomplete when the caret isn't at the end of the text.
  const gfx::Range sel = GetSelectedRange();
  model()->StartAutocomplete(!sel.is_empty(), sel.GetMax() < text().length(),
                             false);
}

void OmniboxViewViews::ApplyCaretVisibility() {
  SetCursorEnabled(model()->is_caret_visible());
}

void OmniboxViewViews::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    bool save_original_selection,
    bool notify_text_changed) {
  if (save_original_selection)
    saved_temporary_selection_ = GetSelectedRange();

  SetWindowTextAndCaretPos(display_text, display_text.length(), false,
                           notify_text_changed);
}

bool OmniboxViewViews::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  if (display_text == text())
    return false;

  if (!IsIMEComposing()) {
    gfx::Range range(display_text.size(), user_text_length);
    SetTextAndSelectedRange(display_text, range);
  } else if (location_bar_view_) {
    location_bar_view_->SetImeInlineAutocompletion(
        display_text.substr(user_text_length));
  }
  TextChanged();
  return true;
}

void OmniboxViewViews::OnInlineAutocompleteTextCleared() {
  // Hide the inline autocompletion for IME users.
  if (location_bar_view_)
    location_bar_view_->SetImeInlineAutocompletion(base::string16());
}

void OmniboxViewViews::OnRevertTemporaryText() {
  SelectRange(saved_temporary_selection_);
  // We got here because the user hit the Escape key. We explicitly don't call
  // TextChanged(), since OmniboxPopupModel::ResetToDefaultMatch() has already
  // been called by now, and it would've called TextChanged() if it was
  // warranted.
}

void OmniboxViewViews::OnBeforePossibleChange() {
  // Record our state.
  GetState(&state_before_change_);
  ime_composing_before_change_ = IsIMEComposing();
}

bool OmniboxViewViews::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  // See if the text or selection have changed since OnBeforePossibleChange().
  State new_state;
  GetState(&new_state);
  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  state_changes.text_differs =
      state_changes.text_differs ||
      (ime_composing_before_change_ != IsIMEComposing());

  const bool something_changed = model()->OnAfterPossibleChange(
      state_changes, allow_keyword_ui_change && !IsIMEComposing());

  // If only selection was changed, we don't need to call model()'s
  // OnChanged() method, which is called in TextChanged().
  // But we still need to call EmphasizeURLComponents() to make sure the text
  // attributes are updated correctly.
  if (something_changed &&
      (state_changes.text_differs || state_changes.keyword_differs))
    TextChanged();
  else if (state_changes.selection_differs)
    EmphasizeURLComponents();
  else if (delete_at_end_pressed_)
    model()->OnChanged();

  return something_changed;
}

gfx::NativeView OmniboxViewViews::GetNativeView() const {
  return GetWidget()->GetNativeView();
}

gfx::NativeView OmniboxViewViews::GetRelativeWindowForPopup() const {
  return GetWidget()->GetTopLevelWidget()->GetNativeView();
}

void OmniboxViewViews::SetGrayTextAutocompletion(const base::string16& input) {
  if (location_bar_view_)
    location_bar_view_->SetGrayTextAutocompletion(input);
}

base::string16 OmniboxViewViews::GetGrayTextAutocompletion() const {
  return location_bar_view_ ?
      location_bar_view_->GetGrayTextAutocompletion() : base::string16();
}

int OmniboxViewViews::GetWidth() const {
  return location_bar_view_ ? location_bar_view_->width() : 0;
}

bool OmniboxViewViews::IsImeShowingPopup() const {
#if defined(OS_CHROMEOS)
  return ime_candidate_window_open_;
#else
  return GetInputMethod() ? GetInputMethod()->IsCandidatePopupOpen() : false;
#endif
}

void OmniboxViewViews::ShowImeIfNeeded() {
  GetInputMethod()->ShowImeIfNeeded();
}

void OmniboxViewViews::OnMatchOpened(AutocompleteMatch::Type match_type) {
}

int OmniboxViewViews::GetOmniboxTextLength() const {
  // TODO(oshima): Support IME.
  return static_cast<int>(text().length());
}

void OmniboxViewViews::EmphasizeURLComponents() {
  if (!location_bar_view_)
    return;

  // If the current contents is a URL, force left-to-right rendering at the
  // paragraph level. Right-to-left runs are still rendered RTL, but will not
  // flip the whole URL around. For example (if "ABC" is Hebrew), this will
  // render "ABC.com" as "CBA.com", rather than "com.CBA".
  bool text_is_url = model()->CurrentTextIsURL();
  GetRenderText()->SetDirectionalityMode(text_is_url
                                             ? gfx::DIRECTIONALITY_FORCE_LTR
                                             : gfx::DIRECTIONALITY_FROM_TEXT);

  // See whether the contents are a URL with a non-empty host portion, which we
  // should emphasize.  To check for a URL, rather than using the type returned
  // by Parse(), ask the model, which will check the desired page transition for
  // this input.  This can tell us whether an UNKNOWN input string is going to
  // be treated as a search or a navigation, and is the same method the Paste
  // And Go system uses.
  url::Component scheme, host;
  AutocompleteInput::ParseForEmphasizeComponents(
      text(), ChromeAutocompleteSchemeClassifier(profile_), &scheme, &host);
  bool grey_out_url = text().substr(scheme.begin, scheme.len) ==
      base::UTF8ToUTF16(extensions::kExtensionScheme);
  bool grey_base = text_is_url && (host.is_nonempty() || grey_out_url);
  SetColor(location_bar_view_->GetColor(
      grey_base ? LocationBarView::DEEMPHASIZED_TEXT : LocationBarView::TEXT));
  if (grey_base && !grey_out_url) {
    ApplyColor(location_bar_view_->GetColor(LocationBarView::TEXT),
               gfx::Range(host.begin, host.end()));
  }

  // Emphasize the scheme for security UI display purposes (if necessary).
  // Note that we check CurrentTextIsURL() because if we're replacing search
  // URLs with search terms, we may have a non-URL even when the user is not
  // editing; and in some cases, e.g. for "site:foo.com" searches, the parser
  // may have incorrectly identified a qualifier as a scheme.
  SetStyle(gfx::DIAGONAL_STRIKE, false);
  if (!model()->user_input_in_progress() && text_is_url &&
      scheme.is_nonempty() &&
      (security_level_ != security_state::SecurityStateModel::NONE)) {
    SkColor security_color =
        location_bar_view_->GetSecureTextColor(security_level_);
    const bool strike =
        (security_level_ == security_state::SecurityStateModel::DANGEROUS);
    const gfx::Range scheme_range(scheme.begin, scheme.end());
    ApplyColor(security_color, scheme_range);
    ApplyStyle(gfx::DIAGONAL_STRIKE, strike, scheme_range);
  }
}

bool OmniboxViewViews::IsItemForCommandIdDynamic(int command_id) const {
  return command_id == IDS_PASTE_AND_GO;
}

base::string16 OmniboxViewViews::GetLabelForCommandId(int command_id) const {
  DCHECK_EQ(IDS_PASTE_AND_GO, command_id);
  return l10n_util::GetStringUTF16(
      model()->IsPasteAndSearch(GetClipboardText()) ?
          IDS_PASTE_AND_SEARCH : IDS_PASTE_AND_GO);
}

const char* OmniboxViewViews::GetClassName() const {
  return kViewClassName;
}

bool OmniboxViewViews::OnMousePressed(const ui::MouseEvent& event) {
  select_all_on_mouse_release_ =
      (event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      (!HasFocus() || (model()->focus_state() == OMNIBOX_FOCUS_INVISIBLE));
  if (select_all_on_mouse_release_) {
    // Restore caret visibility whenever the user clicks in the omnibox in a way
    // that would give it focus.  We must handle this case separately here
    // because if the omnibox currently has invisible focus, the mouse event
    // won't trigger either SetFocus() or OmniboxEditModel::OnSetFocus().
    model()->SetCaretVisibility(true);

    // When we're going to select all on mouse release, invalidate any saved
    // selection lest restoring it fights with the "select all" action.  It's
    // possible to later set select_all_on_mouse_release_ back to false, but
    // that happens for things like dragging, which are cases where having
    // invalidated this saved selection is still OK.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }
  return views::Textfield::OnMousePressed(event);
}

bool OmniboxViewViews::OnMouseDragged(const ui::MouseEvent& event) {
  if (HasTextBeingDragged())
    CloseOmniboxPopup();

  bool handled = views::Textfield::OnMouseDragged(event);

  if (HasSelection() ||
      ExceededDragThreshold(event.location() - last_click_location())) {
    select_all_on_mouse_release_ = false;
  }

  return handled;
}

void OmniboxViewViews::OnMouseReleased(const ui::MouseEvent& event) {
  views::Textfield::OnMouseReleased(event);
  // if the mouse is moved out of the locationbar, skip the command.
  gfx::Point location = event.location();
  if (location.x() < 0 || location.x() > location_bar_view_->width() ||
      location.y() < 0 || location.y() > location_bar_view_->height()) {
    return;
  }

  // When the user has clicked and released to give us focus, select all.
  if ((event.IsOnlyLeftMouseButton() || event.IsOnlyRightMouseButton()) &&
      select_all_on_mouse_release_) {
    // Select all in the reverse direction so as not to scroll the caret
    // into view and shift the contents jarringly.
    SelectAll(true);
  }

  // check and show the copy and paste tip
  if (event.IsOnlyLeftMouseButton() &&
       (select_all_on_mouse_release_ || GetText().empty())) {
    location.set_y(location_bar_view_->y() - location_bar_view_->height() - 4);
    View::ConvertPointToScreen(this, &location);
    CheckAndShowFloatLayer(location);
  }

  select_all_on_mouse_release_ = false;
}

void OmniboxViewViews::OnGestureEvent(ui::GestureEvent* event) {
  if (!HasFocus() && event->type() == ui::ET_GESTURE_TAP_DOWN) {
    select_all_on_gesture_tap_ = true;

    // If we're trying to select all on tap, invalidate any saved selection lest
    // restoring it fights with the "select all" action.
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }

  views::Textfield::OnGestureEvent(event);

  if (select_all_on_gesture_tap_ && event->type() == ui::ET_GESTURE_TAP)
    SelectAll(true);

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_CANCEL ||
      event->type() == ui::ET_GESTURE_TWO_FINGER_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_PINCH_BEGIN ||
      event->type() == ui::ET_GESTURE_LONG_PRESS ||
      event->type() == ui::ET_GESTURE_LONG_TAP) {
    select_all_on_gesture_tap_ = false;
  }
}

void OmniboxViewViews::AboutToRequestFocusFromTabTraversal(bool reverse) {
  views::Textfield::AboutToRequestFocusFromTabTraversal(reverse);
}

bool OmniboxViewViews::SkipDefaultKeyEventProcessing(
    const ui::KeyEvent& event) {
  if (views::FocusManager::IsTabTraversalKeyEvent(event) &&
      ((model()->is_keyword_hint() && !event.IsShiftDown()) ||
       model()->popup_model()->IsOpen())) {
    return true;
  }
  if (event.key_code() == ui::VKEY_ESCAPE)
    return model()->WillHandleEscapeKey();
  return Textfield::SkipDefaultKeyEventProcessing(event);
}

void OmniboxViewViews::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_TEXT_FIELD;
  state->name = l10n_util::GetStringUTF16(IDS_ACCNAME_LOCATION);
  state->value = GetText();

  base::string16::size_type entry_start;
  base::string16::size_type entry_end;
  GetSelectionBounds(&entry_start, &entry_end);
  state->selection_start = entry_start;
  state->selection_end = entry_end;

  if (popup_window_mode_) {
    state->AddStateFlag(ui::AX_STATE_READ_ONLY);
  } else {
    state->AddStateFlag(ui::AX_STATE_EDITABLE);
    state->set_value_callback =
        base::Bind(&OmniboxViewViews::AccessibilitySetValue,
                   weak_ptr_factory_.GetWeakPtr());
  }
}

void OmniboxViewViews::OnFocus() {
  const base::string16& text = base::UTF8ToUTF16(model()->PermanentURL().spec());
  SetWindowTextAndCaretPos(text, text.length(), false, true);
  views::Textfield::OnFocus();
  // TODO(oshima): Get control key state.
  model()->OnSetFocus(false);
  // Don't call controller()->OnSetFocus, this view has already acquired focus.

  // Restore the selection we saved in OnBlur() if it's still valid.
  if (saved_selection_for_focus_change_.IsValid()) {
    SelectRange(saved_selection_for_focus_change_);
    saved_selection_for_focus_change_ = gfx::Range::InvalidRange();
  }
}


void OmniboxViewViews::OnBlur() {
  GURL url = model()->PermanentURL();
  const base::string16& text = base::UTF8ToUTF16(GetShortUrl(url));
  SetWindowTextAndCaretPos(text, text.length(), false, true);
  // Save the user's existing selection to restore it later.
  saved_selection_for_focus_change_ = GetSelectedRange();

  views::Textfield::OnBlur();
  model()->OnWillKillFocus();

  // If ZeroSuggest is active, we may have refused to show an update to the
  // underlying permanent URL that happened while the popup was open, so
  // revert to ensure that update is shown now.  Otherwise, make sure to call
  // CloseOmniboxPopup() unconditionally, so that if ZeroSuggest is in the midst
  // of running but hasn't yet opened the popup, it will be halted.
  if (!model()->user_input_in_progress() && model()->popup_model()->IsOpen())
    RevertAll();
  else
    CloseOmniboxPopup();

  location_bar_view_->CloseOmniboxDropdownView();

  CloseUrlFloatingView();

  // Tell the model to reset itself.
  model()->OnKillFocus();

  // Make sure the beginning of the text is visible.
  SelectRange(gfx::Range(0));

  // The location bar needs to repaint without a focus ring.
  if (ui::MaterialDesignController::IsModeMaterial())
    location_bar_view_->SchedulePaint();
}

bool OmniboxViewViews::IsCommandIdEnabled(int command_id) const {
  if (command_id == IDS_APP_PASTE)
    return !read_only() && !GetClipboardText().empty();
  if (command_id == IDS_PASTE_AND_GO)
    return !read_only() && model()->CanPasteAndGo(GetClipboardText());
  return Textfield::IsCommandIdEnabled(command_id) ||
         location_bar_view_->command_updater()->IsCommandEnabled(command_id);
}

base::string16 OmniboxViewViews::GetSelectionClipboardText() const {
  return SanitizeTextForPaste(Textfield::GetSelectionClipboardText());
}

void OmniboxViewViews::DoInsertChar(base::char16 ch) {
  // If |insert_char_time_| is not null, there's a pending insert char operation
  // that hasn't been painted yet. Keep the earlier time.
  if (insert_char_time_.is_null())
    insert_char_time_ = base::TimeTicks::Now();
  Textfield::DoInsertChar(ch);
}

bool OmniboxViewViews::IsTextEditCommandEnabled(
    ui::TextEditCommand command) const {
  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
    case ui::TextEditCommand::MOVE_DOWN:
      return !read_only();
    case ui::TextEditCommand::PASTE:
      return !read_only() && !GetClipboardText().empty();
    default:
      return Textfield::IsTextEditCommandEnabled(command);
  }
}

void OmniboxViewViews::ExecuteTextEditCommand(ui::TextEditCommand command) {
  // In the base class, touch text selection is deactivated when a command is
  // executed. Since we are not always calling the base class implementation
  // here, we need to deactivate touch text selection here, too.
  DestroyTouchSelection();

  if (!IsTextEditCommandEnabled(command))
    return;

  switch (command) {
    case ui::TextEditCommand::MOVE_UP:
      model()->OnUpOrDownKeyPressed(-1);
      break;
    case ui::TextEditCommand::MOVE_DOWN:
      model()->OnUpOrDownKeyPressed(1);
      break;
    case ui::TextEditCommand::PASTE:
      OnPaste();
      break;
    default:
      Textfield::ExecuteTextEditCommand(command);
      break;
  }
}

#if defined(OS_CHROMEOS)
void OmniboxViewViews::CandidateWindowOpened(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = true;
}

void OmniboxViewViews::CandidateWindowClosed(
      chromeos::input_method::InputMethodManager* manager) {
  ime_candidate_window_open_ = false;
}
#endif

void OmniboxViewViews::ContentsChanged(views::Textfield* sender,
                                       const base::string16& new_contents) {
}

bool OmniboxViewViews::HandleKeyEvent(views::Textfield* textfield,
                                      const ui::KeyEvent& event) {
  if (event.type() == ui::ET_KEY_RELEASED) {
    // The omnibox contents may change while the control key is pressed.
    if (event.key_code() == ui::VKEY_CONTROL)
      model()->OnControlKeyChanged(false);

    return false;
  }

  delete_at_end_pressed_ = false;

  // Skip processing of [Alt]+<num-pad digit> Unicode alt key codes.
  // Otherwise, if num-lock is off, the events are handled as [Up], [Down], etc.
  if (event.IsUnicodeKeyCode())
    return false;

  const bool shift = event.IsShiftDown();
  const bool control = event.IsControlDown();
  const bool alt = event.IsAltDown() || event.IsAltGrDown();
  switch (event.key_code()) {
    case ui::VKEY_RETURN:
      model()->AcceptInput(alt ? WindowOpenDisposition::NEW_FOREGROUND_TAB
                               : WindowOpenDisposition::CURRENT_TAB,
                           false);
      return true;
    case ui::VKEY_ESCAPE:
      return model()->OnEscapeKeyPressed();
    case ui::VKEY_CONTROL:
      model()->OnControlKeyChanged(true);
      break;
    case ui::VKEY_DELETE:
      if (shift && model()->popup_model()->IsOpen())
        model()->popup_model()->TryDeletingCurrentItem();

      delete_at_end_pressed_ = (!event.IsAltDown() && !HasSelection() &&
                                GetCursorPosition() == text().length());
      break;
    case ui::VKEY_UP:
      if (IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_UP)) {
        ExecuteTextEditCommand(ui::TextEditCommand::MOVE_UP);
        return true;
      }
      break;
    case ui::VKEY_DOWN:
      if (IsTextEditCommandEnabled(ui::TextEditCommand::MOVE_DOWN)) {
        ExecuteTextEditCommand(ui::TextEditCommand::MOVE_DOWN);
        return true;
      }
      break;
    case ui::VKEY_PRIOR:
      if (control || alt || shift)
        return false;
      model()->OnUpOrDownKeyPressed(-1 * model()->result().size());
      return true;
    case ui::VKEY_NEXT:
      if (control || alt || shift)
        return false;
      model()->OnUpOrDownKeyPressed(model()->result().size());
      return true;
    case ui::VKEY_V:
      if (control && !alt &&
          IsTextEditCommandEnabled(ui::TextEditCommand::PASTE)) {
        ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
        return true;
      }
      break;
    case ui::VKEY_INSERT:
      if (shift && !control &&
          IsTextEditCommandEnabled(ui::TextEditCommand::PASTE)) {
        ExecuteTextEditCommand(ui::TextEditCommand::PASTE);
        return true;
      }
      break;
    case ui::VKEY_BACK:
      // No extra handling is needed in keyword search mode, if there is a
      // non-empty selection, or if the cursor is not leading the text.
      if (model()->is_keyword_hint() || model()->keyword().empty() ||
          HasSelection() || GetCursorPosition() != 0)
        return false;
      model()->ClearKeyword();
      return true;

    // Handle the right-arrow key for LTR text and the left-arrow key for RTL
    // text if there is gray text that needs to be committed.
    case ui::VKEY_RIGHT:
      if (GetCursorPosition() == text().length() &&
          GetTextDirection() == base::i18n::LEFT_TO_RIGHT) {
        return model()->CommitSuggestedText();
      }
      break;
    case ui::VKEY_LEFT:
      if (GetCursorPosition() == text().length() &&
          GetTextDirection() == base::i18n::RIGHT_TO_LEFT) {
        return model()->CommitSuggestedText();
      }
      break;

    default:
      break;
  }

  return HandleEarlyTabActions(event);
}

void OmniboxViewViews::OnBeforeUserAction(views::Textfield* sender) {
  OnBeforePossibleChange();
}

void OmniboxViewViews::OnAfterUserAction(views::Textfield* sender) {
  OnAfterPossibleChange(true);
}

void OmniboxViewViews::OnAfterCutOrCopy(ui::ClipboardType clipboard_type) {
  ui::Clipboard* cb = ui::Clipboard::GetForCurrentThread();
  base::string16 selected_text;
  cb->ReadText(clipboard_type, &selected_text);
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (IsSelectAll())
    UMA_HISTOGRAM_COUNTS(OmniboxEditModel::kCutOrCopyAllTextHistogram, 1);

  if (write_url) {
    BookmarkNodeData data;
    data.ReadFromTuple(url, selected_text);
    data.WriteToClipboard(clipboard_type);
  } else {
    ui::ScopedClipboardWriter scoped_clipboard_writer(clipboard_type);
    scoped_clipboard_writer.WriteText(selected_text);
  }
}

void OmniboxViewViews::OnWriteDragData(ui::OSExchangeData* data) {
  GURL url;
  bool write_url;
  bool is_all_selected = IsSelectAll();
  base::string16 selected_text = GetSelectedText();
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), is_all_selected,
                             &selected_text, &url, &write_url);
  data->SetString(selected_text);
  if (write_url) {
    gfx::Image favicon;
    base::string16 title = selected_text;
    if (is_all_selected)
      model()->GetDataForURLExport(&url, &title, &favicon);
    button_drag_utils::SetURLAndDragImage(url, title, favicon.AsImageSkia(),
                                          NULL, data, GetWidget());
    data->SetURL(url, title);
  }
}

void OmniboxViewViews::OnGetDragOperationsForTextfield(int* drag_operations) {
  base::string16 selected_text = GetSelectedText();
  GURL url;
  bool write_url;
  model()->AdjustTextForCopy(GetSelectedRange().GetMin(), IsSelectAll(),
                             &selected_text, &url, &write_url);
  if (write_url)
    *drag_operations |= ui::DragDropTypes::DRAG_LINK;
}

void OmniboxViewViews::AppendDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  *formats = *formats | ui::OSExchangeData::URL;
}

int OmniboxViewViews::OnDrop(const ui::OSExchangeData& data) {
  if (HasTextBeingDragged())
    return ui::DragDropTypes::DRAG_NONE;

  if (data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES)) {
    GURL url;
    base::string16 title;
    if (data.GetURLAndTitle(
            ui::OSExchangeData::CONVERT_FILENAMES, &url, &title)) {
      base::string16 text(
          StripJavascriptSchemas(base::UTF8ToUTF16(url.spec())));
      if (model()->CanPasteAndGo(text)) {
        model()->PasteAndGo(text);
        return ui::DragDropTypes::DRAG_COPY;
      }
    }
  } else if (data.HasString()) {
    base::string16 text;
    if (data.GetString(&text)) {
      base::string16 collapsed_text(base::CollapseWhitespace(text, true));
      if (model()->CanPasteAndGo(collapsed_text))
        model()->PasteAndGo(collapsed_text);
      return ui::DragDropTypes::DRAG_COPY;
    }
  }

  return ui::DragDropTypes::DRAG_NONE;
}

void OmniboxViewViews::UpdateContextMenu(ui::SimpleMenuModel* menu_contents) {
  int paste_position = menu_contents->GetIndexOfCommandId(IDS_APP_PASTE);
  DCHECK_GE(paste_position, 0);
  menu_contents->InsertItemWithStringIdAt(
      paste_position + 1, IDS_PASTE_AND_GO, IDS_PASTE_AND_GO);

  menu_contents->AddSeparator(ui::NORMAL_SEPARATOR);

  // Minor note: We use IDC_ for command id here while the underlying textfield
  // is using IDS_ for all its command ids. This is because views cannot depend
  // on IDC_ for now.
  menu_contents->AddItemWithStringId(IDC_EDIT_SEARCH_ENGINES,
      IDS_EDIT_SEARCH_ENGINES);
}

void OmniboxViewViews::CheckAndShowFloatLayer(const gfx::Point& location) {
  // preference control
  if (!profile_->GetPrefs()->GetBoolean(prefs::kAddressClip))  {
    return;
  }
  bool needToShowCopyButton = IsInputTextURL(GetSelectedText());
  bool needToShowPasteButton = IsInputTextURL(GetClipboardText());

  if (url_floating_view_) {
    int show_option = OmniboxUrlFloatingView::URL_NONE;
    if (needToShowCopyButton) {
      show_option |= OmniboxUrlFloatingView::URL_COPY;
    }
    if (needToShowPasteButton) {
      show_option |= OmniboxUrlFloatingView::URL_PASTE_AND_GO;
    }

    if (show_option != OmniboxUrlFloatingView::URL_NONE && !url_floating_view_->IsOpen()) {
      url_floating_view_->Show(location, show_option);
    }
  }
}

bool OmniboxViewViews::IsInputTextURL(base::string16 text)  const {
  if (text.empty()) {
    return false;
  }
  const int max_length = 1000;
  if (text.length() > max_length)  {
    text = text.substr(0, max_length);
  }

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(profile_)->Classify(text, false, false,
      metrics::OmniboxEventProto::INVALID_SPEC, &match, NULL);
  if (match.type == AutocompleteMatchType::URL_WHAT_YOU_TYPED ||
      match.type == AutocompleteMatchType::HISTORY_URL ||
      match.type == AutocompleteMatchType::NAVSUGGEST ||
      match.type == AutocompleteMatchType::NAVSUGGEST_PERSONALIZED) {
    return true;
  }
  return false;
}

void OmniboxViewViews::OnUrlFloatingViewClosed(int result) {
    if (result & OmniboxUrlFloatingView::URL_COPY) {
      ExecuteCommand(IDS_APP_COPY, 0);
    } else if (result & OmniboxUrlFloatingView::URL_PASTE_AND_GO) {
      ExecuteCommand(IDS_PASTE_AND_GO, 0);
    }
}

void OmniboxViewViews::CloseUrlFloatingView() {
  if(url_floating_view_) {
    url_floating_view_->ClosePopup();
  }
}
