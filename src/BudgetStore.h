#pragma once

#include <QLockFile>
#include <QString>

class BudgetModel;

// Saves a BudgetModel to a SQLite file and loads it back. It sits beside the Model and
// uses only the Model's public, validated API, so the Model knows nothing about storage.
class BudgetStore {
public:
    explicit BudgetStore(const QString &databasePath);
    ~BudgetStore();
    // Each store owns one named connection; a copy would remove it twice.
    BudgetStore(const BudgetStore &) = delete;
    BudgetStore &operator=(const BudgetStore &) = delete;

    // Replaces `model` with the saved data only if every saved row is accepted. A missing
    // file loads as an empty model. On failure `model` is unchanged. The first load locks
    // the file for the store's lifetime, so a second store or window on it cannot load.
    bool load(BudgetModel &model, QString *error = nullptr);
    // Rewrites the file in one transaction. Refused unless the last load succeeded, so a
    // file that failed to load is never overwritten.
    bool save(const BudgetModel &model, QString *error = nullptr);

private:
    QString connectionName_;
    // Every save rewrites the whole file, so two writers would erase each other's entries.
    QLockFile lock_;
    bool loaded_ = false;
};
