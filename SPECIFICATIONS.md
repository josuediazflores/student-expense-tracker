# Specifications

This document describes how SCU Student Budget meets the requirements in [REQUIREMENTS.md](REQUIREMENTS.md). Sections marked **Planned** describe MVP work that the walking skeleton does not include yet.

## Platform

- Language: C++17
- User interface: Qt 6 Widgets
- Storage: Qt SQL with SQLite (**Planned**; the skeleton keeps data in memory)
- Build: CMake 3.16 or later
- Tests: doctest, run with CTest
- Continuous integration: GitHub Actions builds the app and runs every test on each push and pull request

## Architecture

The app follows the model-view-controller pattern.

| Component | File | Responsibility |
| --- | --- | --- |
| Model | `src/BudgetModel.*` | Stores budgets and expenses, enforces validation rules, and calculates totals. It does not depend on Qt Widgets, the view, or the controller, so it can be unit tested on its own. |
| Exact money | `src/DecimalCents.*` | An exact decimal number of cents used for spending and remaining balances. It adds, subtracts, and compares decimal digits directly, so fractional cents are never rounded or lost to floating-point error. |
| Controller | `src/BudgetController.*` | Receives requests from the view, converts them into model calls, and returns results and error messages. |
| View | `src/BudgetView.*` | Qt Widgets form and table. It shows data and passes user input to the controller. It holds no business rules. |
| Entry point | `src/main.cpp` | Creates the model, controller, and view, then starts the Qt event loop. |

The test executable links only `BudgetModel.cpp`, `DecimalCents.cpp`, and Qt Core, which keeps unit tests fast and independent of the GUI.

## Data model

### Money

Budget amounts are stored as whole cents in a signed 64-bit integer (`qint64`). For example, $12.50 is stored as `1250`.

Total spending and remaining budget are stored as `DecimalCents`, which keeps fractional cents exactly. For example, $12.345 is stored as `1234.5` cents. `DecimalCents::toString()` returns the value in cents as text, and a `DecimalCents` can be compared directly with a whole number of cents.

Neither type uses floating-point numbers, so totals never pick up rounding errors.

### Budget

| Field | Type | Rule |
| --- | --- | --- |
| `category` | `QString` | Required. Surrounding spaces are removed before saving. |
| `amountCents` | `qint64` | Zero or greater. |
| `startDate` | `QDate` | Required and valid. |
| `endDate` | `QDate` | Required, valid, and on or after `startDate`. |

### Expense

The skeleton records each expense as an exact amount added to a running `DecimalCents` spending total. **Planned:** store each expense as a record with an amount and a date so the app can list entries.

## Business rules

### Adding a budget

`BudgetModel::addBudget(budget, error)` returns `true` and saves the budget when every rule passes. Otherwise it returns `false`, sets `error` to a message for the user, and leaves all saved data unchanged. Rules are checked in this order:

1. Amount is negative: "Budget amount cannot be negative."
2. Start or end date is missing or invalid: "Choose valid start and end dates."
3. End date is before start date: "End date cannot be before start date."
4. Category is empty or only spaces: "Choose a category."
5. Adding the amount would overflow the combined total: "Total budget is too large."

On success, `error` is cleared.

### Daily amount

`Budget::amountPerDay()` returns the budget amount in dollars divided by the number of days in its period. The count includes both the start and end dates, so a budget from October 1 to October 31 covers 31 days and a single-day budget keeps its full amount.

### Totals

- Total budget is the sum of every saved budget's amount.
- Total spending is the sum of every accepted expense.
- Remaining budget is total budget minus total spending. It can be negative, including by a fraction of a cent, when the user overspends.

### Adding an expense

`BudgetModel::addExpense(amountDollars, error)` takes the amount as decimal text in dollars, either a `QString` or a C string. For example, pass `"12.99"` for $12.99.

- The model converts the text to cents by shifting decimal digits, so `"12.99"` becomes exactly `1299` cents and `"12.345"` becomes exactly `1234.5` cents. Nothing is rounded.
- Accepted text is ASCII digits with an optional decimal fraction, such as `"12"`, `"12.99"`, or `"0.00001"`. Surrounding spaces are removed first.
- Zero is a valid amount, including `"0"` and `"-0.000"`.
- The model rejects empty text, missing values (`nullptr`), negative amounts, text that is not a plain decimal (such as `"abc"`, `"$5"`, `"1,000"`, `"1e3"`, or `"+5"`), and any total spending above 9,223,372,036,854,775,807 cents.
- Numbers passed directly, such as `addExpense(12.99)` or `addExpense(1299)`, are rejected. The input layer must pass the user's original text so a floating-point conversion cannot change the value first.
- A rejected expense sets `error` and leaves every total unchanged. A successful expense clears `error`.

## User interface

### Current screen

The main window, titled "SCU Student Budget," contains:

- A form with a category drop-down (Meal plan, Groceries, Dining out, Transport, School supplies, Other), an amount field in dollars, and start and end date pickers. The start date defaults to today and the end date to one month later.
- An **Add budget** button. When the model rejects input, its message appears below the button. On success the status reads "Budget added."
- A total budget label.
- A table with the columns Category, Total, Period, and Per day.

### Planned MVP screen

- An editable overall budget amount that recalculates the remaining budget when changed.
- An expense form with an amount field, a date picker that defaults to today, and a **Save expense** button that shows validation messages.
- Labels for total budget, total spending, and remaining budget. A negative remaining budget is shown with a minus sign.
- A table of logged expenses with Date and Amount columns, newest first.

## Persistence (Planned)

The app will save budgets and expenses to a local SQLite database through Qt SQL, load them at startup, and write each change immediately so nothing is lost when the window closes.

## Testing

Every `.cpp` file in `tests/` is compiled into one test executable. Only `tests/test_main.cpp` defines `main()`. Run the tests with:

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

| Test file | Author | Covers |
| --- | --- | --- |
| `test_budget_totals.cpp` | Troy Capela | Several budgets add up to the total budget |
| `test_expense_totals.cpp` | Xiaochao Zhou | Multiple expenses update spending and remaining budget |
| `test_expense_invalid_inputs.cpp` | Xiaochao Zhou | Invalid expense inputs are rejected without changing totals |
| `test_expense_precision.cpp` | Xiaochao Zhou | Exact fractional cents, tiny expenses, negative fractional balances, and numeric limits |
| `test_decimal_cents.cpp` | Xiaochao Zhou | Exact decimal arithmetic: addition, subtraction, comparison, signs, zero, and invalid text |
| `test_budget_per_day.cpp` | Huy Tran | Daily amount calculation across single days, month boundaries, leap years, and a quarter |
| `test_budget_rules.cpp` | Huy Tran | Budget validation rules, same-day budgets, category trimming, overflow limits, and error reporting |
