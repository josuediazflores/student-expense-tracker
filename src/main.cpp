#include "BudgetController.h"
#include "BudgetView.h"

#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    BudgetModel model;
    BudgetController controller(model);
    BudgetView view(controller);
    view.show();
    return app.exec();
}
