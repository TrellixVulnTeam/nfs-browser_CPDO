// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/bubble/bubble_delegate.h"

#include "build/build_config.h"
#include "grit/ui_resources_nfs.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#include "base/win/windows_version.h"
#endif

namespace views {

namespace {

// Create a widget to host the bubble.
Widget* CreateBubbleWidget(BubbleDelegateView* bubble) {
  Widget* bubble_widget = new Widget();
  Widget::InitParams bubble_params(Widget::InitParams::TYPE_BUBBLE);
  bubble_params.delegate = bubble;
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN10)
    bubble_params.force_software_compositing = true;
#endif
  bubble_params.opacity = Widget::InitParams::TRANSLUCENT_WINDOW;
  bubble_params.accept_events = bubble->accept_events();
  if (bubble->parent_window())
    bubble_params.parent = bubble->parent_window();
  else if (bubble->anchor_widget())
    bubble_params.parent = bubble->anchor_widget()->GetNativeView();
  bubble_params.activatable = bubble->CanActivate() ?
      Widget::InitParams::ACTIVATABLE_YES : Widget::InitParams::ACTIVATABLE_NO;
  bubble_params.wants_mouse_events_when_inactive = bubble->WantsMouseEventsWhenInactive();
  bubble->OnBeforeBubbleWidgetInit(&bubble_params, bubble_widget);
  bubble_widget->Init(bubble_params);
  if (bubble_params.parent)
    bubble_widget->StackAbove(bubble_params.parent);
  return bubble_widget;
}

}  // namespace

// static
const char BubbleDelegateView::kViewClassName[] = "BubbleDelegateView";

BubbleDelegateView::BubbleDelegateView()
    : BubbleDelegateView(nullptr, BubbleBorder::TOP_LEFT) {}

BubbleDelegateView::BubbleDelegateView(View* anchor_view,
                                       BubbleBorder::Arrow arrow, bool transparent)
    : close_on_esc_(true),
      close_on_deactivate_(true),
      anchor_view_storage_id_(ViewStorage::GetInstance()->CreateStorageID()),
      anchor_widget_(NULL),
      arrow_(arrow),
      shadow_(BubbleBorder::SMALL_SHADOW),
      color_explicitly_set_(false),
      margins_(kPanelVertMargin,
               kPanelHorizMargin,
               kPanelVertMargin,
               kPanelHorizMargin),
      accept_events_(true),
      border_accepts_events_(true),
      adjust_if_offscreen_(true),
      parent_window_(NULL),
      close_reason_(CloseReason::UNKNOWN),
      transparent_(transparent) {
  if (anchor_view)
    SetAnchorView(anchor_view);
  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  UpdateColorsFromTheme(GetNativeTheme());
}

BubbleDelegateView::~BubbleDelegateView() {
  if (GetWidget())
    GetWidget()->RemoveObserver(this);
  SetLayoutManager(NULL);
  SetAnchorView(NULL);
}

// static
Widget* BubbleDelegateView::CreateBubble(BubbleDelegateView* bubble_delegate) {
  bubble_delegate->Init();
  // Get the latest anchor widget from the anchor view at bubble creation time.
  bubble_delegate->SetAnchorView(bubble_delegate->GetAnchorView());
  Widget* bubble_widget = CreateBubbleWidget(bubble_delegate);

#if defined(OS_WIN)
  // If glass is enabled, the bubble is allowed to extend outside the bounds of
  // the parent frame and let DWM handle compositing.  If not, then we don't
  // want to allow the bubble to extend the frame because it will be clipped.
  bubble_delegate->set_adjust_if_offscreen(ui::win::IsAeroGlassEnabled());
#elif (defined(OS_LINUX) && !defined(OS_CHROMEOS)) || defined(OS_MACOSX)
  // Linux clips bubble windows that extend outside their parent window bounds.
  // Mac never adjusts.
  bubble_delegate->set_adjust_if_offscreen(false);
#endif

  bubble_delegate->SizeToContents();
  bubble_widget->AddObserver(bubble_delegate);
  return bubble_widget;
}

BubbleDelegateView* BubbleDelegateView::AsBubbleDelegate() {
  return this;
}

bool BubbleDelegateView::ShouldShowCloseButton() const {
  return false;
}

View* BubbleDelegateView::GetContentsView() {
  return this;
}

NonClientFrameView* BubbleDelegateView::CreateNonClientFrameView(
    Widget* widget) {
  BubbleFrameView* frame = new BubbleFrameView(
      gfx::Insets(kPanelVertMargin, kPanelHorizMargin, 0, kPanelHorizMargin),
      margins());
  // Note: In CreateBubble, the call to SizeToContents() will cause
  // the relayout that this call requires.
  frame->SetTitleFontList(GetTitleFontList());
  frame->SetFootnoteView(CreateFootnoteView());

  if (transparent_) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    gfx::ImageSkia images[3];
    images[0] =  *rb.GetImageNamed(IDD_BUBBLE_CLOSE_N).ToImageSkia();
    images[1] =  *rb.GetImageNamed(IDD_BUBBLE_CLOSE_H).ToImageSkia();
    images[2] =  *rb.GetImageNamed(IDD_BUBBLE_CLOSE_H).ToImageSkia();
    frame->UpdateCloseButton(images);
  }

  BubbleBorder::Arrow adjusted_arrow = arrow();
  if (base::i18n::IsRTL())
    adjusted_arrow = BubbleBorder::horizontal_mirror(adjusted_arrow);
  frame->SetBubbleBorder(std::unique_ptr<BubbleBorder>(
      new BubbleBorder(adjusted_arrow, shadow(), color())));
  return frame;
}

void BubbleDelegateView::GetAccessibleState(ui::AXViewState* state) {
  state->role = ui::AX_ROLE_DIALOG;
}

const char* BubbleDelegateView::GetClassName() const {
  return kViewClassName;
}

void BubbleDelegateView::OnWidgetClosing(Widget* widget) {
  DCHECK(GetBubbleFrameView());
  if (widget == GetWidget() && close_reason_ == CloseReason::UNKNOWN &&
      GetBubbleFrameView()->close_button_clicked()) {
    close_reason_ = CloseReason::CLOSE_BUTTON;
  }
}

void BubbleDelegateView::OnWidgetDestroying(Widget* widget) {
  if (anchor_widget() == widget)
    SetAnchorView(NULL);
}

void BubbleDelegateView::OnWidgetVisibilityChanging(Widget* widget,
                                                    bool visible) {
#if defined(OS_WIN)
  // On Windows we need to handle this before the bubble is visible or hidden.
  // Please see the comment on the OnWidgetVisibilityChanging function. On
  // other platforms it is fine to handle it after the bubble is shown/hidden.
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetVisibilityChanged(Widget* widget,
                                                   bool visible) {
#if !defined(OS_WIN)
  HandleVisibilityChanged(widget, visible);
#endif
}

void BubbleDelegateView::OnWidgetActivationChanged(Widget* widget,
                                                   bool active) {
  if (close_on_deactivate() && widget == GetWidget() && !active) {
    if (close_reason_ == CloseReason::UNKNOWN)
      close_reason_ = CloseReason::DEACTIVATION;
    GetWidget()->Close();
  }
}

void BubbleDelegateView::OnWidgetBoundsChanged(Widget* widget,
                                               const gfx::Rect& new_bounds) {
  if (GetBubbleFrameView() && anchor_widget() == widget)
    SizeToContents();
}

View* BubbleDelegateView::GetAnchorView() const {
  return ViewStorage::GetInstance()->RetrieveView(anchor_view_storage_id_);
}

gfx::Rect BubbleDelegateView::GetAnchorRect() const {
  if (!GetAnchorView())
    return anchor_rect_;

  anchor_rect_ = GetAnchorView()->GetBoundsInScreen();
  anchor_rect_.Inset(anchor_view_insets_);
  return anchor_rect_;
}

void BubbleDelegateView::OnBeforeBubbleWidgetInit(Widget::InitParams* params,
                                                  Widget* widget) const {
}

View* BubbleDelegateView::CreateFootnoteView() {
  return nullptr;
}

void BubbleDelegateView::UseCompactMargins() {
  const int kCompactMargin = 6;
  margins_.Set(kCompactMargin, kCompactMargin, kCompactMargin, kCompactMargin);
}

void BubbleDelegateView::SetAlignment(BubbleBorder::BubbleAlignment alignment) {
  GetBubbleFrameView()->bubble_border()->set_alignment(alignment);
  SizeToContents();
}

void BubbleDelegateView::SetArrowPaintType(
    BubbleBorder::ArrowPaintType paint_type) {
  GetBubbleFrameView()->bubble_border()->set_paint_arrow(paint_type);
  SizeToContents();
}

void BubbleDelegateView::OnAnchorBoundsChanged() {
  SizeToContents();
}

//[zhangyq]add
bool BubbleDelegateView::WantsMouseEventsWhenInactive() const {
 return false;
}

bool BubbleDelegateView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!close_on_esc() || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  close_reason_ = CloseReason::ESCAPE;
  GetWidget()->Close();
  return true;
}

void BubbleDelegateView::OnNativeThemeChanged(const ui::NativeTheme* theme) {
  UpdateColorsFromTheme(theme);
}

void BubbleDelegateView::Init() {}

void BubbleDelegateView::SetAnchorView(View* anchor_view) {
  // When the anchor view gets set the associated anchor widget might
  // change as well.
  if (!anchor_view || anchor_widget() != anchor_view->GetWidget()) {
    if (anchor_widget()) {
      anchor_widget_->RemoveObserver(this);
      anchor_widget_ = NULL;
    }
    if (anchor_view) {
      anchor_widget_ = anchor_view->GetWidget();
      if (anchor_widget_)
        anchor_widget_->AddObserver(this);
    }
  }

  // Remove the old storage item and set the new (if there is one).
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (view_storage->RetrieveView(anchor_view_storage_id_))
    view_storage->RemoveView(anchor_view_storage_id_);
  if (anchor_view)
    view_storage->StoreView(anchor_view_storage_id_, anchor_view);

  // Do not update anchoring for NULL views; this could indicate that our
  // NativeWindow is being destroyed, so it would be dangerous for us to update
  // our anchor bounds at that point. (It's safe to skip this, since if we were
  // to update the bounds when |anchor_view| is NULL, the bubble won't move.)
  if (anchor_view && GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDelegateView::SetAnchorRect(const gfx::Rect& rect) {
  anchor_rect_ = rect;
  if (GetWidget())
    OnAnchorBoundsChanged();
}

void BubbleDelegateView::SizeToContents() {
  GetWidget()->SetBounds(GetBubbleBounds());
}

BubbleFrameView* BubbleDelegateView::GetBubbleFrameView() const {
  const NonClientView* view =
      GetWidget() ? GetWidget()->non_client_view() : NULL;
  return view ? static_cast<BubbleFrameView*>(view->frame_view()) : NULL;
}

gfx::Rect BubbleDelegateView::GetBubbleBounds() {
  // The argument rect has its origin at the bubble's arrow anchor point;
  // its size is the preferred size of the bubble's client view (this view).
  bool anchor_minimized = anchor_widget() && anchor_widget()->IsMinimized();
  gfx::Rect ideal_bound = GetBubbleFrameView()->GetUpdatedWindowBounds(GetAnchorRect(),
      GetPreferredSize(), adjust_if_offscreen_ && !anchor_minimized);

  // huk : 尽力不要让气泡弹出anchor widget bounds
  if (anchor_widget()  && anchor_widget()->IsVisible())  {
    gfx::Rect anchor_widget_bound = anchor_widget()->GetWindowBoundsInScreen();
    int offset_x = 0;
    int offset_y = 0;

    if (ideal_bound.x() < anchor_widget_bound.x())  {
      offset_x = anchor_widget_bound.x() - ideal_bound.x();
    } else if (ideal_bound.x() + ideal_bound.width() > anchor_widget_bound.x() + anchor_widget_bound.width()) {
      offset_x = anchor_widget_bound.x() + anchor_widget_bound.width() - (ideal_bound.x() + ideal_bound.width());
    }

    if (ideal_bound.y() < anchor_widget_bound.y())  {
      offset_y = anchor_widget_bound.y() - ideal_bound.y();
    } else if (ideal_bound.y() + ideal_bound.height() > anchor_widget_bound.y() + anchor_widget_bound.height())  {
      offset_y = anchor_widget_bound.y() + anchor_widget_bound.height() - (ideal_bound.y() + ideal_bound.height());
    }

    ideal_bound.Offset(offset_x, offset_y);
  }
  return ideal_bound;
}

const gfx::FontList& BubbleDelegateView::GetTitleFontList() const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  return rb.GetFontListWithDelta(ui::kTitleFontSizeDelta);
}

void BubbleDelegateView::UpdateColorsFromTheme(const ui::NativeTheme* theme) {
  if(transparent_) {//[zhangyq] when bubble is transparent, don't update color.
    return;
  }

  if (!color_explicitly_set_)
    color_ = theme->GetSystemColor(ui::NativeTheme::kColorId_BubbleBackground);
  set_background(Background::CreateSolidBackground(color()));
  BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view)
    frame_view->bubble_border()->set_background_color(color());
}

void BubbleDelegateView::HandleVisibilityChanged(Widget* widget, bool visible) {
  if (widget == GetWidget() && anchor_widget() &&
      anchor_widget()->GetTopLevelWidget()) {
    anchor_widget()->GetTopLevelWidget()->SetAlwaysRenderAsActive(visible);
  }

  // Fire AX_EVENT_ALERT for bubbles marked as AX_ROLE_ALERT_DIALOG; this
  // instructs accessibility tools to read the bubble in its entirety rather
  // than just its title and initially focused view.  See
  // http://crbug.com/474622 for details.
  if (widget == GetWidget() && visible) {
    ui::AXViewState state;
    GetAccessibleState(&state);
    if (state.role == ui::AX_ROLE_ALERT_DIALOG)
      NotifyAccessibilityEvent(ui::AX_EVENT_ALERT, true);
  }
}

}  // namespace views
