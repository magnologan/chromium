// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/actions/action.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

namespace autofill_assistant {
class ClientStatus;

// An action to autofill a form using a local address or credit card.
class AutofillAction : public Action {
 public:
  explicit AutofillAction(ActionDelegate* delegate, const ActionProto& proto);
  ~AutofillAction() override;

 private:
  enum FieldValueStatus { UNKNOWN, EMPTY, NOT_EMPTY };

  // Overrides Action:
  void InternalProcessAction(ProcessActionCallback callback) override;

  void EndAction(ProcessedActionStatusProto status);
  void EndAction(const ClientStatus& status);

  // Fill the form using data in client memory. Return whether filling succeeded
  // or not through OnAddressFormFilled or OnCardFormFilled.
  void FillFormWithData();
  void OnWaitForElement(bool element_found);

  // Called after getting full credit card with its cvc.
  void OnGetFullCard(std::unique_ptr<autofill::CreditCard> card,
                     const base::string16& cvc);

  // Called when the credit card form has been filled.
  void OnCardFormFilled(const ClientStatus& status);

  // Called when the address form has been filled.
  void OnAddressFormFilled(const ClientStatus& status);

  // Check whether all required fields have a non-empty value. If it is the
  // case, finish the action successfully. If it's not and |allow_fallback|
  // false, fail the action. If |allow_fallback| is true, try again by filling
  // the failed fields without Autofill.
  void CheckRequiredFields(bool allow_fallback);

  // Triggers the check for a specific field.
  void CheckRequiredFieldsSequentially(bool allow_fallback,
                                       int required_fields_index);

  // Updates |required_fields_value_status_|.
  void OnGetRequiredFieldValue(int required_fields_index,
                               bool exists,
                               const std::string& value);

  // Called when all required fields have been checked.
  void OnCheckRequiredFieldsDone(bool allow_fallback);

  // Get the value of |address_field| associated to profile |profile|. Return
  // empty string if there is no data available.
  base::string16 GetAddressFieldValue(
      const autofill::AutofillProfile* profile,
      const UseAddressProto::RequiredField::AddressField& address_field);

  // Sets fallback field values for empty fields from
  // |required_fields_value_status_|.
  void SetFallbackFieldValuesSequentially(int required_fields_index);

  // Called after trying to set form values without Autofill in case of fallback
  // after failed validation.
  void OnSetFallbackFieldValue(int required_fields_index,
                               const ClientStatus& status);

  // Usage of the autofilled address. Ignored if autofilling a card.
  std::string name_;
  std::string prompt_;
  Selector selector_;

  // True if autofilling a card, otherwise we are autofilling an address.
  bool is_autofill_card_;
  std::vector<FieldValueStatus> required_fields_value_status_;

  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  ProcessActionCallback process_action_callback_;
  base::WeakPtrFactory<AutofillAction> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillAction);
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ACTIONS_AUTOFILL_ACTION_H_
