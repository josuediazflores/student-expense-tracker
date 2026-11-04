#include "BudgetController.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "BudgetView.h"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>

// Composition root: the only place that sees every layer. Keep this order: objects are
// destroyed in reverse, so the store and the view outlive every controller call that uses
// them, and the view is destroyed before the controller that points to it.
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    // The application name picks the per-user data folder below.
    QCoreApplication::setApplicationName(QStringLiteral("SCU Student Budget"));
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    // If the folder cannot be made, opening the file fails and loadSaved() shows the error.
    QDir().mkpath(dataDir);

    BudgetStore store(dataDir + QStringLiteral("/budget.sqlite"));
    BudgetModel model;
    BudgetController controller(model);
    BudgetView view(controller);
    controller.setStore(&store);
    controller.setDisplay(&view);
    // A failed load is shown to the student, and the store then refuses to overwrite the file.
    controller.loadSaved();
    view.show();
    return app.exec();
}
