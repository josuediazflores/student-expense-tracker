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

## Verdict (CTO, Phase 0)

GO WITH CHANGES. The proposal is right: the skeleton's include direction is correct, but the Controller exposes nothing about expenses, the View reaches into Model types (`Budget`, and calls `Budget::amountPerDay()`, a Model method, directly), and money goes through a `double`. Changes from the literal MVP: persistence (MVP requirement 3, last bullet) is left out of this slice and only its seam is designed (D1), and the skeleton's category budgets are kept rather than collapsed to one overall budget (D2), so teammates' tests stay untouched. Branch `huy/mvc-mvp` is cut from `origin/main` (ed49eb0), not from `huy/spec-decimal-cents`: that branch only changes SPECIFICATIONS.md (open PR #7), and the code does not need it. Local `main` is one commit behind `origin/main`, so `origin/main` is the base.

## Goal

A student can open the app, add or update a budget amount typed as dollar text, log an expense with an amount and a date (today by default), and immediately see total budget, total spending, remaining budget (negative shown as `-$…`), and a newest-first list of expenses. The code follows strict MVC. The Model knows nothing about the controller, view or display interface. The View talks only to the Controller and cannot name Model types. The Controller mediates and pushes view-ready strings through an abstract `BudgetDisplay`. These rules are enforced by CMake target boundaries and by a ctest layering check that runs in CI.

## MVP scope

In (REQUIREMENTS.md "MVP requirements"; SPECIFICATIONS.md "Planned MVP screen"):
- Req 1, create and edit a budget: enter a budget amount as text; update an existing budget's amount; totals and remaining recalculate.
- Req 2, log spending: amount + date (`QDateEdit` defaults to today); clear message for an invalid amount or date; totals update on save.
- Req 3, view: total budget, total spending, remaining (minus sign when negative), and an expenses table (Date, Amount), newest first.

Out:
- Req 3's last bullet, persistence across restarts. It is MVP but a separate slice (D1). Only the seam is described below.
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
  - "Update selected budget" calls `controller_.updateBudgetAmount(budgetsTable_->currentRow(), budgetAmount_->text())`. No selection means row −1, and the model says "Choose a budget to update.".
  - "Save expense" calls `controller_.addExpense(expenseAmount_->text(), expenseDate_->date())` and clears the amount on `true`.
- Labels: "Total budget: %1", "Total spending: %1", "Remaining budget: %1".
- `showMessage` sets `feedback_` text, red (`color: #b3261e`) when `isError`.
- Tables are not editable; budgets use single-row selection.
- The constructor no longer pulls data, because `setDisplay` pushes the first screen.
- No `QValidator`, no `QLocale` money maths, no `<cmath>`, and no Model types.

### `src/main.cpp` (composition root; the only non-test file allowed to see every layer)

`QApplication app; BudgetModel model; BudgetController controller(model); BudgetView view(controller); controller.setDisplay(&view); view.show(); return app.exec();`. It now includes `BudgetModel.h` explicitly.

### Persistence seam (not built here)

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

1. **Targets** (CMakeLists.txt). The `src/*.cpp` glob is replaced:
   - `add_library(budget_model STATIC src/BudgetModel.cpp src/DecimalCents.cpp)`: PUBLIC include dir `src`, PUBLIC link `Qt6::Core` only.
   - `add_library(budget_controller STATIC src/BudgetController.cpp)`: PUBLIC link `budget_model Qt6::Core`.
   - `add_executable(app src/main.cpp src/BudgetView.cpp)`: PRIVATE link `budget_controller Qt6::Widgets Qt6::Sql`.
   - Under `BUILD_TESTING`: keep `file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)` and `add_executable(test_build ${TEST_SOURCES})`, PRIVATE link `budget_controller Qt6::Core`, include `tests`, `add_test(NAME test_build ...)`. Add `add_test(NAME layering COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -P ${CMAKE_SOURCE_DIR}/tests/CheckLayering.cmake)`.
   - The result: if the Model or Controller uses a Widgets symbol, `test_build` fails to link.
2. **`tests/CheckLayering.cmake`** (no new dependency):
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
3. **Controller tests.** `tests/test_budget_controller.cpp` uses a fake `BudgetDisplay` that appends every `BudgetScreen` and `(text, isError)` pair to vectors. It does not include Widgets.

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

- [ ] 1. Write failing model tests (T1).
- [ ] 2. Write failing controller tests with a fake display (T2).
- [ ] 3. Write the layering check; confirm it fails on today's tree (T3).
- [ ] 4. Restructure CMake into `budget_model` / `budget_controller` / `app` and register `layering` (T4).
- [ ] 5. Implement Model additions (T5).
- [ ] 6. Add `BudgetDisplay.h`; rewrite the controller; model, controller and layering tests go green (T6).
- [ ] 7. Rewrite the View and `main.cpp`; full build, all tests, offscreen smoke (T7).
- [ ] 8. Update README.md / REQUIREMENTS.md (T8).
- [ ] 9. Independent verification, including mutation probes (T9).

## Tasks

T1, T2, T3 and T4 run in parallel (disjoint files). T5 needs T1 + T4. T6 needs T2 + T3 + T5. T7 needs T6. T8 may run in parallel with T5 to T7, but its text must match T7's result. T9 runs last.

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
- There are no security or data-migration implications. Data stays in memory (D1).

## Open Questions

1. **Future-dated expenses.** Should the app reject them? *Default taken: accept them (no rule is invented). A later rule would be one more check after the date-validity check.*
2. **New user-facing wording.**
   - Messages: "Budget amount updated.", "Expense saved.", "Choose a budget to update.", "Choose a valid expense date.", "Budget amount is required.", "Budget amount must be in whole cents.", "Budget amount is too large.".
   - Labels: "Total budget:", "Total spending:", "Remaining budget:".
   - Button: "Update selected budget".

   *Default taken: use them as written; changing any one is a single string plus its test.*

## Follow-ups

- After PR #7 (`huy/spec-decimal-cents`) merges, update SPECIFICATIONS.md:
  - Architecture table: `BudgetDisplay`, targets, and the fact that test_build now links the controller.
  - Expense record and `updateBudgetAmount` / `parseBudgetAmount` / dated `addExpense` rules with exact messages.
  - The "Current screen" section.
  - Testing table rows for the four new test files.
- Persistence slice (D1, seam above), including a `BudgetStore` rule in `tests/CheckLayering.cmake`.
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
- Scope: this matches the agreed proposal. Persistence is deliberately left for its own slice (D1).
- Execution change from the Tasks table: T1–T3 go to one test-author run before any implementation. T4–T6 then go to one implementer run (opus, because it is the data model and the layer contract), committed as three separate commits. T7–T8 go to a second implementer run, committed separately. All of these run one after another, because the agents share one `build/` directory.

## Task Ledger

Branch: `huy/mvc-mvp` (local only; no upstream set). Base commit: `ed49eb0` (origin/main).

| # | Task | Role | Model | Files it may touch | Output | Done when | Depends on | Status |
|---|------|------|-------|--------------------|--------|-----------|-----------|--------|
| 1 | Model tests (update, parse, dated expenses) | team-test-author | sonnet | tests/test_budget_update.cpp, tests/test_budget_amount_text.cpp, tests/test_expense_records.cpp | failing tests | DW1 | – | pending |
| 2 | Controller tests with fake display | team-test-author | sonnet | tests/test_budget_controller.cpp | failing tests | DW2 | – | pending |
| 3 | Layering check | team-test-author | sonnet | tests/CheckLayering.cmake | check that fails on today's tree | DW3 | – | pending |
| 4 | CMake targets + layering test | team-implementer | opus | CMakeLists.txt | build config | DW4 | 3 | pending |
| 5 | Model additions | team-implementer | opus | src/BudgetModel.h, src/BudgetModel.cpp | code | DW5 | 1, 4 | pending |
| 6 | BudgetDisplay.h + controller | team-implementer | opus | src/BudgetDisplay.h, src/BudgetController.h, src/BudgetController.cpp | code | DW6 | 2, 3, 5 | pending |
| 7 | View + main | team-implementer | sonnet | src/BudgetView.h, src/BudgetView.cpp, src/main.cpp | code | DW7 | 6 | pending |
| 8 | README / REQUIREMENTS status | team-implementer | sonnet | README.md, REQUIREMENTS.md | docs | DW8 | 7 | pending |
| 9 | Independent verification | team-verifier | sonnet | none | report | DW9 | 1–8 | pending |

## Progress Log

- 2026-11-01 plan: written by team-architect; security review of the plan returned `security:pass`; self-approved (see Plan Review).
