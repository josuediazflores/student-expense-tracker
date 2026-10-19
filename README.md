# SCU Student Budget MVP

A native Qt Widgets and C++ walking skeleton for CSCI 187. Students can add a budget with an amount, date range, and spending category, then see the total budget and the daily amount for each category.

## Requirements covered

- Set an initial amount, start date, end date, and category for each budget.
- View the combined budget total and each budget's amount per day.
- View categories in the budget list.
- Reject negative amounts and end dates before start dates in the model.

The daily amount divides the budget by the number of calendar days, including both start and end dates. Budgets exist in memory for the current app session. Expense tracking, meal-plan balance imports, persistence, and charts are later increments.

## Build and run

Requires CMake 3.21 or later, a C++17 compiler, and Qt 6 with Widgets and Test.

```sh
cmake -S . -B build
cmake --build build
./build/scu_budget
```

On Windows, run the generated `scu_budget.exe` from the appropriate build configuration directory.

## Test

```sh
ctest --test-dir build --output-on-failure
```

GitHub Actions builds and tests on pushes and pull requests after this project is placed in a GitHub repository.

## MVC layout

- `BudgetModel`: stores budgets, validates business rules, computes totals and daily amounts. It does not know the view or controller.
- `BudgetController`: receives actions from the view and mediates access to the model.
- `BudgetView`: Qt Widgets input and display. It only reaches the model through the controller.
