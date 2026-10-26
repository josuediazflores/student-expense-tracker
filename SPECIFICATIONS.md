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
| Controller | `src/BudgetController.*` | Receives requests from the view, converts them into model calls, and returns results and error messages. |
| View | `src/BudgetView.*` | Qt Widgets form and table. It shows data and passes user input to the controller. It holds no business rules. |
| Entry point | `src/main.cpp` | Creates the model, controller, and view, then starts the Qt event loop. |

The test executable links only `BudgetModel.cpp` and Qt Core, which keeps unit tests fast and independent of the GUI.

## Data model

### Money

Budget amounts are stored as whole cents in a signed 64-bit integer (`qint64`). For example, $12.50 is stored as `1250`. Using integer cents avoids floating-point rounding errors in totals.

### Budget

| Field | Type | Rule |
| --- | --- | --- |
| `category` | `QString` | Required. Surrounding spaces are removed before saving. |
| `amountCents` | `qint64` | Zero or greater. |
| `startDate` | `QDate` | Required and valid. |
| `endDate` | `QDate` | Required, valid, and on or after `startDate`. |

### Expense

The skeleton records each expense as an amount added to a running spending total. **Planned:** store each expense as a record with an amount and a date so the app can list entries.

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
- Remaining budget is total budget minus total spending. It can be negative when the user overspends.

### Adding an expense

`BudgetModel::addExpense(amount, error)` adds one expense to total spending.

- Zero is a valid amount.
- The model rejects negative amounts, missing values (`nullptr`), malformed input, and amounts that would overflow the spending total.
- A rejected expense sets `error` and leaves every total unchanged.

The exact accepted input format and how fractional cents are handled are documented in the README, next to the tests that check them.

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
| `test_expense_precision.cpp` | Xiaochao Zhou | Fractional-cent handling and numeric limits |
| `test_budget_per_day.cpp` | Huy Tran | Daily amount calculation across single days, month boundaries, leap years, and a quarter |
| `test_budget_rules.cpp` | Huy Tran | Budget validation rules, same-day budgets, category trimming, overflow limits, and error reporting |
