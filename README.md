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

`tests/test_expense_invalid_inputs.cpp` contains independent cases for missing amounts (`nullptr`), malformed text, negative amounts, unsupported numeric argument types, and amounts that would overflow. Rejected inputs must provide an error and leave total spending, remaining budget, and the allocated budget unchanged.

`tests/test_expense_precision.cpp` contains separate cases for preserving fractional cents, exact decimal accumulation, tiny positive expenses, negative fractional remaining balances, and the supported numeric limit. `tests/test_decimal_cents.cpp` verifies the underlying exact decimal arithmetic.

The user-facing `addExpense()` API takes **decimal text in dollars**, using `QString` or UTF-8 C-string input. Pass `"12.99"` for $12.99; the model converts it exactly to `1299` cents by shifting decimal digits. Pass `"12.345"` for $12.345; it remains `1234.5` cents. Fractional cents are retained without rounding or truncation. Adding `"12.505"` and `"23.755"` produces exactly `3626` cents ($36.26). Small positive amounts are retained, and overspending produces a negative fractional remaining balance.

Spending and remaining balances use `DecimalCents`, an exact decimal value backed by decimal digits. Read its canonical cents representation with `.toString()`; these getters no longer return `qint64`, because that would discard fractional cents. Existing whole-cent comparisons remain valid. Budget allocations stay in integer cents. Spending remains limited to the exact value of `qint64::max()` cents, including its fractional boundary.

Amounts enter the function as text so binary floating-point arithmetic cannot alter a value before validation. The expense path performs only decimal parsing and integer digit operations. `addExpense(12.99)`, integer arguments, and externally constructed `DecimalCents` arguments are rejected; callers must supply the original text rather than first converting it to a number. Numeric-looking text is now valid, while text such as `"abc"` remains invalid. Internal cents insertion is private.

The text format uses ASCII digits with an optional decimal fraction, such as `"12"`, `"12.99"`, or `"0.00001"`. Surrounding whitespace is trimmed; leading zeros and extra fractional digits are retained numerically. Exponents, currency symbols, grouping commas, internal whitespace, a leading plus, and missing integer or fractional digits are rejected. Negative amounts are rejected; textual zero, including `"-0.000"`, is valid. Empty text, `nullptr`, NaN/infinity text, booleans, native numeric values, and totals exceeding exactly `9223372036854775807` cents are rejected without changing balances.

Tests use the vendored doctest header. Each new test file should include `doctest.h`; only `tests/test_main.cpp` defines the test runner.

## Project structure

- `src/BudgetModel.*`: stores budgets, validates rules, and calculates totals. It has no dependency on the view or controller.
- `src/DecimalCents.*`: preserves fractional cents with exact decimal arithmetic.
- `src/BudgetController.*`: receives requests from the view and mediates access to the model.
- `src/BudgetView.*`: Qt Widgets form and budget table.
