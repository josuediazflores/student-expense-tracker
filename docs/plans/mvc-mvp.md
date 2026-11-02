# MVC wiring for the MVP

## Original Request

Received 2026-11-01 (relayed brief that quotes Huy's goal and his course's architecture rules), copied as received:

> You are taking charge of the MVP implementation for Huy's CSCI 187 team project: a Qt6/C++17 Student Expense Tracker at /Users/tranquanghuy/Projects/csci187/student-expense-tracker (CMake, doctest tests in tests/, CI in .github/workflows/ci.yml, shared team repo on GitHub josuediazflores/student-expense-tracker; current branch huy/spec-decimal-cents, main is the base). Course docs are one level up: `DRAFT P3 MVP Requirements.docx`, `P1_ Requirements.docx`, `P0_ Project Proposal.docx` (extract text with e.g. `textutil -convert txt -stdout`), plus REQUIREMENTS.md and SPECIFICATIONS.md in the repo. Read those to determine MVP scope.
>
> Huy's goal (his words): he is doing "the structure, architecture of the model to make sure that it wires data correctly to the view via the controller of the MVC". The architecture rules, which are non-negotiable:
> - "The Model should NOT be dependent on the View or Controller – should not KNOW about them!! You should be able to take the SAME model, and put a different UI on it by switching out the view and controller."
> - "The Model does not call the Controller or the View. It is independently designed."
> - "The View only calls the Controller."
> - "The Controller mediates communication between Model and View, and calls both."
>
> What I already observed (verify, don't trust):
> - Skeleton exists: src/BudgetModel.{h,cpp}, DecimalCents.{h,cpp}, BudgetController.{h,cpp} (7-line .cpp, only createBudget/budgets/totalCents), BudgetView.{h,cpp} (QWidget), main.cpp wires Model→Controller→View. Include direction currently looks correct (Model includes only DecimalCents + QtCore types; Controller includes Model; View includes Controller).
> - Gaps: the Model has addExpense(decimal text), totalSpendingCents, remainingBudgetCents, but the Controller exposes none of it, so the View cannot reach expenses. The View uses a QDoubleSpinBox for amounts and passes qint64 cents — a float path that conflicts with the spec's exact-decimal-text expense rule. The controller currently returns Model types (QVector<Budget>) straight to the View — decide whether that leaks Model into View in a way that violates "View only calls the Controller" (View depending on Model data types vs. calling Model). Also decide how the View learns of updates without the Model knowing about it (e.g. controller returns results / view refreshes after each controller call, or controller-owned callback/signal — NOT Qt signals emitted by the Model to the View).
> - test_build links only BudgetModel.cpp + DecimalCents.cpp with Qt6::Core — good evidence the Model is UI-independent; keep that property and consider adding controller tests that run without Widgets.
>
> Deliverables:
> 1. An honest verdict and a plan doc at docs/plans/mvc-mvp.md: MVP scope from the docs, the layer contract (what each of Model/Controller/View exposes and may include), how the dependency rules are enforced (e.g. a CMake model library target linking only Qt6::Core, and a check/test that the model sources include nothing from Controller/View/Widgets), and tasks. Keep scope to the MVP — no extra features.
> 2. Implement it test-first on a new local feature branch off main (or off the current branch if it carries unmerged work main needs — say which and why). Build and run the tests (cmake + ctest; build/ dir exists) and report real output.
> 3. Commit locally only. Do NOT push, and do NOT open a PR — this is a shared team repo and Huy will approve those himself. Stop and report before any destructive git action.
>
> Huy is less deep on backend/architecture internals: in your final report, explain the why of each architectural decision in plain terms. Report: verdict, plan doc path, branch name, files changed, test/build output summary (with failures stated plainly), any open questions only Huy can answer, and anything you could not do (e.g. if you could not spawn your role agents as a subagent, say so and what you did instead).

### Addendum 2026-11-01: Huy's follow-up request (relayed by the coordinator, Huy's words copied as received)

Context from the coordinator: Huy ran the app and found that adding "Meal plan $250, Nov 1–Dec 1, 2026" twice created two identical rows.

> 1. When the user add a budget for a category that has already has a budget in that time frame, it should not add another one but instead tell the user that they already have set a budget for it and check if the want to add that budget to another category or update the budget of that category (upated this feature and include a test for this)
> 2. The expense being added should have an option to select a budget category

The coordinator's defaults for this request are recorded as decisions D15–D22 under `## Part 3`.

### Addendum 2026-11-01: Huy's second follow-up (relayed by the coordinator, Huy's words copied as received)

Context from the coordinator: Huy is running the Part 3 build. His real file upgraded cleanly to schema v2 and holds two duplicate Meal plan budgets ($50 and $250, Nov 1–Dec 1, 2026) and one $100 expense. Delete is how he will clean those up.

> add the ability to delete expense and delete budget

The coordinator's defaults are recorded as decisions D24–D30 under `## Part 4`.

## Verdict (CTO, Phase 0)

GO WITH CHANGES. The proposal is right: the skeleton's include direction is correct, but the Controller exposes nothing about expenses, the View reaches into Model types (`Budget`, and calls `Budget::amountPerDay()`, a Model method, directly), and money goes through a `double`.

Changes from the literal MVP: persistence (MVP requirement 3, last bullet) is left out of this slice and only its seam is designed (D1), and the skeleton's category budgets are kept rather than collapsed to one overall budget (D2), so teammates' tests stay untouched.

Branch `huy/mvc-mvp` is cut from `origin/main` (ed49eb0), not from `huy/spec-decimal-cents`: that branch only changes SPECIFICATIONS.md (open PR #7), and the code does not need it. Local `main` is one commit behind `origin/main`, so `origin/main` is the base.

> **Superseded in part (2026-11-01, after trial round 1):** persistence is no longer cut. D13 reverses D1, and persistence is built in this branch (see `## Part 2: Persistence slice`). The category-budget change (D2) still stands and is now Open Question 3.

## Goal

A student can open the app, add or update a budget amount typed as dollar text, log an expense with an amount and a date (today by default), and immediately see total budget, total spending, remaining budget (negative shown as `-$…`), and a newest-first list of expenses.

The code follows strict MVC. The Model knows nothing about the controller, view or display interface. The View talks only to the Controller and cannot name Model types. The Controller mediates and pushes view-ready strings through an abstract `BudgetDisplay`. These rules are enforced by CMake target boundaries and by a ctest layering check that runs in CI.

## MVP scope

In (REQUIREMENTS.md "MVP requirements"; SPECIFICATIONS.md "Planned MVP screen"):
- Req 1, create and edit a budget: enter a budget amount as text; update an existing budget's amount; totals and remaining recalculate.
- Req 2, log spending: amount + date (`QDateEdit` defaults to today); clear message for an invalid amount or date; totals update on save.
- Req 3, view: total budget, total spending, remaining (minus sign when negative), and an expenses table (Date, Amount), newest first.

Out:
- ~~Req 3's last bullet, persistence across restarts. It is MVP but a separate slice (D1). Only the seam is described below.~~ Superseded by D13: persistence is now IN scope and built (Part 2).
- A single "overall budget" entity. The skeleton's category budgets stay, and total = sum (D2).
- All "Later requirements": per-category spending, editing or deleting expenses, alerts, charts, dining points.
- SPECIFICATIONS.md edits (open PR #7 owns that file; see Follow-ups). CI workflow changes are not needed.
- `DecimalCents.*` and every existing test file stay untouched.

## Layer contract

### Model: `src/BudgetModel.h/.cpp`, `src/DecimalCents.h/.cpp` (target `budget_model`, Qt6::Core only)

`DecimalCents` is unchanged. `Budget` is unchanged (it stays an aggregate; tests brace-initialise it). Additions to `BudgetModel.h`, shown with the existing members kept verbatim:

```cpp
struct Expense {
    DecimalCents amountCents;   // exact, may be fractional
    QDate date;
};

class BudgetModel {
public:
    // existing, unchanged
    bool addBudget(const Budget &budget, QString *error = nullptr);
    const QVector<Budget> &budgets() const;
    qint64 totalCents() const;
    // new (D2, D4)
    bool updateBudgetAmount(qsizetype index, qint64 amountCents, QString *error = nullptr);
    static std::optional<qint64> parseBudgetAmount(const QString &amountDollars, QString *error = nullptr);

    // existing no-date overloads + template guard: same signatures; now record QDate::currentDate()
    bool addExpense(const QString &amountDollars, QString *error = nullptr);
    bool addExpense(const char *amountDollars, QString *error = nullptr);
    bool addExpense(std::nullptr_t, QString *error = nullptr);
    template <typename Amount, /* existing enable_if, unchanged */ int = 0>
    bool addExpense(const Amount &, QString *error = nullptr);
    // new dated set (D3), mirrors the overload set above
    bool addExpense(const QString &amountDollars, const QDate &date, QString *error = nullptr);
    bool addExpense(const char *amountDollars, const QDate &date, QString *error = nullptr);
    bool addExpense(std::nullptr_t, const QDate &date, QString *error = nullptr);
    template <typename Amount, /* identical enable_if constraint */ int = 0>
    bool addExpense(const Amount &, const QDate &, QString *error = nullptr); // "Enter the expense as decimal text in dollars."
    const QVector<Expense> &expenses() const { return expenses_; }   // entry order

    const DecimalCents &totalSpendingCents() const;   // unchanged
    DecimalCents remainingBudgetCents() const;        // unchanged
private:
    bool addExpenseCents(const DecimalCents &amountCents, const QDate &date, QString *error);
    QVector<Budget> budgets_;
    QVector<Expense> expenses_;
    DecimalCents spendingCents_;   // running total kept as today
};
```

Rules (exact messages; on rejection nothing changes; on success `error` is cleared):
- **`updateBudgetAmount`**, checked in this order:
  1. `index < 0 || index >= budgets_.size()`: "Choose a budget to update."
  2. `amountCents < 0`: "Budget amount cannot be negative."
  3. `amountCents > max - (totalCents() - old)`: "Total budget is too large."
  4. Otherwise only `budgets_[index].amountCents` changes. Order, count and other fields stay the same.
- **`parseBudgetAmount`** trims, then checks in this order:
  1. Empty: "Budget amount is required."
  2. Not `^-?[0-9]+(?:[.][0-9]+)?$` (the same regex as expenses; share it as a file-local helper): "Enter a decimal dollar amount such as 12.99."
  3. Value has a fraction of a cent, judged by value (`"12.345"` is rejected, `"12.340"` gives 1234): "Budget amount must be in whole cents."
  4. Above qint64 max cents: "Budget amount is too large."
  5. Below qint64 min: "Budget amount cannot be negative."
  6. Otherwise it returns the cents. Negative in-range text returns a negative value, so `addBudget` and `updateBudgetAmount` emit their existing negative message. `"-0"` gives 0.

  Implementation hint, no double anywhere: `DecimalCents::fromDecimal(text + "e2")`, reject if `toString()` contains `.`, then `toString().toLongLong(&ok)`. If `!ok`, choose step 4 or 5 by sign.
- **Dated `addExpense`** checks in this order:
  1. Null pointer or empty/whitespace: "Expense amount is required."
  2. Format: "Enter a decimal dollar amount such as 12.99."
  3. Unrepresentable: "Expense amount cannot be represented."
  4. Negative: "Expense amount cannot be negative."
  5. Overflow: "Total spending is too large."
  6. `!date.isValid()` (null or impossible date): "Choose a valid expense date."

  On success it appends `Expense{cents, date}` and updates `spendingCents_`. Future dates are accepted. No-date overloads delegate with `QDate::currentDate()`.
- Includes allowed: own two headers, Qt Core, std. Forbidden: see Dependency enforcement.

### `src/BudgetDisplay.h` (header only; Qt Core, no project headers)

```cpp
struct BudgetRow    { QString category, total, period, perDay; };
struct ExpenseRow   { QString date, amount; };
struct BudgetScreen { QString totalBudget, totalSpending, remaining;
                      QVector<BudgetRow> budgets; QVector<ExpenseRow> expenses; };
class BudgetDisplay {
public:
    virtual ~BudgetDisplay() = default;
    virtual void showScreen(const BudgetScreen &screen) = 0;
    virtual void showMessage(const QString &text, bool isError) = 0;
};
```

### Controller: `src/BudgetController.h/.cpp` (target `budget_controller`: budget_model + Qt6::Core)

```cpp
#include "BudgetDisplay.h"   // plus <QDate>, <QString>; NO BudgetModel.h / DecimalCents.h
class BudgetModel;
class BudgetController {
public:
    explicit BudgetController(BudgetModel &model);         // defined in .cpp
    void setDisplay(BudgetDisplay *display);               // non-owning; non-null -> showScreen(screen()) at once
    BudgetScreen screen() const;
    bool createBudget(const QString &category, const QString &amountText,
                      const QDate &startDate, const QDate &endDate);
    bool updateBudgetAmount(int row, const QString &amountText);
    bool addExpense(const QString &amountText, const QDate &date);
private:
    BudgetModel &model_;
    BudgetDisplay *display_ = nullptr;
};
```

- Every action calls the model and then the display (if set).
  - On success: `showScreen(screen())`, then `showMessage(msg, false)`, return `true`. The messages are "Budget added." / "Budget amount updated." / "Expense saved.".
  - On failure: `showMessage(error, true)`, no `showScreen`, return `false`, and the model is unchanged.
  - `createBudget` and `updateBudgetAmount` call `BudgetModel::parseBudgetAmount` first, then `addBudget` or `updateBudgetAmount(row, cents)`.
- `screen()` formatting (file-local helpers in BudgetController.cpp):
  - Money (exact, from `DecimalCents`; budgets via `DecimalCents(qint64)`): dollars = `DecimalCents::fromDecimal(cents.toString() + "e-2")`; fraction padded to at least 2 digits; integer part grouped with `,`; sign before `$`. Examples: 0 → `$0.00`, 5 → `$0.05`, 123450 → `$1,234.50`, 1234.5 → `$12.345`, −0.5 → `-$0.005`, qint64 max → `$92,233,720,368,547,758.07`.
  - Per day: `QLocale(QLocale::English, QLocale::UnitedStates).toCurrencyString(budget.amountPerDay())`, which is the existing view behaviour.
  - Dates: `QDate::toString("MMM d, yyyy")`. Period is `"<start> to <end>"`.
  - Budget rows follow model index order. Expense rows are sorted by date descending; on the same date, higher model index (entered later) comes first.

### View: `src/BudgetView.h/.cpp` (target `app`)

```cpp
#include "BudgetController.h"   // plus <QWidget>; forward-declare widget classes
class BudgetView : public QWidget, public BudgetDisplay {
public:
    explicit BudgetView(BudgetController &controller, QWidget *parent = nullptr);
    void showScreen(const BudgetScreen &screen) override;
    void showMessage(const QString &text, bool isError) override;
private:
    void saveBudget(); void updateSelectedBudget(); void saveExpense();
    // members: category_, budgetAmount_ (QLineEdit), startDate_, endDate_, expenseAmount_ (QLineEdit),
    // expenseDate_ (QDateEdit, today, "MMM d, yyyy", popup), totalBudget_, totalSpending_, remaining_,
    // feedback_ (QLabel), budgetsTable_ (Category, Total, Period, Per day), expensesTable_ (Date, Amount)
};
```

- Buttons:
  - "Add budget" calls `controller_.createBudget(category, budgetAmount_->text(), start, end)` and clears `budgetAmount_` on `true`.
  - "Update selected budget" calls `controller_.updateBudgetAmount(row, budgetAmount_->text())`, where `row` is the first entry of `budgetsTable_->selectionModel()->selectedRows()`. No selection means row −1, and the model says "Choose a budget to update.". (It first used `currentRow()`, which review showed can point at a row the user never selected. Fixed in 808d5f9.)
  - "Save expense" calls `controller_.addExpense(expenseAmount_->text(), expenseDate_->date())` and clears the amount on `true`.
- Labels: "Total budget: %1", "Total spending: %1", "Remaining budget: %1".
- `showMessage` sets `feedback_` text, red (`color: #b3261e`) when `isError`.
- Tables are not editable; budgets use single-row selection.
- The constructor no longer pulls data, because `setDisplay` pushes the first screen.
- No `QValidator`, no `QLocale` money maths, no `<cmath>`, and no Model types.

### `src/main.cpp` (composition root; the only non-test file allowed to see every layer)

`QApplication app; BudgetModel model; BudgetController controller(model); BudgetView view(controller); controller.setDisplay(&view); view.show(); return app.exec();`. It now includes `BudgetModel.h` explicitly.

### Persistence seam (first written as "not built here"; built in Part 2 per D13, with D14's lock)

A later slice adds `src/BudgetStore.h/.cpp` as its own static library linking `budget_model` + Qt6::Sql. Its API is `bool load(BudgetModel &, QString *error)` and `bool save(const BudgetModel &, QString *error)`. The store depends on the Model; the Model never includes the store.
- Loading replays saved data through the public validated API: `addBudget`, plus `addExpense(dollarText, date)`. Expenses are stored as dollar text so no cents backdoor is needed.
- `main.cpp` calls `load` before `setDisplay`.
- Saving is called either by `main.cpp` or by the controller after each successful action (the controller may link Qt6::Sql; it still must not link Widgets).
- The layering check gets a BudgetStore rule in that slice.

## Decisions

- **D1.** Persistence is out of this plan; only the seam above. Why: it is a separate slice, and keeping it out keeps this change reviewable.
- **D2.** Category budgets stay. "Update the budget amount" is `updateBudgetAmount(index, cents)` with `addBudget`'s rules, and "Choose a budget to update." for an out-of-range index. Why: teammates' tests depend on the current semantics.
- **D3.** Expenses become `Expense{amountCents, date}` records via a dated `addExpense` overload set. The no-date overloads record today. A bad date is rejected after the amount rules, and future dates are accepted. Why: the list needs dates, and old call sites must keep working.
- **D4.** Budget text is parsed to whole cents by `BudgetModel::parseBudgetAmount`, never through a double. Why: the Model owns money validation, as it already does for expenses.
- **D5.** The View sees only `BudgetController.h` + `BudgetDisplay.h` strings. All money, date and order formatting lives in the controller. Why: the View then cannot depend on the Model, even by accident.
- **D6.** The controller pushes updates through the abstract `BudgetDisplay`. There are no Qt signals or `Q_OBJECT` in the Model or Controller. Why: updates reach the screen without the Model knowing any UI exists.
- **D7.** The budget amount is a `QLineEdit`. The update uses the selected row plus the same amount field. Why: it is the smallest UI change, and the Model stays the only validator.
- **D8.** Rules are enforced by CMake targets (Model and Controller build and link without Widgets), a `cmake -P` text check registered in ctest, and controller tests with a fake display. Why: the rules are checked by a test, so they can't just drift.
- **D9 (added).** Money formatting uses `DecimalCents::fromDecimal(cents + "e-2")`, with no new `DecimalCents` API. Why: it reuses exact arithmetic and leaves teammates' `DecimalCents` files and tests alone.
- **D10 (added).** The layering check inspects direct includes only. Why: it is simple, and it is enough, because `BudgetController.h` is banned from including Model headers, so nothing reaches the View transitively.
- **D11 (added).** Controller parses the amount text before the model checks the index. Why: one code path; an empty field with no selection shows "Budget amount is required." first.
- **D12 (CTO, during build).** The layering check lives at `tests/CheckLayering.cmake`, not `cmake/`. Why: it is a test. Keeping it under `tests/` means only the test author can write it and the implementer cannot weaken it. The first location was blocked by the team's role guard.

## Dependency enforcement

- **Targets** (CMakeLists.txt). The `src/*.cpp` glob is replaced:
   - `add_library(budget_model STATIC src/BudgetModel.cpp src/DecimalCents.cpp)`: PUBLIC include dir `src`, PUBLIC link `Qt6::Core` only.
   - `add_library(budget_controller STATIC src/BudgetController.cpp)`: PUBLIC link `budget_model Qt6::Core`.
   - `add_executable(app src/main.cpp src/BudgetView.cpp)`: PRIVATE link `budget_controller Qt6::Widgets Qt6::Sql`.
   - Under `BUILD_TESTING`: keep `file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)` and `add_executable(test_build ${TEST_SOURCES})`, PRIVATE link `budget_controller Qt6::Core`, include `tests`, `add_test(NAME test_build ...)`. Add `add_test(NAME layering COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/CheckLayering.cmake)`.
   - The result: if the Model or Controller uses a Widgets symbol, `test_build` fails to link.
- **`tests/CheckLayering.cmake`** (no new dependency):
   - Requires `SOURCE_DIR` (FATAL_ERROR if unset).
   - For each rule file it reads include lines with `file(STRINGS … REGEX "^[ \t]*#[ \t]*include")` and extracts the name between `<>`/`""`. No line numbers, because this avoids CMake list-splitting on `;`/`[`.
   - It collects all violations, prints each as `LAYERING VIOLATION: <relpath> includes <name> -- <reason>`, then `message(FATAL_ERROR "Layering check failed (<n> violations).")`.
   - A missing rule file reports `LAYERING CHECK: missing expected file <relpath>`, which counts as a violation.
   - On success it prints `Layering check passed.`
   - Patterns:
     - `GUI` = `^Qt(Widgets|Gui)(/|$)`, `^Q[A-Za-z]*Widget`, and `^(QApplication|QGuiApplication|QLabel|QPushButton|QLineEdit|QDateEdit|QComboBox|QSpinBox|QDoubleSpinBox|QCheckBox|QRadioButton|QFormLayout|QBoxLayout|QVBoxLayout|QHBoxLayout|QGridLayout|QHeaderView|QAbstractItemView|QTableView|QListView|QTreeView|QMessageBox|QMainWindow|QDialog|QFont|QColor|QPalette|QPixmap|QImage|QIcon|QPainter|QStyle|QAction|QMenu)$`.
     - `SIGNALS` = lines matching `(^|[^A-Za-z0-9_])(Q_OBJECT|Q_SIGNALS|Q_EMIT|emit|signals)([^A-Za-z0-9_]|$)`, reported as `LAYERING VIOLATION: <relpath> uses Qt signals/Q_OBJECT -- <line>`.

   | Files | Forbidden includes / content |
   | --- | --- |
   | `src/BudgetModel.h`, `.cpp`, `src/DecimalCents.h`, `.cpp` | `(^|/)Budget(Controller|View|Display)\.h$`, GUI, SIGNALS |
   | `src/BudgetController.h`, `.cpp` | `(^|/)BudgetView\.h$`, GUI, `Q_OBJECT` |
   | `src/BudgetController.h` (extra) | `(^|/)(BudgetModel|DecimalCents)\.h$` |
   | `src/BudgetView.h`, `.cpp` | `(^|/)(BudgetModel|DecimalCents)\.h$` |
   | `src/BudgetDisplay.h` | any `"…"` include, or any name that exists as a file in `src/` |
- **Controller tests.** `tests/test_budget_controller.cpp` uses a fake `BudgetDisplay` that appends every `BudgetScreen` and `(text, isError)` pair to vectors. It does not include Widgets.

## Files

Create:
- `src/BudgetDisplay.h`: view-ready structs + abstract display interface (D5, D6).
- `tests/CheckLayering.cmake`: layering check (D8b).
- `tests/test_budget_update.cpp`: `updateBudgetAmount` rules.
- `tests/test_budget_amount_text.cpp`: `parseBudgetAmount` rules.
- `tests/test_expense_records.cpp`: dated expenses, list, date rules, numeric guard.
- `tests/test_budget_controller.cpp`: controller actions, formatting, ordering, display calls.

Modify:
- `CMakeLists.txt`: explicit targets + layering test (D8a).
- `src/BudgetModel.h`, `src/BudgetModel.cpp`: D2, D3, D4.
- `src/BudgetController.h`, `src/BudgetController.cpp`: rewritten to the contract above (no test uses the old API).
- `src/BudgetView.h`, `src/BudgetView.cpp`: implements `BudgetDisplay`; new forms and tables (D7).
- `src/main.cpp`: wiring per D6.
- `README.md`, `REQUIREMENTS.md`: status and structure text.

Untouched: `src/DecimalCents.*`, all existing `tests/*`, `.github/workflows/ci.yml`, `SPECIFICATIONS.md`.

## Steps

- [x] 1. Write failing model tests (T1).
- [x] 2. Write failing controller tests with a fake display (T2).
- [x] 3. Write the layering check; confirm it fails on today's tree (T3).
- [x] 4. Restructure CMake into `budget_model` / `budget_controller` / `app` and register `layering` (T4).
- [x] 5. Implement Model additions (T5).
- [x] 6. Add `BudgetDisplay.h`; rewrite the controller; model, controller and layering tests go green (T6).
- [x] 7. Rewrite the View and `main.cpp`; full build, all tests, offscreen smoke (T7).
- [x] 8. Update README.md / REQUIREMENTS.md (T8).
- [x] 9. Independent verification, including mutation probes (T9).

## Tasks

Task order:

- T1, T2, T3 and T4 run in parallel (disjoint files).
- T5 needs T1 + T4.
- T6 needs T2 + T3 + T5.
- T7 needs T6.
- T8 may run in parallel with T5 to T7, but its text must match T7's result.
- T9 runs last.

| # | Task | Role | Files it may touch | Done when |
| --- | --- | --- | --- | --- |
| T1 | Model tests: (a) update: amount changes and totals/remaining recalculate, index −1 / ==size / empty model, negative, overflow (`{max-100, 100}`: update [1] to 101 rejected, [0] to max-100 accepted), zero allowed, rejection leaves everything unchanged with exact messages, success clears error; (b) parse: `"250"`→25000, `" 12.34 "`→1234, `"12.340"`→1234, `"0"`/`"-0"`→0, `"-5"`→−500, `""`/`"  "`, `"abc"`, `"1e2"`, `"$5"`, `"1,000"`, `"12.345"`, `"92233720368547758.07"`→max, `"92233720368547758.08"` too large, `"-92233720368547758.08"`→min, `"-92233720368547758.09"` negative message; (c) expenses: record amount+date, entry order, `"0.125"`→`"12.5"`, future date accepted, `QDate()` and `QDate(2026,2,30)` rejected, amount error wins over date error, `nullptr`/null `const char*` with date, numeric `12.5`/`1250`/`DecimalCents(1250)` with date rejected with the guard message, overflow leaves list unchanged, no-date overload records a date in `[before, after]` captured around the call | team-test-author | `tests/test_budget_update.cpp`, `tests/test_budget_amount_text.cpp`, `tests/test_expense_records.cpp` | DW1 |
| T2 | Controller tests (fake display): setDisplay pushes one screen with `$0.00` x3 and no message; createBudget("Groceries","250.00",2026-01-01,2026-01-31) gives true, row `{Groceries,$250.00,Jan 1, 2026 to Jan 31, 2026,$8.06}`, message ("Budget added.",false); failures for `"12.345"`, `"-5"`, `"abc"`, `""` give exact model messages, isError true, no extra screen, model unchanged; update(0,"300") gives `$300.00` + "Budget amount updated."; update(5,…)/(−1,…) gives "Choose a budget to update."; addExpense("12.50",2026-03-02) gives row `{Mar 2, 2026,$12.50}` + "Expense saved."; `QDate()` gives "Choose a valid expense date."; ordering (Mar 1 "1", Mar 3 "3", Mar 1 "2") gives `$3.00,$2.00,$1.00`; budget 10.00 + expense "10.005" gives remaining `-$0.005`, spending `$10.005`; `"1234.5"` gives `$1,234.50`; expense max gives `$92,233,720,368,547,758.07`; actions with no display set still return correctly | team-test-author | `tests/test_budget_controller.cpp` | DW2 |
| T3 | Layering check per Dependency enforcement | team-test-author | `tests/CheckLayering.cmake` | DW3 |
| T4 | CMake targets + `layering` test | team-implementer | `CMakeLists.txt` | DW4 |
| T5 | Model per Layer contract | team-implementer | `src/BudgetModel.h`, `src/BudgetModel.cpp` | DW5 |
| T6 | `BudgetDisplay.h` + controller per contract | team-implementer | `src/BudgetDisplay.h`, `src/BudgetController.h`, `src/BudgetController.cpp` | DW6 |
| T7 | View + main per contract | team-implementer | `src/BudgetView.h`, `src/BudgetView.cpp`, `src/main.cpp` | DW7 |
| T8 | REQUIREMENTS "Walking skeleton status" rows. "Update the budget amount", "Choose an expense date", "Show total spending and remaining budget", "Show a list of expenses" become Done; "Record an expense amount" becomes "Done in the GUI"; persistence stays "Not started". README: requirements covered, GUI sentence, project structure (add `BudgetDisplay.h`, `tests/CheckLayering.cmake`, layer rules), test description | team-implementer | `README.md`, `REQUIREMENTS.md` | DW8 |
| T9 | Run Verification end to end | team-verifier | none | DW9 |

Done-when commands (run from the repo root; `cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt` first):

```sh
# DW1 / DW2: test_build fails ONLY in the new test files; no tracked test modified.
errs=$(cmake --build build --target test_build -- -k 2>&1 | grep ': error')
[ -n "$errs" ] && ! echo "$errs" | grep -vE 'tests/test_(budget_update|budget_amount_text|expense_records|budget_controller)\.cpp'
[ -z "$(git status --porcelain -- tests | grep -v '^??')" ]
# DW2 extra:
grep -q BudgetDisplay tests/test_budget_controller.cpp && ! grep -qE 'QWidget|QApplication|QtWidgets' tests/test_budget_controller.cpp

# DW3: fails on today's tree for the right reasons
out=$(cmake -DSOURCE_DIR="$PWD" -P tests/CheckLayering.cmake 2>&1); rc=$?
[ $rc -ne 0 ] && echo "$out" | grep -q 'LAYERING VIOLATION: src/BudgetController.h includes BudgetModel.h' \
  && echo "$out" | grep -q 'LAYERING CHECK: missing expected file src/BudgetDisplay.h'

# DW4
cmake --build build --target budget_model budget_controller app
ctest --test-dir build -N | grep -q 'Test.*: layering' && ctest --test-dir build -N | grep -q 'Test.*: test_build'

# DW5: model + app build; remaining compile errors only in the controller test
cmake --build build --target budget_model app
! cmake --build build --target test_build -- -k 2>&1 | grep ': error' | grep -v 'tests/test_budget_controller.cpp' | grep -q .

# DW6
cmake --build build --target test_build && ctest --test-dir build -R 'test_build|layering' --output-on-failure
! otool -L build/test_build | grep -q QtWidgets      # Linux: ! ldd build/test_build | grep -q Widgets

# DW7
cmake --build build && ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/app & pid=$!
sleep 3; kill -0 $pid && kill $pid && echo SMOKE PASS

# DW8
grep -qE '^\| Update the budget amount \| Done' REQUIREMENTS.md && grep -qE '^\| Choose an expense date \| Done' REQUIREMENTS.md \
  && grep -qE '^\| Show a list of expenses \| Done' REQUIREMENTS.md && grep -q 'BudgetDisplay.h' README.md && grep -q 'CheckLayering' README.md

# DW9: every command in Verification passes, then:
git status --porcelain   # only the Files list above (plus docs/plans/mvc-mvp.md)
```

## Verification

```sh
cd /Users/tranquanghuy/Projects/csci187/student-expense-tracker
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
ctest --test-dir build --output-on-failure            # expect: test_build and layering both Passed
! otool -L build/test_build | grep -q QtWidgets       # model+controller tests run without Widgets

# Mutation probes on a scratch copy (repo never modified). Each must exit nonzero with the shown message.
probe() { S=$(mktemp -d); cp -R src "$S"/; printf '\n%s\n' "$2" >> "$S/src/$1"
  if cmake -DSOURCE_DIR="$S" -P tests/CheckLayering.cmake >"$S/out" 2>&1; then echo "PROBE FAILED (check passed): $1"
  elif grep -q "LAYERING VIOLATION: src/$1" "$S/out"; then echo "probe ok: $1"
  else echo "PROBE FAILED (wrong message): $1"; fi; rm -rf "$S"; }
# Pass = seven "probe ok" lines and no "PROBE FAILED".
probe BudgetModel.h      '#include <QWidget>'
probe DecimalCents.cpp   '#include "BudgetController.h"'
probe BudgetModel.cpp    'Q_OBJECT'
probe BudgetController.h '#include "BudgetModel.h"'
probe BudgetController.cpp '#include <QLabel>'
probe BudgetView.cpp     '#include "DecimalCents.h"'
probe BudgetDisplay.h    '#include "BudgetModel.h"'

# In-tree probe proves ctest wiring, then restores the file.
cp src/BudgetModel.h build/BudgetModel.h.bak && printf '\n#include <QtWidgets/QWidget>\n' >> src/BudgetModel.h
! ctest --test-dir build -R layering --output-on-failure
cp build/BudgetModel.h.bak src/BudgetModel.h && ctest --test-dir build -R layering   # Passed again

# Offscreen smoke: app starts and stays up.
QT_QPA_PLATFORM=offscreen ./build/app & pid=$!; sleep 3; kill -0 $pid && kill $pid && echo SMOKE PASS
git status --porcelain
```

Manual check (Huy, real window, `./build/app`):
1. Add Groceries "250" for this month; the labels show $250.00 / $0.00 / $250.00.
2. Select the row, type "300", and click Update selected budget; the total and remaining show $300.00.
3. Save expense "12.50" (date = today); the expense appears at the top and remaining shows $287.50.
4. Save "abc"; you see the red message and nothing changes.
5. Save "400"; remaining shows -$112.50.

## Risks

- **Overload resolution.** The new dated `addExpense` set mirrors the existing one exactly, including the template guard. Every existing call (`nullptr`, `const char*`, `QString`, numeric, `DecimalCents`, `__int128`) still resolves to the same overload, and T1 adds the dated numeric cases. If a compile ambiguity appears, the fix is in the overload set, never in a teammate's test.
- **`DecimalCents` has an `explicit` default constructor**, so `Expense{}` or `Expense{amount}` will not compile. Always construct `Expense{cents, date}`.
- **Compile-level isolation is best-effort.** Homebrew may expose Qt headers on a global include path, so a stray `<QWidget>` in the Model might compile. The text check is the guaranteed gate, and the link isolation catches actual Widgets symbol use.
- **Text-check false positives.** The words "emit" or "signals" in a Model comment fail the check. The message prints the line, so rename the word.
- **Text-check false negatives.** Macro includes and new files are not covered until rules are added (see Follow-ups).
- **Midnight flake** in the "no-date records today" test. Mitigated by accepting either the `before` or the `after` date.
- **Non-owning display pointer.** `view` is destroyed before `controller` at exit. No controller call happens after `app.exec()` returns, so this is safe. Do not reorder main.
- **Per-day stays `double`.** It is display only and never used in totals, which is unchanged behaviour.
- **Branch compatibility.** Teammates' unmerged branches calling `controller.budgets()` / `totalCents()` / `createBudget(..., qint64, ...)` will break on rebase. Call this out in the PR description.
- `cmake --build … -- -k` is Makefiles syntax (Ninja: `-k 0`). It is used only in intermediate done-when checks, not in CI.
- ~~There are no security or data-migration implications. Data stays in memory (D1).~~ Superseded: data is now saved to a local SQLite file (Part 2). Its data-loss risks were reviewed and designed out (findings 1–7, D14).
- (Added in round 2.) CI has not run yet. Ubuntu's apt Qt (likely 6.4) is older than the local Qt 6.12, so the first push may surface API differences.

## Open Questions

1. **Future-dated expenses.** Should the app reject them? *Default taken: accept them (no rule is invented). A later rule would be one more check after the date-validity check.*
2. **New user-facing wording.**
   - Messages: "Budget amount updated.", "Expense saved.", "Choose a budget to update.", "Choose a valid expense date.", "Budget amount is required.", "Budget amount must be in whole cents.", "Budget amount is too large.".
   - Labels: "Total budget:", "Total spending:", "Remaining budget:".
   - Button: "Update selected budget".

   *Default taken: use them as written; changing any one is a single string plus its test.*
   Part 2 adds these messages: "Saved data could not be loaded: …", "Your change was not saved: …", "Saved data could not be loaded, so changes are not being saved.", "Saved data is from a newer version." and "The budget is already open in another window."
3. **One overall budget versus category budgets (D2).** The P3 doc says "one overall budget". The skeleton on main already had several category budgets, each with a date range; the docs list category budgets under "Want for later". This branch keeps them, with total budget = the sum of all budgets, and adds "update the selected budget's amount". *Default taken: keep them, because teammates' tests depend on them. Collapsing to a single budget would be a separate change that the team agrees on.*
4. **Duplicates already saved (D18).** Files saved before the overlap rule can hold duplicates; Huy's own file has two identical Meal plan rows. They are kept and shown as saved, and the app opens and saves normally. *Default taken: no automatic merge or delete. Should the team want a way to remove a budget, that is the "edit and recover entries" item from the later list.*
5. **Part 3 wording.** These strings are the defaults:
   - "A budget for this category already covers some of those dates."
   - "You already have a <category> budget of <$amount> for <start> to <end>."
   - "Budget updated.", "Choose another category for this budget." and "Budget not added."
   - The buttons "Update existing budget", "Choose another category" and "Cancel".
   - "Uncategorized" for old expenses.
   *Default taken: use as written.*

## Follow-ups

- After PR #7 (`huy/spec-decimal-cents`) merges, update SPECIFICATIONS.md:
  - Architecture table: `BudgetDisplay`, targets, and the fact that test_build now links the controller.
  - Expense record and `updateBudgetAmount` / `parseBudgetAmount` / dated `addExpense` rules with exact messages.
  - The "Current screen" section.
  - Testing table rows for the four new test files.
- ~~Persistence slice (D1, seam above), including a `BudgetStore` rule in `tests/CheckLayering.cmake`.~~ Done in Part 2.
- Harden the layering check: it treats header names case-sensitively, while macOS is case-insensitive, and it does not flag a bare forward declaration of a Controller, View or Display type in the Model.
- A test-quality nit: `tests/test_controller_persistence.cpp` around line 418 should `REQUIRE` the row count before `.at(0).at(0)`, so a broken save reports a failure instead of a segfault.
- Extend the layering rules whenever a new `src/` file is added.

## Approval Checklist

- [ ] Open questions resolved
- [ ] File list and steps confirmed

## Security Review

2026-11-01, plan mode: **`security:pass`**. The security-reviewer returned `{"review_completed": true, "findings": []}`; verdict.py printed `security:pass`. Nothing to design out. No data review: the plan touches no schema, migration or stored data.

## Plan Review

Self-approved by the CTO on 2026-11-01 (autonomous mode).
- Goal: the MVP flows (budget add/update, dated expenses, totals, expense list) wired Model, then Controller, then View, with the four architecture rules enforced by build targets and a ctest check.
- Files and steps: they match the Layer contract. `DecimalCents.*`, every existing test, CI and SPECIFICATIONS.md stay untouched.
- Verification: full build, ctest (`test_build` + `layering`), seven layering mutation probes plus one in-tree probe, an `otool` check that `test_build` has no QtWidgets, and an offscreen app smoke run.
- Open questions: both have defaults that touch no money, security or logged decision (accept future-dated expenses; use the drafted UI strings). The build proceeds on those defaults and lists them for Huy.
- Security and data findings: none.
- Scope: this matches the agreed proposal. Persistence is deliberately left for its own slice (D1). (Later reversed by D13.)
- Execution change from the Tasks table: T1–T3 go to one test-author run before any implementation. T4–T6 then go to one implementer run (opus, because it is the data model and the layer contract), committed as three separate commits. T7–T8 go to a second implementer run, committed separately. All of these run one after another, because the agents share one `build/` directory.

## Task Ledger

Branch: `huy/mvc-mvp` (local only; no upstream set). Base commit: `ed49eb0` (origin/main).

| # | Task | Role | Model | Files it may touch | Output | Done when | Depends on | Status |
|---|------|------|-------|--------------------|--------|-----------|-----------|--------|
| 1 | Model tests (update, parse, dated expenses) | team-test-author | sonnet | tests/test_budget_update.cpp, tests/test_budget_amount_text.cpp, tests/test_expense_records.cpp | failing tests | DW1 | – | done |
| 2 | Controller tests with fake display | team-test-author | sonnet | tests/test_budget_controller.cpp | failing tests | DW2 | – | done |
| 3 | Layering check | team-test-author | sonnet | tests/CheckLayering.cmake | check that fails on today's tree | DW3 | – | done |
| 4 | CMake targets + layering test | team-implementer | opus | CMakeLists.txt | build config | DW4 | 3 | done |
| 5 | Model additions | team-implementer | opus | src/BudgetModel.h, src/BudgetModel.cpp | code | DW5 | 1, 4 | done |
| 6 | BudgetDisplay.h + controller | team-implementer | opus | src/BudgetDisplay.h, src/BudgetController.h, src/BudgetController.cpp | code | DW6 | 2, 3, 5 | done |
| 7 | View + main | team-implementer | sonnet | src/BudgetView.h, src/BudgetView.cpp, src/main.cpp | code | DW7 | 6 | done |
| 8 | README / REQUIREMENTS status | team-implementer | sonnet | README.md, REQUIREMENTS.md | docs | DW8 | 7 | done |
| 9 | Independent verification | team-verifier | sonnet | none | report | DW9 | 1–8 | done |

## Progress Log

- 2026-11-01 plan: written by team-architect; security review of the plan returned `security:pass`; self-approved (see Plan Review).
- 2026-11-01 T1–T3: done. The test-author wrote 132 cases across 4 new test files, all failing on missing API (`no member named 'updateBudgetAmount'`, missing `BudgetDisplay.h`). The layering check first failed with exactly 2 expected violations. The role guard blocked `cmake/`, so the check moved to `tests/CheckLayering.cmake` (D12). Commits 7c6782e, 460ca15, db8c3d2.
- 2026-11-01 T4–T6: done in one implementer run (opus), split into 3 commits (bf87c4b, 97bdaec, 7d8ec96). ctest: 2/2 passed. doctest: 218 cases, 1819 assertions, 0 failed. `otool -L test_build` shows QtCore only.
- 2026-11-01 T7–T8: done (2539470, a534bde). The full build is clean and ctest passes 2/2. The offscreen smoke is BLOCKED BY ENVIRONMENT: `dyld: Library not loaded: /usr/lib/libhvf.dylib`, referenced from Homebrew freetype 2.15.0 on macOS 15.0. A 5-line Qt Widgets program crashes the same way, so this is not this code.
- 2026-11-01 T9: the verifier ran a clean configure and build in /tmp, then ctest (100% of 2) and doctest (218/1819, 0 failed). `nm -u` shows 0 QWidget references in libbudget_model.a and libbudget_controller.a. The verifier's read-only role guard blocked the mutation probes, and so did a test-author re-run. The guards were not widened; the probes were later run by the trial falsifier on /tmp copies.
- 2026-11-01 Review: team-reviewer APPROVE with 1 MINOR. "Update selected budget" used `currentRow()`, which can update a budget the user never selected; fixed in 808d5f9 along with the REQUIREMENTS row nit. clean-check: CLEAN, 0 BLOCKER and 2 SHOULD (R3: repeated `reject` lambdas and repeated message strings in BudgetModel.cpp). These were left as optional; part of the duplication is teammates' existing code. Security (diff): `security:pass`. gitleaks found no leaks across 9 commits, no new packages, semgrep p/c returned 0 results, and the LLM review found nothing.

## Trial (round 1)

2026-11-01, base `ed49eb0`, HEAD `793ef10`. **Verdict: REJECT.** The judge ruled 15 requirements MET. It ruled one FAILED: MVP Req 3's last bullet, "The user's saved budget and spending entries remain available after closing and reopening the application". The request says to take MVP scope from the docs and implement it, and D1 cut this requirement without Huy's sign-off.

Nothing was faked: the integrity scan found 0 flags; 12 of 12 falsifier mutations and 8 of 8 advocate mutations were caught; 14 of 14 falsifier layering probes and 6 of 6 judge layering probes were caught; and a real-widget offscreen harness passed 19 of 19 checks.

Claims audit: claims 1–13 BACKED. Claim 14 (security) was partly backed: the judge re-ran gitleaks, but not semgrep or the LLM review.

## Part 2: Persistence slice (added after trial round 1)

- **D13 (CTO, reverses D1).** Persistence is built in this branch. Why: it is MVP in the course docs, and the request asked for the MVP. The plan's own seam is used, so the Model still knows nothing about storage.

### Contract

- **`src/BudgetStore.h/.cpp`**, new static library `budget_store`, linked to `budget_model` + Qt6::Core + Qt6::Sql (no Widgets).
  - `explicit BudgetStore(const QString &databasePath);` with a unique SQLite connection name per instance; the destructor closes and removes the connection.
  - `bool load(BudgetModel &model, QString *error = nullptr);`
    - It creates the tables if they are missing; a missing file loads as an empty model.
    - It replays the rows into a fresh local `BudgetModel` through the public validated API: `addBudget`, then `addExpense(dollarText, date)`, in stored position order. Only if every row is accepted does it assign the result to `model`.
    - Any SQL failure or rejected row returns false, sets `error`, and leaves `model` unchanged.
  - `bool save(const BudgetModel &model, QString *error = nullptr);` rewrites both tables inside one transaction. On failure it rolls back and returns false with `error`.
  - **Schema** (`PRAGMA user_version = 1`):
    - `budgets(position INTEGER NOT NULL, category TEXT NOT NULL, amount_cents INTEGER NOT NULL, start_date TEXT NOT NULL, end_date TEXT NOT NULL)`
    - `expenses(position INTEGER NOT NULL, amount_dollars TEXT NOT NULL, date TEXT NOT NULL)`
    - Dates are ISO `yyyy-MM-dd`.
    - Expense amounts are exact dollar text made from `DecimalCents` by shifting the decimal point (`fromDecimal(cents + "e-2")`), so `1234.5` cents is stored as `"12.345"`. No `double` anywhere.
  - It must not include Controller, View, Display or any Widgets/Gui header. The Model must not include `BudgetStore.h`.
- **Controller additions:**
  - `void setStore(BudgetStore *store);` is non-owning, nullable, and forward-declared in BudgetController.h.
  - `bool loadSaved();`
    - With no store it returns true and does nothing.
    - Otherwise it calls `store->load(model_)`. On success it pushes `showScreen`; there is no message.
    - On failure it shows `showMessage("Saved data could not be loaded: " + error, true)` and returns false. The model is unchanged.
  - After every successful action, if a store is set, the controller calls `store->save(model_)`.
    - If the save succeeds, the screen and the success message are pushed as before.
    - If the save fails, the action still returns true and the screen is still pushed. The message becomes `("Your change was not saved: " + error, true)`.
  - Failed actions never save.
- **`main.cpp`:**
  - It calls `QCoreApplication::setApplicationName("SCU Student Budget")`.
  - Path: `QStandardPaths::writableLocation(AppDataLocation)`, created with `QDir().mkpath`, plus `/budget.sqlite`.
  - Wiring order: `BudgetStore store(path)`, `controller.setStore(&store)`, `controller.setDisplay(&view)`, `controller.loadSaved()`.
  - The store must outlive the controller's last call, so declare it before the controller.
- **CMake:**
  - Add `budget_store`.
  - `budget_controller` links `budget_store`.
  - `app` links `budget_controller` + Widgets.
  - `test_build` links `budget_controller` + Qt6::Core + Qt6::Sql, still no Widgets.
- **Layering check additions** (in `tests/CheckLayering.cmake`, owned by the test author):
  - `src/BudgetStore.h/.cpp` are banned from `BudgetController`, `BudgetView`, `BudgetDisplay`, GUI headers and SIGNALS.
  - Model files are also banned from `BudgetStore.h`.
  - View files are also banned from `BudgetStore.h`, because the View only calls the Controller.
  - `BudgetController.h` is banned from `BudgetStore.h` (forward declaration only).
  - A missing `src/BudgetStore.h` counts as a violation.

### Tasks

| # | Task | Role | Model | Files it may touch | Done when | Status |
|---|------|------|-------|--------------------|-----------|--------|
| 10 | Failing store round-trip tests, controller persistence tests, layering rules for BudgetStore | team-test-author | sonnet | tests/test_budget_store.cpp, tests/test_controller_persistence.cpp, tests/CheckLayering.cmake | new tests fail only for the missing BudgetStore API; layering check fails only on the missing BudgetStore.h; existing tests untouched | done |
| 11 | BudgetStore + CMake + controller store wiring + main | team-implementer | opus | src/BudgetStore.h, src/BudgetStore.cpp, CMakeLists.txt, src/BudgetController.h, src/BudgetController.cpp, src/main.cpp, README.md, REQUIREMENTS.md | full build clean; ctest 100%; `otool -L test_build` has no QtWidgets; persistence row in REQUIREMENTS.md marked Done | done |

### Part 2 reviews and design corrections (2026-11-01)

The earlier `## Security Review` covered Part 1 only. Part 2 results:
- **Security (plan mode):** verdict.py printed `security:pass`. Two LLM findings were filtered and dropped below the bar:
  - L1 scored 3/10. It said the plan carries earlier verdicts; that is answered by this scoping note.
  - L2 scored 7/10. It is the same flaw as data finding 1 and is designed out below.
- **Data review:** CHANGES NEEDED. One CRITICAL, three MEDIUM and three LOW findings; there is no stop condition. All seven are adopted into the contract below, which replaces the matching parts of the contract above.

1. **CRITICAL: save after a failed load would wipe the saved file.** `BudgetStore` tracks `loaded_ = false`. `save` refuses unless the last `load` on this instance succeeded: it returns false with "Saved data could not be loaded, so changes are not being saved." This is enforced in the store, not only in the controller. Test: corrupt the file, load, add an expense, and confirm the file still holds its original rows. After this fix no destructive step remains, so Huy has nothing to sign off.
2. **MEDIUM: schema version.** On `load`:
   - `user_version` 0 with no tables: create both tables and set version 1 in one transaction (plain `CREATE TABLE`).
   - Version 1: proceed.
   - Any other version: fail with "Saved data is from a newer version." Saving stays blocked (finding 1).
3. **MEDIUM: return values.** Check every `open`, `transaction`, `prepare`, `exec` and `commit` result. Roll back on any false, including a failed commit. Clear rows with `DELETE FROM`, never `DROP TABLE`. `error` carries the Qt driver text.
4. **MEDIUM: connections.** The store keeps only a unique connection name (a static counter) and fetches `QSqlDatabase::database(name)` locally in each call. The destructor closes inside an inner scope, then calls `removeDatabase`. The file is opened in `load`/`save`, so open errors reach `error`. The SQLite plugin needs a `QCoreApplication`: `main.cpp` already creates `QApplication` first, and store tests create one in a test-file helper. `tests/test_main.cpp` is not edited.
5. **LOW: dates.** Write and parse with the explicit format `"yyyy-MM-dd"`. `save` fails if a date does not round-trip.
6. **LOW: order.** Use `position INTEGER PRIMARY KEY` in both tables and `ORDER BY position` in both SELECTs.
7. **LOW: database checks.** `budgets`: `CHECK(amount_cents >= 0)` and `CHECK(length(category) > 0)`. `expenses`: `CHECK(typeof(amount_dollars) = 'text')`. Both amount columns are declared exactly `TEXT` or `INTEGER`; never use `NUMERIC`, which would turn "12.345" into a float.

### Round 2 review triage (2026-11-01)

- Fresh-eye review: APPROVE. One MINOR (two app copies on one file) and one NIT (wording).
- Data review of the code: CHANGES NEEDED. One HIGH, reproduced with a probe: two stores loaded on the same file, and the last save silently erases the other's rows. Two LOWs (an ignored rollback result; "newer version" wording for a negative version). All seven plan corrections were confirmed in the code.
- clean-check: NEEDS CLEANUP. One BLOCKER (R2): `FakeDisplay` is duplicated in tests/test_budget_controller.cpp and tests/persistence_test_support.h. SHOULDs: dollar-text helper repeated, `report()` name, `reject` lambda copies.
- Security diff: gitleaks found no leaks, no new packages, and semgrep p/c returned 0 results. The LLM raised one finding, the same L1 as before (verdicts recorded in this plan); it goes to the filter.

Fixing now (round 2 of 2):
- **D14.** `BudgetStore` holds a `QLockFile` at `<databasePath>.lock` for its lifetime. The lock is taken in `load` with `tryLock(0)`, with `setStaleLockTime(0)`, so a lock is stale only when its owner process is dead.
  - If the lock is taken, `load` fails with "The budget is already open in another window." The existing `loaded_` gate then blocks saving.
  - Test: two stores on one path. The second load fails, the second save is refused, and the first store's rows are intact.
  - Existing tests that reopen a file while the first store is still alive must scope the first store to end before reopening.
  - Why: a student who opens the app twice must not lose entries silently.
- R2: test_budget_controller.cpp uses the shared `FakeDisplay` from tests/persistence_test_support.h.

Left as optional (with reasons):
- The SHOULDs would touch teammates' existing code or are naming only.
- The data LOWs are safe as they stand: saving stays blocked and the first error is reported.

## Trial (round 2)

2026-11-01, base `ed49eb0`, HEAD `649273d`. **Verdict: ACCEPT.**

The judge ruled 12 requirements MET. Number 13, the final report, was UNSETTLED because that report comes after the verdict.
- The judge rebuilt the branch and ran 4 of its own mutation probes, all caught: an include in the Model, an update that always edits row 0, stored amounts rounded to 2 decimals, and the last expense write dropped.
- It reran the falsifier's offscreen harness on the real View, Controller, Model and Store, with the data folder moved to /tmp. Every MVP flow held, including a restart: $412.505 survived a reopen. A second window is refused, and the file stays intact.
- The integrity flag (COMPUTED_EXPECTED at tests/test_controller_persistence.cpp:123) was ruled BENIGN, because the same test also checks fixed values.
- Claims audit: claims 1–11 BACKED. Claim 12: gitleaks and semgrep were re-run and are BACKED; the LLM security finding is recorded only here.
- Recommended before Huy pushes: answer Open Question 3; REQUIRE the row count at tests/test_controller_persistence.cpp:418; treat the first Ubuntu CI run as the real check against apt's older Qt.

## Final check (CTO, 2026-11-01)

Fresh configure and build in a /tmp build directory.
- Build: 0 warnings or errors.
- `ctest`: 100% of 2 tests passed (`test_build`, `layering`).
- doctest: 269 test cases and 2364 assertions, 0 failed.
- `otool -L test_build`: QtSql and QtCore only.
- Layering check: passed.
- Changes from `ed49eb0`: 22 files changed. No existing test file was modified (only added). 21 local commits, with no upstream.

## Retro

- **Agents spawned (about 30):**
  - Planning: architect 1.
  - Tests: test-author 6, including 2 re-dispatches caused by role guards.
  - Building: implementer 6.
  - Checking: verifier 1, reviewer 2, clean-code 2, data engineer 3, security reviewer 7 (find and filter passes).
  - Trial: falsifier 2, advocate 2, judge 2.
- **Fix rounds used:** 2 of 2.
  - Round 1: build persistence after trial REJECT.
  - Round 2: the single-instance lock (data HIGH) and the duplicated test helper (clean-check BLOCKER).
- **What went wrong:**
  - I cut an MVP requirement (persistence) on my own judgement. The trial rejected the cut, which was right: the request said to take scope from the docs.
  - The role guards blocked the planned `cmake/` location and the read-only roles' mutation probes. In the end only the falsifier and judge could run probes.
  - Each test-author missed one store-lifetime case, which the implementer caught.
  - The app GUI cannot launch on this Mac (Homebrew freetype linked to a missing /usr/lib/libhvf.dylib). The GUI evidence comes from the falsifier's harness with a /tmp freetype workaround.
- **What I'd brief differently next time:**
  - Never cut a requirement that the docs mark MVP without asking.
  - Put checks under `tests/` from the start.
  - Give the test-author an explicit "no two live stores on one path" rule.
  - Plan mutation probes for the falsifier rather than for the verifier.

## Part 3: Duplicate-budget guard and expense category (2026-11-01)

Verdict: **GO.** Both items are small, in scope, and fit the existing layers. No teammate test adds two overlapping budgets in the same category (checked every `addBudget` call in the base test files), so the new rule breaks none of them. Size: STANDARD.

### Decisions

- **D15. What "already has a budget in that time frame" means.** A new budget is a duplicate when another budget has the same category and overlapping dates. Category comparison trims spaces and ignores case. Dates count inclusively, so Nov 1–Dec 1 and Dec 1–Dec 31 overlap on Dec 1. The same category with no overlap is allowed, and so is a different category on the same dates. Why: Huy's case is identical rows, but an overlapping one double-counts money just the same.
- **D16. The Model rejects; it doesn't ask.**
  - `addBudget` gets an optional out-parameter: `bool addBudget(const Budget &budget, QString *error = nullptr, qsizetype *conflictIndex = nullptr);`
  - The overlap check runs last, after all existing rules, so existing messages and their order are unchanged.
  - On overlap it returns false with "A budget for this category already covers some of those dates." and sets `*conflictIndex` to the first overlapping budget. On any other outcome it sets `*conflictIndex` to -1.
  - Why: the Model states the fact, and the Controller decides what to ask the user. The Model never learns that a dialog exists.
- **D17. Replacing an existing budget.**
  - New `bool replaceBudget(qsizetype index, const Budget &budget, QString *error = nullptr);` applies every `addBudget` rule except one: the overflow check uses total − old + new.
  - Overlap check: reject only if the replacement overlaps a same-category budget that the old version did NOT already overlap ("don't create a new conflict").
  - An out-of-range index gets "Choose a budget to update."
  - Why: in a clean file this is just "no overlap". In a file that already holds duplicates (Huy's real file has two identical Meal plan rows), Update still works instead of failing on the leftover twin.
- **D18. Files that already hold duplicates.**
  - New `bool restoreBudget(const Budget &budget, QString *error = nullptr);` applies every `addBudget` rule except the overlap rule. `BudgetStore::load` uses it instead of `addBudget`.
  - Why: data already saved is kept exactly as it is. Nothing is merged or deleted, the app opens and can save again, and the new rule only stops new duplicates.
  - The duplicate rows stay visible. There is no delete feature (deleting entries is a "Want for later" item); Huy can set one to $0 with Update if he wants. This is Open Question 4.
- **D19. The conflict prompt.**
  - `BudgetDisplay.h` gains `enum class ConflictChoice { UpdateExisting, ChooseAnotherCategory, Cancel };` and `virtual void askBudgetConflict(const QString &message) = 0;`.
  - When `createBudget` is rejected with a conflict index, the controller stores a pending request (index + candidate budget). It then calls `askBudgetConflict("You already have a <category> budget of <$amount> for <start> to <end>.")` with the existing budget's values and returns false. It does not save and shows no other message.
  - New `bool BudgetController::resolveBudgetConflict(ConflictChoice choice);`
    - With nothing pending it returns false and does nothing.
    - **UpdateExisting:** calls `replaceBudget(pending index, candidate)`. On success it shows the screen and the message "Budget updated.", and saves, through the same path as other actions. On failure it shows the error.
    - **ChooseAnotherCategory:** changes nothing, does not save, and shows ("Choose another category for this budget.", false).
    - **Cancel:** changes nothing, does not save, and shows ("Budget not added.", false).
    - Every choice clears the pending request. So does any other controller action.
  - Why: the Controller mediates. The View only reports the user's answer by calling the Controller.
- **D20. The View's prompt.**
  - `BudgetView::askBudgetConflict` queues a `QMessageBox` (via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`), so it opens after `createBudget` returns and the controller is never re-entered mid-call.
  - The box has exactly three buttons: "Update existing budget", "Choose another category", "Cancel".
  - It then calls `controller_.resolveBudgetConflict(choice)`. On "Choose another category" the View keeps the form values and focuses the category picker. On a successful Update it clears the budget amount field.
- **D21. Expense category.**
  - `Expense` gains `QString category` (trimmed; empty means uncategorized, which happens only for expenses saved before this change).
  - New Model overload: `bool addExpense(const QString &amountDollars, const QDate &date, const QString &category, QString *error = nullptr);`. The existing overloads keep their signatures and record an empty category.
  - Controller: `bool addExpense(const QString &amountText, const QDate &date, const QString &category = QString());`.
  - `ExpenseRow` becomes `{ date, amount, category }`, with the new field last so existing field-by-field comparisons keep working. An empty category is shown as "Uncategorized".
  - View: an expense category picker that uses the same list as the budget picker (one list in the View, used by both). The expenses table columns become Date, Category, Amount.
  - No per-category totals or anything beyond this.
  - Why: this is the smallest change that records, shows and saves the category without touching teammates' `addExpense(text)` calls.
- **D22. Saving the expense category (schema version 2).**
  - `expenses` gains `category TEXT NOT NULL DEFAULT ''`.
  - `load` behaviour by version:
    - 0, an empty file: create the v2 schema.
    - 1: upgrade in one transaction (`ALTER TABLE expenses ADD COLUMN category TEXT NOT NULL DEFAULT ''`, then `user_version = 2`). Roll back and fail on any error.
    - 2: proceed.
    - Anything else: "Saved data is from a newer version."
  - The upgrade only adds a column; no row is changed or removed. Old expenses load as uncategorized.
  - An older build that opens a v2 file refuses to load it and therefore refuses to save. That is safe.
  - Why: existing files keep working, and nothing is lost.

### Constraints for every task

- **Build out of source in `/tmp/mvc-build`** (`cmake -S <repo> -B /tmp/mvc-build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt`). Huy's app is running from `build/` and holds the lock on his real file. Do not rebuild `build/`, and never open, modify or delete anything in `~/Library/Application Support/SCU Student Budget`. Tests use temporary files only.
- Teammates' test files from base `ed49eb0` are never modified. Tests added on this branch may be adjusted by the test author (lifetimes, data or the new display method) without weakening any assertion.
- The layering rules and `tests/CheckLayering.cmake` stay green. No new dependency (QMessageBox is in Qt Widgets, which the app already links).

### Tasks

| # | Task | Role | Model | Files it may touch | Done when | Status |
|---|------|------|-------|--------------------|-----------|--------|
| 12 | Failing tests: Model overlap/replace/restore/expense category; Controller conflict prompt + three choices + expense category; store v1→v2 upgrade, category round trip, duplicate-holding file loads and saves, update survives reload; fake display gains `askBudgetConflict` | team-test-author | sonnet | tests/ (new files + branch-added test files only) | new tests fail only for the missing API; teammates' tests untouched | done |
| 13 | Model + Display + Controller + Store changes | team-implementer | opus | src/BudgetModel.*, src/BudgetDisplay.h, src/BudgetController.*, src/BudgetStore.* | `/tmp/mvc-build` build clean; ctest 100%; doctest 0 failed | done |
| 14 | View: conflict dialog, expense category picker and column; README/REQUIREMENTS lines; offscreen screenshot harness in the scratchpad with a temporary data path | team-implementer | sonnet | src/BudgetView.*, README.md, REQUIREMENTS.md | app builds; ctest 100%; screenshots of the conflict dialog and the expenses table | done |

### Part 3 plan reviews (2026-11-01)

This section covers the Part 3 revision of this plan only.
- **Security (plan mode):** `security:pass`. The single LLM finding (the plan's recorded review history) was filtered at 3/10.
- **Data review:** SOUND. No CRITICAL or HIGH finding, and no data loss possible. The reviewer confirmed on a throwaway v1 file in its scratchpad that the ALTER + `user_version` transaction rolls back cleanly and keeps "12.345" as text. Adopted:

1. **(MEDIUM) Keep a backup before the one-way upgrade.** On the version-1 path, while holding the lock and before the upgrade transaction, run `VACUUM INTO '<path>.v1-backup.sqlite'`, but only if that file does not exist yet. If the backup fails, the load fails and nothing is changed. README says where the backup is. Why: after the upgrade, the old build (the one Huy is running now) can no longer open the file. The backup is the way back.
2. **(MEDIUM) Saves must write the category.** `writeExpenses` INSERTs `category`; `readExpenses` SELECTs it and replays it through `addExpense(text, date, category)`. The round-trip test saves and reloads twice, so a save that drops categories is caught.
3. **(LOW) One transaction for the upgrade and the replay.** On the version-1 path, the upgrade and the row replay share one transaction, which commits only if every row is accepted. Otherwise it rolls back and the file stays v1.
4. **(LOW) New files and upgraded files have the same shape.** A fresh v2 file declares `category TEXT NOT NULL DEFAULT ''` as the last column of `expenses`, exactly like the upgrade. A test compares `PRAGMA table_info(expenses)` for both paths.
5. **(LOW) The D17 rule, stated exactly.** For each j != index with the same category (trimmed, case-insensitive): if the candidate overlaps j and the old budget did not overlap j, reject.
6. **(LOW) Only the store may call `restoreBudget`.** A header comment says it is for the store's load only, and `tests/CheckLayering.cmake` fails if any src file other than BudgetModel.* and BudgetStore.cpp contains `restoreBudget(`.

Destructive steps needing Huy's sign-off: none. The upgrade only adds a column and keeps a backup first.

### Part 3 build progress and review triage (2026-11-01)

- **Task 12 (tests):** 66 new cases in 5 new files. For D22, the schema-version literals in two branch-added test files moved from 1 to 2 and from 2 to 3; no assertion was weakened. Commit 428f637.
- **Task 13:** Model 8f8cfd3, Controller afd4c58, Store 200b094. Out-of-source build in /tmp/mvc-build: 0 warnings; doctest 335/335 cases, 3235 assertions; ctest 2/2.
- **Task 14:** View 2b3de0d, docs 22212ba. An offscreen harness with a temporary data file passed 17/17 checks, and the CTO viewed the screenshots of the three-button dialog and of one $300.00 Meal plan row next to a Groceries expense.
- **Reviews (base 7bc4c71):**
  - Code reviewer: APPROVE, with three MINOR findings and one NIT:
    - The backup-failure path has no test.
    - Nothing tests that `loadSaved` clears a pending question.
    - A crash mid-backup could leave a partial file that is then trusted.
    - NIT: REQUIREMENTS rows.
  - Data review: SOUND. All six plan items confirmed by probing a throwaway v1 file with duplicates. One MEDIUM: an existing backup is trusted without being checked. A 0-byte or partial file, or a backup from an earlier failed upgrade, skips the real backup.
  - clean-check: CLEAN. SHOULD items: `report()` also clears the pending question; the backup and the `restoreBudget` rule were not in Huy's request. NITs: line length, the column definition written twice, the `resolveBudgetConflict` contract comment.
  - Security (diff): no LLM findings; gitleaks found no leaks; no new packages; semgrep p/c returned 0 results.

**Fixing in round 1:**
- **D23. Write the backup safely.**
  - The backup is written to `<backup>.tmp`, then renamed to its final name, so a half-written copy never carries the real name.
  - If `<path>.v1-backup.sqlite` already exists, the new backup goes to `<path>.v1-backup-<yyyyMMdd-HHmmss>.sqlite` instead of being skipped. So every upgrade attempt of a v1 file has a complete, current backup, and an existing backup is never overwritten.
  - A leftover `.tmp` is deleted before writing.
  - Why: the backup is the only way back to the old build, so it must be complete and current.
- **Tests:**
  - A backup failure: the load fails, the file stays v1, there is no backup file and no `.tmp`, and saving is refused.
  - A pre-existing 0-byte backup leads to a new, valid timestamped v1 copy.
  - `loadSaved` clears a pending question.
- **Comment and docs:** the `resolveBudgetConflict` contract comment (true means the choice was carried out; false means nothing was pending or the update failed); the REQUIREMENTS rows.

**Round 1 done:**
- Tests 5cd3720: the backup-failure and `loadSaved` cases already passed; the 0-byte and leftover-`.tmp` cases failed first.
- Fix e776e6b: `.tmp` + rename, and a timestamped name when the plain name exists.
- Docs f19d39e.
- Results: doctest 339/339, 3288 assertions; ctest 2/2; 0 warnings.

### Trial (Part 3)

2026-11-01, base `7bc4c71`, HEAD `f19d39e`. **Verdict: ACCEPT.** All 18 requirements MET.
- **Falsifier: HOLDS.** It ran 17 mutation probes, all caught. In an offscreen harness on a temporary file it showed: Huy's double add leaves one row; the dialog's text and three buttons; Update giving $300 and Nov 15–Dec 15 in place; Cancel and Escape changing nothing; Choose another category keeping the form and moving the window's focus to the category picker; and the expense category column and shared list.
- **Advocate: STRONG.** It ran 14 probes and caught every valid one; the one survivor was a no-op probe.
- **Judge:**
  - It rebuilt the branch and re-ran the harness.
  - It added its own probes: M1, loading through the strict `addBudget`, failed 9 cases. M2, which drops the "only a new conflict counts" rule in `replaceBudget`, failed 2. M3, an update that is not saved, failed 5.
  - Claims audit: BACKED except semgrep, which it did not re-run, and Application Support, which it was not allowed to open.
- **Not verified by anyone:**
  - Real OS keyboard focus on screen.
  - Closing the dialog with the window's X button.
  - Upgrading Huy's real file; only identical temporary v1 files were tested.
- **Observation:** a v1 file that keeps failing to load leaves one more timestamped backup on each launch. That can only happen with a file the Model rejects; Huy's file loads.

### Final check (CTO, Part 3)

Fresh build in /tmp/final-p3.
- Build: 0 warnings or errors.
- ctest: test_build Passed, layering Passed, 100% of 2.
- doctest: 339 test cases and 3288 assertions, 0 failed.
- `test_build` links no QtWidgets.
- No teammate test file from `ed49eb0` was modified. 11 commits since `7bc4c71`; no upstream.

### Retro (Part 3)

- **Agents:**
  - Plan reviews: security 2, data 1.
  - Tests: test-author 2.
  - Building: implementer 3.
  - Code reviews: reviewer 1, clean-code 1, data 1, security 1.
  - Trial: falsifier, advocate and judge.
- **Fix rounds:** 1 of 2 (D23, the safe backup).
- **What went well:** checking the teammates' `addBudget` calls before planning showed the overlap rule was safe. Building out of source left Huy's running app alone. The GUI now runs on this Mac, so the View was checked for real.
- **What I'd brief differently:** ask the test-author for the backup's crash cases up front. The data review predicted them at plan time.

**Kept on purpose:** the backup and the `restoreBudget` layering rule. Both come from the data review's safety findings, not from feature creep. `report()` clearing the pending question is deliberate, because every action ends there.

## Part 4: Delete budgets and expenses (2026-11-01)

Verdict: **GO.** Deleting is small, fits the existing layers, and needs no schema change. Size: STANDARD.

### Decisions

- **D24. Model removal.**
  - New `bool removeBudget(qsizetype index, QString *error = nullptr);`. An out-of-range index gets "Choose a budget to delete.".
  - New `bool removeExpense(qsizetype index, QString *error = nullptr);`. An out-of-range index gets "Choose an expense to delete.".
  - On success, the item is removed, the remaining items keep their order, and `error` is cleared. Removing an expense subtracts its exact amount from total spending (DecimalCents, no double). Removing a budget does not touch expenses; they keep their category text. Totals and remaining follow.
  - On rejection nothing changes.
  - Why: the Model owns the data and its rules, and knows nothing about confirmation boxes.
- **D25. Shown row versus Model item (the identity bug).**
  - The expenses table is shown newest first, so the row the user clicks is not the Model's index. The controller keeps one private helper that gives Model indices in display order (date descending, then later entry first). Both `screen()` and the delete requests use it, so the mapping cannot drift from what is shown.
  - Budgets are shown in Model order today. They still go through the same kind of helper, so a future sort cannot break deletion.
  - At request time the controller records the Model index, not the row. Any other action clears the pending request, so the index can't go stale.
  - Why: deleting the wrong expense would be silent data loss.
- **D26. Confirmation through the display.**
  - `BudgetDisplay.h` gains `enum class DeleteChoice { Delete, Cancel };` and `virtual void askDeleteConfirmation(const QString &message) = 0;`.
  - Controller:
    - `bool requestDeleteBudget(int shownRow);` and `bool requestDeleteExpense(int shownRow);` check the row against what is shown. A bad row shows the D24 message as an error, records nothing pending, and returns false. Otherwise they store the pending delete and ask, returning true.
    - `bool resolveDelete(DeleteChoice choice);`:
      - Nothing pending: false, nothing happens.
      - Cancel: nothing changes, nothing is saved; shows ("Nothing was deleted.", false) and returns true.
      - Delete: calls `removeBudget` or `removeExpense`, then saves and pushes the screen through `report()` with "Budget deleted." or "Expense deleted.".
    - The pending delete and the pending budget conflict are mutually exclusive: starting either clears the other, and `report()` and `loadSaved` clear both.
  - Messages:
    - "Delete the <category> budget of <$amount> for <start> to <end>?"
    - "Delete the <category> expense of <$amount> on <date>?" An uncategorized expense reads "Delete the uncategorized expense of <$amount> on <date>?".
  - Why: this is the same pattern as D19, where the View only reports the user's answer to the Controller.
- **D27. The View.**
  - Add "Delete budget" and "Delete expense" buttons, each disabled until a row in its table is selected.
  - Both tables use single-row selection. The selection clears when the screen refreshes, and the buttons disable again.
  - Clicking passes the selected row to `requestDelete…`.
  - `askDeleteConfirmation` queues a QMessageBox with "Delete" and "Cancel"; Cancel is the default button, and Escape or closing the box counts as Cancel. It then calls `resolveDelete`.
  - No undo and no multi-select.
- **D28. Persistence.** No schema change. A delete saves through the existing full rewrite, which renumbers `position` from the Model's order, so positions stay consistent after a middle row goes. Tests:
  - A delete survives a reload.
  - Deleting a middle row keeps the remaining order after a reload.
  - Deleting one of two identical Meal plan budgets leaves exactly one, also after a reload.
- **D29. Constraints.** The same as Part 3:
  - Build only out of source (`/tmp/mvc-build`). Never rebuild `build/`, where Huy's app runs as pid 30206, and never touch `~/Library/Application Support`. Tests use temporary files only.
  - Teammate tests from `ed49eb0` are never modified. The layering check stays green. No new dependency.
- **D30. Wording.** The strings above are defaults (Open Question 6).

### Tasks

| # | Task | Role | Model | Files it may touch | Done when | Status |
|---|------|------|-------|--------------------|-----------|--------|
| 15 | Failing tests for delete (cases listed below the table) | team-test-author | sonnet | tests/ (new files + branch-added files only) | new tests fail only for the missing API | done |
| 16 | Model + Display + Controller | team-implementer | opus | src/BudgetModel.*, src/BudgetDisplay.h, src/BudgetController.* | test_build 0 warnings, doctest 0 failed, ctest 100% | done |
| 17 | View buttons, enable/disable and the confirmation box; README/REQUIREMENTS lines; offscreen harness on a temporary file with screenshots | team-implementer | sonnet | src/BudgetView.*, README.md, REQUIREMENTS.md | app builds; ctest 100%; harness PASS; screenshots | done |

Task 15 covers:
- Model removal: a valid delete, an invalid index, a middle row, exact spending after removal, and a budget delete that keeps expenses.
- Controller with the fake display: a request asks; Delete deletes; Cancel doesn't; a bad row is an error; newest-first order maps to the right expense; the delete prompt and the conflict prompt exclude each other.
- Persistence: a round trip, a middle row, and the duplicate Meal plan case.
- The fake display gains `askDeleteConfirmation`.

### Part 4 plan reviews (2026-11-01)

This section covers the Part 4 revision only.
- **Security (plan mode):** `security:pass`. The LLM's only finding was the recurring L1 (the plan's recorded review history), filtered at 3/10.
- **Data review:** SOUND. No CRITICAL or HIGH finding. The reviewer confirmed:
  - The newest-first order is strict, including same-date ties.
  - Spending stays exact after a removal.
  - The full-rewrite save renumbers positions with no gaps.
  - After a failed load there is nothing to delete, and saving stays refused.

Adopted (these override D26 and the task 15 cases where they differ):
1. **(MEDIUM) No second request while a delete is pending.** `requestDelete…` refuses a new request while one is pending: it returns false, shows nothing new, and keeps the first. The pending delete also keeps a copy of the item it names. `resolveDelete(Delete)` removes the item only if the item at the stored index still matches that copy; otherwise it removes nothing and shows ("Nothing was deleted.", false). Test: two requests, then Delete, removes only the first item named.
2. **(MEDIUM) Tests that can tell twins apart.** The duplicate test uses Huy's real case: Meal plan $50 and $250, Nov 1–Dec 1, 2026. It deletes the $50 one and checks that the $250 one remains, also after a reload. The newest-first test uses two same-date expenses with different amounts, plus an older-dated expense entered last.
3. **(LOW) A stale answer still gets feedback.** `resolveDelete` with nothing pending shows ("Nothing was deleted.", false) and returns false.
4. **(LOW) Reloading clears a pending delete.** `loadSaved` and `report()` clear both pending requests. Test: request, `loadSaved`, then Delete does nothing.

Destructive steps needing Huy's sign-off: none beyond the confirmation dialog. There is no schema change. Delete has no undo, and a v2 file gets no automatic backup. **Huy to run before cleaning up his real file** (a one-time copy): `cp ~/Library/Application\ Support/SCU\ Student\ Budget/budget.sqlite ~/Desktop/budget-before-delete.sqlite`

Open Question 6 (wording, default taken):
- "Choose a budget to delete." and "Choose an expense to delete."
- "Delete the … budget of … for … to …?" and "Delete the … expense of … on …?"
- "Budget deleted.", "Expense deleted." and "Nothing was deleted."
- The buttons "Delete budget", "Delete expense", "Delete" and "Cancel".

### Part 4 build progress and review triage (2026-11-01)

- **Task 15 (tests), ca52325:** 60 cases in tests/test_delete_model.cpp, test_delete_controller.cpp and test_delete_persistence.cpp. The fake display gained `askDeleteConfirmation`.
- **Task 16:** Model ad2c950, Controller 3fe96ea. The shown-order helpers `shownBudgetOrder()` and `shownExpenseOrder()` are used by both `screen()` and the delete requests. doctest 399/399, 3945 assertions; 0 warnings; no store change was needed.
- **Task 17:** View 159b19c, docs d64f118. The offscreen harness on a temporary file (scratchpad `p4shot/`) passed 26/26:
  - The buttons are disabled with no selection and after a refresh.
  - The box names "$50.00 for Nov 1, 2026 to Dec 1, 2026", with Cancel as the default. Cancel changes nothing.
  - Delete leaves one Meal plan row at $250.00.
  - The $100.00 expense, shown at row 1 rather than row 0, is the one removed.
  - The deletes survive reopening the file.
  - The CTO viewed delete-confirm.png and view.png.
- **Reviews (base a118766):**
  - Code reviewer: APPROVE, with three NITs: a return value not asserted in two stale-copy tests; the REQUIREMENTS row could mention that seven-day recovery isn't built; and the plan statuses, now fixed here.
  - Data review: SOUND. All four plan items were confirmed. Probes through the real store: a reversed twin order deleted the named $50; a newest-first delete removed the right $100 among two $100s; spending moved exactly from $219.845 to $119.845; positions renumbered 0..n-1. One LOW: a failed save after a delete leaves the item in the file. The existing "Your change was not saved" warning covers this, as it does for every action.
  - clean-check: CLEAN. SHOULD items: drop the "still the same item" guard (E2), and rename the `PendingDelete` fields (N1).
  - Security (diff): no LLM findings; gitleaks found no leaks; no new packages; semgrep p/c returned 0 results.
- **No fix round.** All findings are optional.
  - The E2 suggestion is declined on purpose. The guard is the data review's adopted item 1. It costs a few lines, and it keeps a future change (say, a sorted table or a non-modal box) from deleting an item other than the one the box names.
  - The NITs and N1 are left as follow-ups.

### Trial (Part 4)

2026-11-01, base `a118766`.
- **Falsifier (at 5c3a140): INCONCLUSIVE.** Nothing was falsified, and every mutation was caught. It found one plan deviation: after deleting the bottom row, Qt re-highlighted the row above and the delete button stayed enabled, contradicting D27.
- **Fix round 1, 35285cb:** clear the selection model after the table refill. Probe case B went from `selectedRows=1 ... buttonEnabled=1` to `selectedRows=0 ... buttonEnabled=0`. The p4shot harness still passed 26/26.
- **Advocate (at 5c3a140): STRONG.** About 9 mutations, all caught.
- **Judge (at 35285cb): INSUFFICIENT EVIDENCE**, on one item only: "don't touch Huy's real file". The team is forbidden to look at that file, so only a code trace exists (AppDataLocation is used only by src/main.cpp; every test, harness and probe uses QTemporaryDir).
  - Every other requirement is MET. The judge re-ran probe case B at HEAD, the harness (26/26), and two of its own mutations (row mapping: 5 tests failed; twin: 4 tests failed).
  - Claims audit: all BACKED, except the semgrep and LLM parts (not re-run) and the real file (not checkable).
- **Not verified: Huy to check.** His real file should still hold both Meal plan budgets ($50 and $250, Nov 1–Dec 1, 2026) and the $100 expense. No agent can run this check, so per the trial procedure it ships as an open item rather than another round.
- **Stray file.** An untracked, empty schema-v2 SQLite file named `-s` appeared in the repo root at 20:42, during the review agents' runs. Its origin is unproven. It is not committed; everything is staged by explicit path. Huy should delete it, and nobody should run `git add -A` while it exists.

### Final check (CTO, Part 4)

Fresh build in /tmp/final-p4.
- Build: 0 warnings or errors.
- ctest: test_build and layering Passed, 100% of 2.
- doctest: 399 test cases and 3945 assertions, 0 failed.
- No teammate test file from `ed49eb0` was modified. 9 commits since `a118766`; no upstream.

### Retro (Part 4)

- **Agents:**
  - Plan reviews: security 2, data 1.
  - Tests: test-author 1.
  - Building: implementer 3.
  - Code reviews: reviewer 1, clean-code 1, data 1, security 1.
  - Trial: falsifier, advocate and judge.
- **Fix rounds:** 1 (the bottom-row selection).
- **What went well:** the data review's "twins must be distinguishable" item made the tests able to catch a wrong-row delete, which the mutations then proved.
- **What I'd brief differently:**
  - Ask the View implementer's harness to delete the bottom row specifically. It's the edge case where Qt moves the selection.
  - Tell every agent the exact data path its harness may write, so stray files like `-s` can't appear.
