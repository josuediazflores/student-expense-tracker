# SCU Student Budget MVP

A native Qt Widgets and C++ walking skeleton for CSCI 187, built on the [course project template](https://github.com/jadekandel/CSCI-187-Template). Students can enter a budget amount, date range, and spending category, then view the combined total and the daily amount for each budget.

## Requirements covered

- Set an initial amount, start date, end date, and category for each budget.
- View the combined budget total and each budget's amount per day.
- View categories in the budget list.
- Reject negative amounts and end dates before start dates in the model.
- Record expense amounts in the model and calculate total spending and remaining budget.

The daily amount includes both the start and end dates. Budgets and spending totals are stored in memory for the current app session. The GUI currently provides budget entry; expense entry is available through the model API. Expense-entry screens, meal-plan balance imports, persistence, and charts are later increments.

## Build and run

Install CMake 3.16 or later, a C++17 compiler, and Qt 6 with the Widgets and SQL modules. The template's CMake file requires Qt 6. On macOS with Homebrew, `brew install cmake qt` installs the build tools.

```sh
cmake -B build
cmake --build build
```

Run `./build/app` on macOS or Linux. On Windows, run `app.exe` from the generated build configuration directory. GitHub Actions builds the app and runs unit tests on pushes and pull requests.

## Run the unit tests

After building, run:

```sh
cd build
ctest --output-on-failure
```

`tests/test_expense_totals.cpp` contains Xiaochao Zhou's test. It starts with a $100.00 budget, adds expenses of $12.50, $23.75, and $5.99, and checks spending and remaining budget after each addition. The final totals are $42.24 spent and $57.76 remaining. Overspending produces a negative remaining balance.

`tests/test_expense_invalid_inputs.cpp` contains independent cases for missing amounts (`nullptr`), text, negative numbers, nonfinite numbers, boolean values, and amounts that would overflow. It also checks that integer zero remains valid. Rejected inputs must provide an error and leave total spending, remaining budget, and the allocated budget unchanged.

`tests/test_expense_precision.cpp` contains separate cases for preserving fractional cents, exact decimal accumulation, tiny positive expenses, negative fractional remaining balances, and the supported numeric limit. `tests/test_decimal_cents.cpp` verifies the underlying exact decimal arithmetic.

The model's `addExpense()` API takes amounts in **cents**, including fractional cents. For example, $12.50 is passed as `1250`, and $12.345 is passed as `1234.5`. Fractional cents are retained without rounding or truncation. Adding `1250.5` and `2375.5` cents produces exactly `3626` cents. Small positive amounts are retained, and overspending produces a negative fractional remaining balance.

Spending and remaining balances use `DecimalCents`, an exact decimal value backed by decimal digits. Read its canonical cents representation with `.toString()`; these getters no longer return `qint64`, because that would discard fractional cents. Existing whole-cent comparisons remain valid. Budget allocations stay in integer cents. Spending remains limited to the exact value of `qint64::max()` cents, including its fractional boundary.

Each floating input is interpreted using its original type's shortest round-trip decimal representation, then all addition and subtraction use exact decimal arithmetic. Two separate expenses of `0.1` and `0.2` cents total exactly `0.3` cents. An expression already evaluated by the caller, such as `0.1 + 0.2`, arrives as `0.30000000000000004` and is preserved; the model does not round that input. Integral floating values near the numeric limit are converted through their exact integer value. For input beyond native floating precision, explicitly parse a decimal with `DecimalCents::fromDecimal()` and pass that value to `addExpense()`.

Text (including numeric-looking strings) must be explicitly parsed by the input layer before calling the model. The model rejects text, `nullptr`, booleans, negative amounts, NaN, infinity, and values outside the supported integer-cent range. Integral input types wider than 64 bits are unsupported and rejected before conversion. Integer `0` and floating-point `0.0` are valid amounts and are distinct from missing input.

Tests use the vendored doctest header. Each new test file should include `doctest.h`; only `tests/test_main.cpp` defines the test runner.

## Project structure

- `src/BudgetModel.*`: stores budgets, validates rules, and calculates totals. It has no dependency on the view or controller.
- `src/DecimalCents.*`: preserves fractional cents with exact decimal arithmetic.
- `src/BudgetController.*`: receives requests from the view and mediates access to the model.
- `src/BudgetView.*`: Qt Widgets form and budget table.
