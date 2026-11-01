#include "BudgetController.h"
#include "BudgetModel.h"
#include "BudgetView.h"

#include <QApplication>

// Composition root: the only place that sees every layer. Keep this order, because the
// view is destroyed before the controller and the controller holds a pointer to it.
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    BudgetModel model;
    BudgetController controller(model);
    BudgetView view(controller);
    controller.setDisplay(&view);
    view.show();
    return app.exec();
}
