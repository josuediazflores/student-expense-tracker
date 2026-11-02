// The duplicate-budget guard in the Model. addBudget rejects a same-category overlap and says which
// budget it clashes with, replaceBudget swaps a budget in place, and restoreBudget keeps data
// that was saved before the rule existed.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
const QString kOverlap = QStringLiteral("A budget for this category already covers some of those dates.");
const QString kBadIndex = QStringLiteral("Choose a budget to update.");
const QString kNegative = QStringLiteral("Budget amount cannot be negative.");
const QString kTooLarge = QStringLiteral("Total budget is too large.");
const qint64 kMax = std::numeric_limits<qint64>::max();

const QDate kOct1(2026, 10, 1);
const QDate kOct31(2026, 10, 31);
const QDate kNov1(2026, 11, 1);
const QDate kNov10(2026, 11, 10);
const QDate kNov15(2026, 11, 15);
const QDate kNov20(2026, 11, 20);
const QDate kNov25(2026, 11, 25);
const QDate kNov30(2026, 11, 30);
const QDate kDec1(2026, 12, 1);
const QDate kDec2(2026, 12, 2);
const QDate kDec15(2026, 12, 15);
const QDate kDec31(2026, 12, 31);

BudgetModel withMealPlan() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
    return model;
}

void checkOnlyMealPlan(const BudgetModel &model) {
    REQUIRE(model.budgets().size() == 1);
    CHECK(model.budgets().at(0).category == QStringLiteral("Meal plan"));
    CHECK(model.budgets().at(0).amountCents == 25000);
    CHECK(model.budgets().at(0).startDate == kNov1);
    CHECK(model.budgets().at(0).endDate == kDec1);
    CHECK(model.totalCents() == 25000);
}

// Adds `candidate` to a model holding only the Meal plan and expects the overlap rejection.
void checkOverlapRejected(const Budget &candidate) {
    BudgetModel model = withMealPlan();
    QString error;
    qsizetype conflict = -7;

    CHECK_FALSE(model.addBudget(candidate, &error, &conflict));

    CHECK(error == kOverlap);
    CHECK(conflict == 0);
    checkOnlyMealPlan(model);
}

void checkOverlapAllowed(const Budget &candidate) {
    BudgetModel model = withMealPlan();
    QString error = QStringLiteral("Previous error");
    qsizetype conflict = -7;

    CHECK(model.addBudget(candidate, &error, &conflict));

    CHECK(error.isEmpty());
    CHECK(conflict == -1);
    CHECK(model.budgets().size() == 2);
}
}

// ---- addBudget: what counts as an overlap (D15, D16) ---------------------------------

TEST_CASE("a same-category budget with an overlapping range is rejected with the overlap message") {
    checkOverlapRejected({"Meal plan", 10000, kNov15, kDec15});
}

TEST_CASE("a same-category budget with identical dates is rejected") {
    checkOverlapRejected({"Meal plan", 25000, kNov1, kDec1});
}

TEST_CASE("sharing a single day counts as an overlap, in either order") {
    checkOverlapRejected({"Meal plan", 10000, kDec1, kDec31});

    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 10000, kDec1, kDec31}));
    QString error;
    qsizetype conflict = -7;
    CHECK_FALSE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}, &error, &conflict));
    CHECK(error == kOverlap);
    CHECK(conflict == 0);
}

TEST_CASE("a budget inside, around or on one day of an existing one is an overlap") {
    checkOverlapRejected({"Meal plan", 100, kNov10, kNov20});    // inside
    checkOverlapRejected({"Meal plan", 100, kOct1, kDec31});     // around
    checkOverlapRejected({"Meal plan", 100, kNov15, kNov15});    // one day inside
    checkOverlapRejected({"Meal plan", 100, kOct31, kNov1});     // touches the first day
}

TEST_CASE("the same category is allowed when the dates do not overlap") {
    checkOverlapAllowed({"Meal plan", 10000, kDec2, kDec31});  // the day after it ends
    checkOverlapAllowed({"Meal plan", 10000, kOct1, kOct31});  // the day before it starts
}

TEST_CASE("a different category on the same dates is allowed") {
    checkOverlapAllowed({"Transport", 10000, kNov1, kDec1});
}

TEST_CASE("the category match ignores case and surrounding spaces") {
    checkOverlapRejected({"  meal PLAN ", 10000, kNov15, kDec15});
    checkOverlapRejected({"MEAL PLAN", 10000, kNov1, kDec1});
}

TEST_CASE("conflictIndex is the index of the budget it clashes with, not always zero") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 100, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Transport", 100, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
    QString error;
    qsizetype conflict = -7;

    CHECK_FALSE(model.addBudget({"Meal plan", 10000, kNov15, kDec15}, &error, &conflict));

    CHECK(conflict == 2);
    CHECK(model.budgets().size() == 3);
}

TEST_CASE("when several budgets overlap, conflictIndex is the first one") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 100, kNov1, kNov10}));
    REQUIRE(model.addBudget({"Meal plan", 200, kNov20, kNov30}));
    QString error;
    qsizetype conflict = -7;

    CHECK_FALSE(model.addBudget({"Meal plan", 300, kNov10, kNov25}, &error, &conflict));

    CHECK(error == kOverlap);
    CHECK(conflict == 0);
}

// ---- addBudget: existing rules still win, and conflictIndex is -1 otherwise ----------

TEST_CASE("an overlapping negative budget gets the negative message, not the overlap message") {
    BudgetModel model = withMealPlan();
    QString error;
    qsizetype conflict = -7;

    CHECK_FALSE(model.addBudget({"Meal plan", -1, kNov15, kDec15}, &error, &conflict));

    CHECK(error == kNegative);
    CHECK(conflict == -1);
    checkOnlyMealPlan(model);
}

TEST_CASE("an overlapping budget that overflows the total gets the overflow message") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", kMax - 100, kNov1, kDec1}));
    QString error;
    qsizetype conflict = -7;

    CHECK_FALSE(model.addBudget({"Meal plan", 200, kNov15, kDec15}, &error, &conflict));

    CHECK(error == kTooLarge);
    CHECK(conflict == -1);
}

TEST_CASE("conflictIndex is -1 for every other rejection and on success") {
    BudgetModel model = withMealPlan();
    QString error;
    qsizetype conflict = 42;

    CHECK_FALSE(model.addBudget({"Food", 100, kDec15, kNov15}, &error, &conflict));
    CHECK(error == QStringLiteral("End date cannot be before start date."));
    CHECK(conflict == -1);

    conflict = 42;
    CHECK_FALSE(model.addBudget({"Food", 100, QDate(), kNov15}, &error, &conflict));
    CHECK(conflict == -1);

    conflict = 42;
    CHECK_FALSE(model.addBudget({"   ", 100, kNov15, kDec15}, &error, &conflict));
    CHECK(error == QStringLiteral("Choose a category."));
    CHECK(conflict == -1);

    conflict = 42;
    CHECK(model.addBudget({"Food", 100, kNov15, kDec15}, &error, &conflict));
    CHECK(conflict == -1);
}

TEST_CASE("an overlap is rejected without out-parameters and leaves the model unchanged") {
    BudgetModel model = withMealPlan();

    CHECK_FALSE(model.addBudget({"Meal plan", 10000, kNov15, kDec15}));
    CHECK_FALSE(model.addBudget({"Meal plan", 10000, kNov15, kDec15}, nullptr, nullptr));

    checkOnlyMealPlan(model);
}

// ---- replaceBudget (D17) -------------------------------------------------------------

TEST_CASE("replaceBudget replaces the amount and dates in place") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 100, kOct1, kOct31}));
    REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Transport", 700, kNov1, kDec1}));
    REQUIRE(model.addExpense("5", QDate(2026, 11, 2)));
    QString error = QStringLiteral("Previous error");

    CHECK(model.replaceBudget(1, {"Meal plan", 30000, kNov15, kDec15}, &error));

    CHECK(error.isEmpty());
    REQUIRE(model.budgets().size() == 3);
    CHECK(model.budgets().at(0).category == QStringLiteral("Food"));
    CHECK(model.budgets().at(1).category == QStringLiteral("Meal plan"));
    CHECK(model.budgets().at(1).amountCents == 30000);
    CHECK(model.budgets().at(1).startDate == kNov15);
    CHECK(model.budgets().at(1).endDate == kDec15);
    CHECK(model.budgets().at(2).category == QStringLiteral("Transport"));
    CHECK(model.budgets().at(2).amountCents == 700);
    CHECK(model.totalCents() == 30800);
    CHECK(model.expenses().size() == 1);
}

TEST_CASE("replaceBudget with an index outside the list is rejected") {
    BudgetModel model = withMealPlan();
    const Budget fine{"Meal plan", 100, kNov1, kDec1};

    for (qsizetype index : {qsizetype(-1), qsizetype(1), qsizetype(5)}) {
        QString error;
        CHECK_FALSE(model.replaceBudget(index, fine, &error));
        CHECK(error == kBadIndex);
    }
    checkOnlyMealPlan(model);
}

TEST_CASE("replaceBudget applies the same amount, date and category rules as addBudget") {
    BudgetModel model = withMealPlan();
    QString error;

    CHECK_FALSE(model.replaceBudget(0, {"Meal plan", -1, kNov1, kDec1}, &error));
    CHECK(error == kNegative);
    CHECK_FALSE(model.replaceBudget(0, {"Meal plan", 100, QDate(), kDec1}, &error));
    CHECK(error == QStringLiteral("Choose valid start and end dates."));
    CHECK_FALSE(model.replaceBudget(0, {"Meal plan", 100, kDec1, kNov1}, &error));
    CHECK(error == QStringLiteral("End date cannot be before start date."));
    CHECK_FALSE(model.replaceBudget(0, {"  ", 100, kNov1, kDec1}, &error));
    CHECK(error == QStringLiteral("Choose a category."));

    checkOnlyMealPlan(model);
}

TEST_CASE("replaceBudget checks overflow as total minus old plus new") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Small", 100, kNov1, kDec1}));
    QString error;

    // Replacing 100 with 100 keeps the total at the maximum, so it must be allowed.
    CHECK(model.replaceBudget(1, {"Small", 100, kNov1, kDec1}, &error));
    CHECK(model.totalCents() == kMax);

    CHECK_FALSE(model.replaceBudget(1, {"Small", 101, kNov1, kDec1}, &error));
    CHECK(error == kTooLarge);
    CHECK(model.budgets().at(1).amountCents == 100);
    CHECK(model.totalCents() == kMax);
}

TEST_CASE("replaceBudget rejects dates that newly overlap another budget of the same category") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Meal plan", 9000, QDate(2027, 1, 1), QDate(2027, 1, 31)}));
    QString error;

    CHECK_FALSE(model.replaceBudget(1, {"Meal plan", 9000, kDec1, kDec31}, &error));

    CHECK(error == kOverlap);
    CHECK(model.budgets().at(1).startDate == QDate(2027, 1, 1));
    CHECK(model.budgets().at(1).endDate == QDate(2027, 1, 31));
}

TEST_CASE("replaceBudget is not blocked by itself or by other categories on the same dates") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Transport", 700, kNov1, kDec1}));

    CHECK(model.replaceBudget(0, {"Meal plan", 26000, kNov1, kDec1}));
    CHECK(model.replaceBudget(0, {"Meal plan", 26000, kNov10, kDec15}));

    CHECK(model.budgets().at(0).amountCents == 26000);
}

TEST_CASE("with two identical restored duplicates, replacing row 0 succeeds") {
    BudgetModel model;
    REQUIRE(model.restoreBudget({"Meal plan", 25000, kNov1, kDec1}));
    REQUIRE(model.restoreBudget({"Meal plan", 25000, kNov1, kDec1}));
    QString error;

    // Row 1 overlapped the old row 0 already, so the replacement creates no new conflict.
    CHECK(model.replaceBudget(0, {"Meal plan", 30000, kNov1, kDec1}, &error));
    CHECK(error.isEmpty());
    CHECK(model.replaceBudget(0, {"Meal plan", 30000, kNov15, kDec15}, &error));

    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).amountCents == 30000);
    CHECK(model.budgets().at(0).startDate == kNov15);
    CHECK(model.budgets().at(1).amountCents == 25000);
    CHECK(model.budgets().at(1).startDate == kNov1);
}

// ---- restoreBudget (D18, review item 6) ----------------------------------------------

TEST_CASE("restoreBudget accepts a duplicate that addBudget would reject") {
    BudgetModel model = withMealPlan();
    QString error = QStringLiteral("Previous error");

    CHECK(model.restoreBudget({"Meal plan", 25000, kNov1, kDec1}, &error));

    CHECK(error.isEmpty());
    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).amountCents == 25000);
    CHECK(model.budgets().at(1).amountCents == 25000);
    CHECK(model.budgets().at(1).startDate == kNov1);
    CHECK(model.budgets().at(1).endDate == kDec1);
    CHECK(model.totalCents() == 50000);
}

TEST_CASE("restoreBudget rejects a negative amount and leaves the list unchanged") {
    BudgetModel model = withMealPlan();
    QString error;

    CHECK_FALSE(model.restoreBudget({"Meal plan", -1, kNov15, kDec15}, &error));

    CHECK(error == kNegative);
    checkOnlyMealPlan(model);
}

TEST_CASE("restoreBudget still applies the date, category and overflow rules") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kNov1, kDec1}));
    QString error;

    CHECK_FALSE(model.restoreBudget({"Big", 100, QDate(), kDec1}, &error));
    CHECK(error == QStringLiteral("Choose valid start and end dates."));
    CHECK_FALSE(model.restoreBudget({"Big", 100, kDec1, kNov1}, &error));
    CHECK(error == QStringLiteral("End date cannot be before start date."));
    CHECK_FALSE(model.restoreBudget({" ", 100, kNov1, kDec1}, &error));
    CHECK(error == QStringLiteral("Choose a category."));
    CHECK_FALSE(model.restoreBudget({"Other", 101, kNov1, kDec1}, &error));
    CHECK(error == kTooLarge);
    CHECK(model.budgets().size() == 1);
}
