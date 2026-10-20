# SCU Student Budget MVP

A native Qt Widgets and C++ walking skeleton for CSCI 187, built on the [course project template](https://github.com/jadekandel/CSCI-187-Template). Students can enter a budget amount, date range, and spending category, then view the combined total and the daily amount for each budget.

## Requirements covered

- Set an initial amount, start date, end date, and category for each budget.
- View the combined budget total and each budget's amount per day.
- View categories in the budget list.
- Reject negative amounts and end dates before start dates in the model.

The daily amount includes both the start and end dates. Budgets are stored in memory for the current app session. Expense tracking, meal-plan balance imports, persistence, and charts are later increments.

## Build and run

Install CMake 3.16 or later, a C++17 compiler, and Qt 6 with the Widgets and SQL modules. The template's CMake file requires Qt 6. On macOS with Homebrew, `brew install cmake qt` installs the build tools.

```sh
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run `./build/app` on macOS or Linux. On Windows, run `app.exe` from the generated build configuration directory. GitHub Actions runs the same build and test commands on pushes and pull requests.

## Project structure

- `src/BudgetModel.*`: stores budgets, validates rules, and calculates totals. It has no dependency on the view or controller.
- `src/BudgetController.*`: receives requests from the view and mediates access to the model.
- `src/BudgetView.*`: Qt Widgets form and budget table.
- `tests/test_budget.cpp`: doctest unit tests for model behavior, using the template's `test_main.cpp` and `doctest.h`.

The template's example prime-number files were replaced with the budgeting model. Its CMake source discovery, target names, test runner, GitHub Actions workflow, and repository settings are retained.
