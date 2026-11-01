// Part 3, task 12: tests/CheckLayering.cmake must fail if any src file other than
// BudgetModel.h, BudgetModel.cpp and BudgetStore.cpp contains "restoreBudget(" (plan review
// item 6). The script is run on a throwaway copy of src/ with the text planted, so nothing in
// the repository is changed. Needs the cmake executable; the test fails if it cannot be found.
#include "doctest.h"

#include "persistence_test_support.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

using namespace persistence_test;

namespace {
QString repoRoot() {
    return QDir(QFileInfo(QStringLiteral(__FILE__)).absolutePath()).absoluteFilePath(QStringLiteral(".."));
}

QString cmakeExecutable() {
    const QString fromEnv = qEnvironmentVariable("CMAKE_COMMAND");
    if (!fromEnv.isEmpty()) return fromEnv;
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("cmake"));
    if (!onPath.isEmpty()) return onPath;
    for (const QString &candidate : {QStringLiteral("/opt/homebrew/bin/cmake"),
                                     QStringLiteral("/usr/local/bin/cmake")}) {
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return QString();
}

struct CheckResult {
    int exitCode = -1;
    QString output;
};

CheckResult runLayeringCheck(const QString &sourceDir) {
    ensureApp();
    const QString cmake = cmakeExecutable();
    REQUIRE_MESSAGE(!cmake.isEmpty(), "cmake was not found, so CheckLayering.cmake cannot be run");
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(cmake, {QStringLiteral("-DSOURCE_DIR=") + sourceDir, QStringLiteral("-P"),
                          QDir(repoRoot()).absoluteFilePath(QStringLiteral("tests/CheckLayering.cmake"))});
    REQUIRE(process.waitForFinished(60000));
    return {process.exitCode(), QString::fromUtf8(process.readAll())};
}

// A copy of every file in the repository's src/, in a temporary tree.
void copySrc(const QTemporaryDir &tree) {
    REQUIRE(QDir(tree.path()).mkpath(QStringLiteral("src")));
    const QDir source(QDir(repoRoot()).absoluteFilePath(QStringLiteral("src")));
    for (const QString &name : source.entryList(QDir::Files)) {
        REQUIRE(QFile::copy(source.absoluteFilePath(name),
                            QDir(tree.path()).absoluteFilePath(QStringLiteral("src/") + name)));
    }
}

void plantText(const QTemporaryDir &tree, const QString &file, const QByteArray &text) {
    QFile out(QDir(tree.path()).absoluteFilePath(QStringLiteral("src/") + file));
    REQUIRE(out.open(QIODevice::WriteOnly | QIODevice::Append));
    out.write(text);
}

const QByteArray kCall = "\nvoid planted() { model_.restoreBudget(budget); }\n";
}

TEST_CASE("the layering check passes on the current tree and on an unmodified copy of src") {
    const CheckResult real = runLayeringCheck(repoRoot());
    CHECK_MESSAGE(real.exitCode == 0, real.output.toStdString());

    QTemporaryDir tree;
    copySrc(tree);
    const CheckResult copy = runLayeringCheck(tree.path());
    CHECK_MESSAGE(copy.exitCode == 0, copy.output.toStdString());
    CHECK(copy.output.contains(QStringLiteral("Layering check passed.")));
}

TEST_CASE("the layering check fails when the Controller, View, main or a header names restoreBudget(") {
    for (const QString &file : {QStringLiteral("BudgetController.cpp"), QStringLiteral("BudgetController.h"),
                                QStringLiteral("BudgetView.cpp"), QStringLiteral("BudgetView.h"),
                                QStringLiteral("BudgetDisplay.h"), QStringLiteral("BudgetStore.h"),
                                QStringLiteral("main.cpp")}) {
        QTemporaryDir tree;
        copySrc(tree);
        plantText(tree, file, kCall);

        const CheckResult result = runLayeringCheck(tree.path());

        CAPTURE(file);
        CHECK_MESSAGE(result.exitCode != 0, result.output.toStdString());
        CHECK(result.output.contains(QStringLiteral("src/") + file));
    }
}

TEST_CASE("a comment that mentions restoreBudget( is also flagged outside the Model and the store") {
    QTemporaryDir tree;
    copySrc(tree);
    plantText(tree, QStringLiteral("BudgetController.cpp"), "\n// do not call restoreBudget(x) here\n");

    const CheckResult result = runLayeringCheck(tree.path());

    CHECK(result.exitCode != 0);
}

TEST_CASE("the layering check allows restoreBudget( in BudgetModel.h, BudgetModel.cpp and BudgetStore.cpp") {
    for (const QString &file : {QStringLiteral("BudgetModel.h"), QStringLiteral("BudgetModel.cpp"),
                                QStringLiteral("BudgetStore.cpp")}) {
        QTemporaryDir tree;
        copySrc(tree);
        plantText(tree, file, "\n// restoreBudget(budget) is for the store's load only\n");

        const CheckResult result = runLayeringCheck(tree.path());

        CAPTURE(file);
        CHECK_MESSAGE(result.exitCode == 0, result.output.toStdString());
    }
}
