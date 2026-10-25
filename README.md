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

## Run the unit test

After building, run:

```sh
cd build
ctest --output-on-failure
```

`tests/test_expense_totals.cpp` contains Xiaochao Zhou's test. It starts with a $100.00 budget, adds expenses of $12.50, $23.75, and $5.99, and checks spending and remaining budget after each addition. The final totals are $42.24 spent and $57.76 remaining. Amounts use integer cents; overspending produces a negative remaining balance.

Tests use the vendored doctest header. Each new test file should include `doctest.h`; only `tests/test_main.cpp` defines the test runner.

## Project structure

- `src/BudgetModel.*`: stores budgets, validates rules, and calculates totals. It has no dependency on the view or controller.
- `src/BudgetController.*`: receives requests from the view and mediates access to the model.
- `src/BudgetView.*`: Qt Widgets form and budget table.
