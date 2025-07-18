/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLFormControlAccessible.h"

#include "Accessible-inl.h"
#include "nsAccUtils.h"
#include "nsEventShell.h"
#include "nsTextEquivUtils.h"
#include "Relation.h"
#include "Role.h"
#include "States.h"

#include "nsContentList.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "nsIFormControl.h"
#include "nsIPersistentProperties2.h"
#include "nsITextControlFrame.h"
#include "nsNameSpaceManager.h"
#include "mozilla/dom/ScriptSettings.h"

#include "mozilla/EventStates.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextControlElement.h"
#include "mozilla/TextEditor.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLFormAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLFormAccessible::NativeRole() const {
  nsAutoString name;
  const_cast<HTMLFormAccessible*>(this)->Name(name);
  return name.IsEmpty() ? roles::FORM : roles::FORM_LANDMARK;
}

nsAtom* HTMLFormAccessible::LandmarkRole() const {
  if (!HasOwnContent()) {
    return nullptr;
  }

  // Only return xml-roles "form" if the form has an accessible name.
  nsAutoString name;
  const_cast<HTMLFormAccessible*>(this)->Name(name);
  return name.IsEmpty() ? nullptr : nsGkAtoms::form;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLRadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

uint64_t HTMLRadioButtonAccessible::NativeState() const {
  uint64_t state = AccessibleWrap::NativeState();

  state |= states::CHECKABLE;

  HTMLInputElement* input = HTMLInputElement::FromNode(mContent);
  if (input && input->Checked()) state |= states::CHECKED;

  return state;
}

void HTMLRadioButtonAccessible::GetPositionAndSizeInternal(int32_t* aPosInSet,
                                                           int32_t* aSetSize) {
  int32_t namespaceId = mContent->NodeInfo()->NamespaceID();
  nsAutoString tagName;
  mContent->NodeInfo()->GetName(tagName);

  nsAutoString type;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
  nsAutoString name;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::name, name);

  RefPtr<nsContentList> inputElms;

  nsCOMPtr<nsIFormControl> formControlNode(do_QueryInterface(mContent));
  dom::Element* formElm = formControlNode->GetFormElement();
  if (formElm)
    inputElms = NS_GetContentList(formElm, namespaceId, tagName);
  else
    inputElms = NS_GetContentList(mContent->OwnerDoc(), namespaceId, tagName);
  NS_ENSURE_TRUE_VOID(inputElms);

  uint32_t inputCount = inputElms->Length(false);

  // Compute posinset and setsize.
  int32_t indexOf = 0;
  int32_t count = 0;

  for (uint32_t index = 0; index < inputCount; index++) {
    nsIContent* inputElm = inputElms->Item(index, false);
    if (inputElm->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                           type, eCaseMatters) &&
        inputElm->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::name,
                                           name, eCaseMatters) &&
        mDoc->HasAccessible(inputElm)) {
      count++;
      if (inputElm == mContent) indexOf = count;
    }
  }

  *aPosInSet = indexOf;
  *aSetSize = count;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLButtonAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLButtonAccessible::HTMLButtonAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mGenericTypes |= eButton;
}

uint8_t HTMLButtonAccessible::ActionCount() const { return 1; }

void HTMLButtonAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) aName.AssignLiteral("press");
}

bool HTMLButtonAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) return false;

  DoCommand();
  return true;
}

uint64_t HTMLButtonAccessible::State() {
  uint64_t state = HyperTextAccessibleWrap::State();
  if (state == states::DEFUNCT) return state;

  // Inherit states from input@type="file" suitable for the button. Note,
  // no special processing for unavailable state since inheritance is supplied
  // other code paths.
  if (mParent && mParent->IsHTMLFileInput()) {
    uint64_t parentState = mParent->State();
    state |= parentState & (states::BUSY | states::REQUIRED | states::HASPOPUP |
                            states::INVALID);
  }

  return state;
}

uint64_t HTMLButtonAccessible::NativeState() const {
  uint64_t state = HyperTextAccessibleWrap::NativeState();

  EventStates elmState = mContent->AsElement()->State();
  if (elmState.HasState(NS_EVENT_STATE_DEFAULT)) state |= states::DEFAULT;

  return state;
}

role HTMLButtonAccessible::NativeRole() const { return roles::PUSHBUTTON; }

ENameValueFlag HTMLButtonAccessible::NativeName(nsString& aName) const {
  // No need to check @value attribute for buttons since this attribute results
  // in native anonymous text node and the name is calculated from subtree.
  // The same magic works for @alt and @value attributes in case of type="image"
  // element that has no valid @src (note if input@type="image" has an image
  // then neither @alt nor @value attributes are used to generate a visual label
  // and thus we need to obtain the accessible name directly from attribute
  // value). Also the same algorithm works in case of default labels for
  // type="submit"/"reset"/"image" elements.

  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty() || !mContent->IsHTMLElement(nsGkAtoms::input) ||
      !mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                          nsGkAtoms::image, eCaseMatters))
    return nameFlag;

  if (!mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::alt, aName))
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value, aName);

  aName.CompressWhitespace();
  return eNameOK;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLButtonAccessible: Widgets

bool HTMLButtonAccessible::IsWidget() const { return true; }

////////////////////////////////////////////////////////////////////////////////
// HTMLTextFieldAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLTextFieldAccessible::HTMLTextFieldAccessible(nsIContent* aContent,
                                                 DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mType = mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                             nsGkAtoms::password, eIgnoreCase)
              ? eHTMLTextPasswordFieldType
              : eHTMLTextFieldType;
}

role HTMLTextFieldAccessible::NativeRole() const {
  if (mType == eHTMLTextPasswordFieldType) {
    return roles::PASSWORD_TEXT;
  }

  return roles::ENTRY;
}

already_AddRefed<nsIPersistentProperties>
HTMLTextFieldAccessible::NativeAttributes() {
  nsCOMPtr<nsIPersistentProperties> attributes =
      HyperTextAccessibleWrap::NativeAttributes();

  // Expose type for text input elements as it gives some useful context,
  // especially for mobile.
  nsAutoString type;
  // In the case of this element being part of a binding, the binding's
  // parent's type should have precedence. For example an input[type=number]
  // has an embedded anonymous input[type=text] (along with spinner buttons).
  // In that case, we would want to take the input type from the parent
  // and not the anonymous content.
  nsIContent* widgetElm = BindingOrWidgetParent();
  if ((widgetElm && widgetElm->AsElement()->GetAttr(kNameSpaceID_None,
                                                    nsGkAtoms::type, type)) ||
      mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::type,
                                     type)) {
    nsAccUtils::SetAccAttr(attributes, nsGkAtoms::textInputType, type);
    if (!ARIARoleMap() && type.EqualsLiteral("search")) {
      nsAccUtils::SetAccAttr(attributes, nsGkAtoms::xmlroles, u"searchbox"_ns);
    }
  }

  // If this element  has the placeholder attribute set,
  // and if that is not identical to the name, expose it as an object attribute.
  nsAutoString placeholderText;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                                     placeholderText)) {
    nsAutoString name;
    const_cast<HTMLTextFieldAccessible*>(this)->Name(name);
    if (!name.Equals(placeholderText)) {
      nsAccUtils::SetAccAttr(attributes, nsGkAtoms::placeholder,
                             placeholderText);
    }
  }

  return attributes.forget();
}

ENameValueFlag HTMLTextFieldAccessible::NativeName(nsString& aName) const {
  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  // If part of compound of XUL widget then grab a name from XUL widget element.
  nsIContent* widgetElm = BindingOrWidgetParent();
  if (widgetElm) XULElmName(mDoc, widgetElm, aName);

  if (!aName.IsEmpty()) return eNameOK;

  // text inputs and textareas might have useful placeholder text
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::placeholder,
                                 aName);
  return eNameOK;
}

void HTMLTextFieldAccessible::Value(nsString& aValue) const {
  aValue.Truncate();
  if (NativeState() & states::PROTECTED)  // Don't return password text!
    return;

  HTMLTextAreaElement* textArea = HTMLTextAreaElement::FromNode(mContent);
  if (textArea) {
    textArea->GetValue(aValue);
    return;
  }

  HTMLInputElement* input = HTMLInputElement::FromNode(mContent);
  if (input) {
    // Pass NonSystem as the caller type, to be safe.  We don't expect to have a
    // file input here.
    input->GetValue(aValue, CallerType::NonSystem);
  }
}

void HTMLTextFieldAccessible::ApplyARIAState(uint64_t* aState) const {
  HyperTextAccessibleWrap::ApplyARIAState(aState);
  aria::MapToState(aria::eARIAAutoComplete, mContent->AsElement(), aState);

  // If part of compound of XUL widget then pick up ARIA stuff from XUL widget
  // element.
  nsIContent* widgetElm = BindingOrWidgetParent();
  if (widgetElm)
    aria::MapToState(aria::eARIAAutoComplete, widgetElm->AsElement(), aState);
}

uint64_t HTMLTextFieldAccessible::NativeState() const {
  uint64_t state = HyperTextAccessibleWrap::NativeState();

  // Text fields are always editable, even if they are also read only or
  // disabled.
  state |= states::EDITABLE;

  // can be focusable, focused, protected. readonly, unavailable, selected
  if (mContent->AsElement()->AttrValueIs(kNameSpaceID_None, nsGkAtoms::type,
                                         nsGkAtoms::password, eIgnoreCase)) {
    state |= states::PROTECTED;
  }

  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::readonly)) {
    state |= states::READONLY;
  }

  // Is it an <input> or a <textarea> ?
  HTMLInputElement* input = HTMLInputElement::FromNode(mContent);
  state |= input && input->IsSingleLineTextControl() ? states::SINGLE_LINE
                                                     : states::MULTI_LINE;

  if (state & (states::PROTECTED | states::MULTI_LINE | states::READONLY |
               states::UNAVAILABLE))
    return state;

  // Expose autocomplete states if this input is part of autocomplete widget.
  Accessible* widget = ContainerWidget();
  if (widget && widget - IsAutoComplete()) {
    state |= states::HASPOPUP | states::SUPPORTS_AUTOCOMPLETION;
    return state;
  }

  // Expose autocomplete state if it has associated autocomplete list.
  if (mContent->AsElement()->HasAttr(kNameSpaceID_None, nsGkAtoms::list_))
    return state | states::SUPPORTS_AUTOCOMPLETION | states::HASPOPUP;

  // Ordinal XUL textboxes don't support autocomplete.
  if (!BindingOrWidgetParent() &&
      Preferences::GetBool("browser.formfill.enable")) {
    // Check to see if autocompletion is allowed on this input. We don't expose
    // it for password fields even though the entire password can be remembered
    // for a page if the user asks it to be. However, the kind of autocomplete
    // we're talking here is based on what the user types, where a popup of
    // possible choices comes up.
    nsAutoString autocomplete;
    mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete,
                                   autocomplete);

    if (!autocomplete.LowerCaseEqualsLiteral("off")) {
      Element* formElement = input->GetFormElement();
      if (formElement) {
        formElement->GetAttr(kNameSpaceID_None, nsGkAtoms::autocomplete,
                             autocomplete);
      }

      if (!formElement || !autocomplete.LowerCaseEqualsLiteral("off"))
        state |= states::SUPPORTS_AUTOCOMPLETION;
    }
  }

  return state;
}

uint8_t HTMLTextFieldAccessible::ActionCount() const { return 1; }

void HTMLTextFieldAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) aName.AssignLiteral("activate");
}

bool HTMLTextFieldAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != 0) return false;

  TakeFocus();
  return true;
}

already_AddRefed<TextEditor> HTMLTextFieldAccessible::GetEditor() const {
  RefPtr<TextControlElement> textControlElement =
      TextControlElement::FromNodeOrNull(mContent);
  if (!textControlElement) {
    return nullptr;
  }
  RefPtr<TextEditor> textEditor = textControlElement->GetTextEditor();
  return textEditor.forget();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLTextFieldAccessible: Widgets

bool HTMLTextFieldAccessible::IsWidget() const { return true; }

Accessible* HTMLTextFieldAccessible::ContainerWidget() const {
  if (!mParent || mParent->Role() != roles::AUTOCOMPLETE) {
    return nullptr;
  }
  return mParent;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLFileInputAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLFileInputAccessible::HTMLFileInputAccessible(nsIContent* aContent,
                                                 DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {
  mType = eHTMLFileInputType;
}

role HTMLFileInputAccessible::NativeRole() const {
  // JAWS wants a text container, others don't mind. No specific role in
  // AT APIs.
  return roles::TEXT_CONTAINER;
}

nsresult HTMLFileInputAccessible::HandleAccEvent(AccEvent* aEvent) {
  nsresult rv = HyperTextAccessibleWrap::HandleAccEvent(aEvent);
  NS_ENSURE_SUCCESS(rv, rv);

  // Redirect state change events for inherited states to child controls. Note,
  // unavailable state is not redirected. That's a standard for unavailable
  // state handling.
  AccStateChangeEvent* event = downcast_accEvent(aEvent);
  if (event && (event->GetState() == states::BUSY ||
                event->GetState() == states::REQUIRED ||
                event->GetState() == states::HASPOPUP ||
                event->GetState() == states::INVALID)) {
    Accessible* button = GetChildAt(0);
    if (button && button->Role() == roles::PUSHBUTTON) {
      RefPtr<AccStateChangeEvent> childEvent = new AccStateChangeEvent(
          button, event->GetState(), event->IsStateEnabled(),
          event->FromUserInput());
      nsEventShell::FireEvent(childEvent);
    }
  }

  return NS_OK;
}

Accessible* HTMLFileInputAccessible::CurrentItem() const {
  // Allow aria-activedescendant to override.
  if (Accessible* item = HyperTextAccessibleWrap::CurrentItem()) {
    return item;
  }

  // The HTML file input itself gets DOM focus, not the button inside it.
  // For a11y, we want the button to get focus.
  Accessible* button = FirstChild();
  if (!button) {
    MOZ_ASSERT_UNREACHABLE("File input doesn't contain a button");
    return nullptr;
  }
  MOZ_ASSERT(button->IsButton());
  return button;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLSpinnerAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLSpinnerAccessible::NativeRole() const { return roles::SPINBUTTON; }

void HTMLSpinnerAccessible::Value(nsString& aValue) const {
  HTMLTextFieldAccessible::Value(aValue);
  if (!aValue.IsEmpty()) return;

  // Pass NonSystem as the caller type, to be safe.  We don't expect to have a
  // file input here.
  HTMLInputElement::FromNode(mContent)->GetValue(aValue, CallerType::NonSystem);
}

double HTMLSpinnerAccessible::MaxValue() const {
  double value = HTMLTextFieldAccessible::MaxValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetMaximum().toDouble();
}

double HTMLSpinnerAccessible::MinValue() const {
  double value = HTMLTextFieldAccessible::MinValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetMinimum().toDouble();
}

double HTMLSpinnerAccessible::Step() const {
  double value = HTMLTextFieldAccessible::Step();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetStep().toDouble();
}

double HTMLSpinnerAccessible::CurValue() const {
  double value = HTMLTextFieldAccessible::CurValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetValueAsDecimal().toDouble();
}

bool HTMLSpinnerAccessible::SetCurValue(double aValue) {
  ErrorResult er;
  HTMLInputElement::FromNode(mContent)->SetValueAsNumber(aValue, er);
  return !er.Failed();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLRangeAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLRangeAccessible::NativeRole() const { return roles::SLIDER; }

bool HTMLRangeAccessible::IsWidget() const { return true; }

void HTMLRangeAccessible::Value(nsString& aValue) const {
  LeafAccessible::Value(aValue);
  if (!aValue.IsEmpty()) return;

  // Pass NonSystem as the caller type, to be safe.  We don't expect to have a
  // file input here.
  HTMLInputElement::FromNode(mContent)->GetValue(aValue, CallerType::NonSystem);
}

double HTMLRangeAccessible::MaxValue() const {
  double value = LeafAccessible::MaxValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetMaximum().toDouble();
}

double HTMLRangeAccessible::MinValue() const {
  double value = LeafAccessible::MinValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetMinimum().toDouble();
}

double HTMLRangeAccessible::Step() const {
  double value = LeafAccessible::Step();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetStep().toDouble();
}

double HTMLRangeAccessible::CurValue() const {
  double value = LeafAccessible::CurValue();
  if (!IsNaN(value)) return value;

  return HTMLInputElement::FromNode(mContent)->GetValueAsDecimal().toDouble();
}

bool HTMLRangeAccessible::SetCurValue(double aValue) {
  ErrorResult er;
  HTMLInputElement::FromNode(mContent)->SetValueAsNumber(aValue, er);
  return !er.Failed();
}

////////////////////////////////////////////////////////////////////////////////
// HTMLGroupboxAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLGroupboxAccessible::HTMLGroupboxAccessible(nsIContent* aContent,
                                               DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {}

role HTMLGroupboxAccessible::NativeRole() const { return roles::GROUPING; }

nsIContent* HTMLGroupboxAccessible::GetLegend() const {
  for (nsIContent* legendContent = mContent->GetFirstChild(); legendContent;
       legendContent = legendContent->GetNextSibling()) {
    if (legendContent->NodeInfo()->Equals(nsGkAtoms::legend,
                                          mContent->GetNameSpaceID())) {
      // Either XHTML namespace or no namespace
      return legendContent;
    }
  }

  return nullptr;
}

ENameValueFlag HTMLGroupboxAccessible::NativeName(nsString& aName) const {
  ENameValueFlag nameFlag = Accessible::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  nsIContent* legendContent = GetLegend();
  if (legendContent)
    nsTextEquivUtils::AppendTextEquivFromContent(this, legendContent, &aName);

  return eNameOK;
}

Relation HTMLGroupboxAccessible::RelationByType(RelationType aType) const {
  Relation rel = HyperTextAccessibleWrap::RelationByType(aType);
  // No override for label, so use <legend> for this <fieldset>
  if (aType == RelationType::LABELLED_BY) rel.AppendTarget(mDoc, GetLegend());

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLLegendAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLLegendAccessible::HTMLLegendAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {}

Relation HTMLLegendAccessible::RelationByType(RelationType aType) const {
  Relation rel = HyperTextAccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABEL_FOR) return rel;

  Accessible* groupbox = Parent();
  if (groupbox && groupbox->Role() == roles::GROUPING)
    rel.AppendTarget(groupbox);

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLFigureAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLFigureAccessible::HTMLFigureAccessible(nsIContent* aContent,
                                           DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {}

ENameValueFlag HTMLFigureAccessible::NativeName(nsString& aName) const {
  ENameValueFlag nameFlag = HyperTextAccessibleWrap::NativeName(aName);
  if (!aName.IsEmpty()) return nameFlag;

  nsIContent* captionContent = Caption();
  if (captionContent)
    nsTextEquivUtils::AppendTextEquivFromContent(this, captionContent, &aName);

  return eNameOK;
}

Relation HTMLFigureAccessible::RelationByType(RelationType aType) const {
  Relation rel = HyperTextAccessibleWrap::RelationByType(aType);
  if (aType == RelationType::LABELLED_BY) rel.AppendTarget(mDoc, Caption());

  return rel;
}

nsIContent* HTMLFigureAccessible::Caption() const {
  for (nsIContent* childContent = mContent->GetFirstChild(); childContent;
       childContent = childContent->GetNextSibling()) {
    if (childContent->NodeInfo()->Equals(nsGkAtoms::figcaption,
                                         mContent->GetNameSpaceID())) {
      return childContent;
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLFigcaptionAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLFigcaptionAccessible::HTMLFigcaptionAccessible(nsIContent* aContent,
                                                   DocAccessible* aDoc)
    : HyperTextAccessibleWrap(aContent, aDoc) {}

Relation HTMLFigcaptionAccessible::RelationByType(RelationType aType) const {
  Relation rel = HyperTextAccessibleWrap::RelationByType(aType);
  if (aType != RelationType::LABEL_FOR) return rel;

  Accessible* figure = Parent();
  if (figure && figure->GetContent()->NodeInfo()->Equals(
                    nsGkAtoms::figure, mContent->GetNameSpaceID())) {
    rel.AppendTarget(figure);
  }

  return rel;
}

////////////////////////////////////////////////////////////////////////////////
// HTMLProgressAccessible
////////////////////////////////////////////////////////////////////////////////

role HTMLProgressAccessible::NativeRole() const { return roles::PROGRESSBAR; }

uint64_t HTMLProgressAccessible::NativeState() const {
  uint64_t state = LeafAccessible::NativeState();

  // An undetermined progressbar (i.e. without a value) has a mixed state.
  nsAutoString attrValue;
  mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                                 attrValue);
  if (attrValue.IsEmpty()) {
    state |= states::MIXED;
  }

  return state;
}

bool HTMLProgressAccessible::IsWidget() const { return true; }

void HTMLProgressAccessible::Value(nsString& aValue) const {
  LeafAccessible::Value(aValue);
  if (!aValue.IsEmpty()) {
    return;
  }

  double maxValue = MaxValue();
  if (IsNaN(maxValue) || maxValue == 0) {
    return;
  }

  double curValue = CurValue();
  if (IsNaN(curValue)) {
    return;
  }

  // Treat the current value bigger than maximum as 100%.
  double percentValue =
      (curValue < maxValue) ? (curValue / maxValue) * 100 : 100;

  aValue.AppendFloat(percentValue);
  aValue.Append('%');
}

double HTMLProgressAccessible::MaxValue() const {
  double value = LeafAccessible::MaxValue();
  if (!IsNaN(value)) {
    return value;
  }

  nsAutoString strValue;
  if (mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::max,
                                     strValue)) {
    nsresult result = NS_OK;
    value = strValue.ToDouble(&result);
    if (NS_SUCCEEDED(result)) {
      return value;
    }
  }

  return 1;
}

double HTMLProgressAccessible::MinValue() const {
  double value = LeafAccessible::MinValue();
  return IsNaN(value) ? 0 : value;
}

double HTMLProgressAccessible::Step() const {
  double value = LeafAccessible::Step();
  return IsNaN(value) ? 0 : value;
}

double HTMLProgressAccessible::CurValue() const {
  double value = LeafAccessible::CurValue();
  if (!IsNaN(value)) {
    return value;
  }

  nsAutoString attrValue;
  if (!mContent->AsElement()->GetAttr(kNameSpaceID_None, nsGkAtoms::value,
                                      attrValue)) {
    return UnspecifiedNaN<double>();
  }

  nsresult error = NS_OK;
  value = attrValue.ToDouble(&error);
  return NS_FAILED(error) ? UnspecifiedNaN<double>() : value;
}

bool HTMLProgressAccessible::SetCurValue(double aValue) {
  return false;  // progress meters are readonly.
}
