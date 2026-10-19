#include "BudgetController.h"

bool BudgetController::createBudget(const QString &category, qint64 amountCents,
                                    const QDate &startDate, const QDate &endDate,
                                    QString *error) {
    return model_.addBudget({category, amountCents, startDate, endDate}, error);
}
