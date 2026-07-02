#ifndef SPLITWISE_EXPENSE_H
#define SPLITWISE_EXPENSE_H

#include <map>
#include <string>
#include <vector>

namespace splitwise {

// Abstract base for a single expense.
//
// The whole point of the class hierarchy: a bill can be split in different
// ways (evenly, by exact amounts, by percentage). Instead of the Ledger doing
// a big switch on "what kind of split is this?", every Expense knows how to
// answer one question about itself:
//
//     "how much does user X owe for me?"  ->  shareFor(userId)
//
// That single virtual call is polymorphism doing real work.
class Expense {
public:
    Expense(std::string description, int paidBy, double total,
            std::vector<int> participants);
    virtual ~Expense() = default;

    // How much `userId` owes for this expense (0 if not a participant).
    virtual double shareFor(int userId) const = 0;

    // Short tag identifying the split kind ("EQUAL" / "EXACT" / "PERCENT").
    virtual std::string type() const = 0;

    const std::string& description() const { return description_; }
    int paidBy() const { return paidBy_; }
    double total() const { return total_; }
    const std::vector<int>& participants() const { return participants_; }

    // Serialize the whole expense to a single CSV line (no trailing newline).
    std::string toCsv() const;

protected:
    // Subclass-specific tail of the CSV line (participant / amount data).
    virtual std::string payloadCsv() const = 0;

    std::string description_;
    int paidBy_;
    double total_;
    std::vector<int> participants_;
};

// Split the bill evenly among all participants.
class EqualSplit : public Expense {
public:
    EqualSplit(std::string description, int paidBy, double total,
               std::vector<int> participants);

    double shareFor(int userId) const override;
    std::string type() const override { return "EQUAL"; }

protected:
    std::string payloadCsv() const override;
};

// Each participant owes an exact, explicitly-given amount. The total is the
// sum of those amounts.
class ExactSplit : public Expense {
public:
    ExactSplit(std::string description, int paidBy,
               std::map<int, double> amounts);

    double shareFor(int userId) const override;
    std::string type() const override { return "EXACT"; }

protected:
    std::string payloadCsv() const override;

private:
    std::map<int, double> amounts_;
};

// Each participant owes a percentage of the total (percentages should sum
// to 100).
class PercentSplit : public Expense {
public:
    PercentSplit(std::string description, int paidBy, double total,
                 std::map<int, double> percents);

    double shareFor(int userId) const override;
    std::string type() const override { return "PERCENT"; }

protected:
    std::string payloadCsv() const override;

private:
    std::map<int, double> percents_;
};

} // namespace splitwise

#endif // SPLITWISE_EXPENSE_H
