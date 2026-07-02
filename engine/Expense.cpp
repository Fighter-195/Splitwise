#include "Expense.h"

#include <sstream>

namespace splitwise {

namespace {
// Format a double with 2 decimal places for storage/display.
std::string money(double v) {
    std::ostringstream os;
    os.setf(std::ios::fixed);
    os.precision(2);
    os << v;
    return os.str();
}
} // namespace

// ---- Expense (base) --------------------------------------------------------

Expense::Expense(std::string description, int paidBy, double total,
                 std::vector<int> participants)
    : description_(std::move(description)),
      paidBy_(paidBy),
      total_(total),
      participants_(std::move(participants)) {}

std::string Expense::toCsv() const {
    // Layout: TYPE,description,paidBy,total,payload
    std::ostringstream os;
    os << type() << ',' << description_ << ',' << paidBy_ << ','
       << money(total_) << ',' << payloadCsv();
    return os.str();
}

// ---- EqualSplit ------------------------------------------------------------

EqualSplit::EqualSplit(std::string description, int paidBy, double total,
                       std::vector<int> participants)
    : Expense(std::move(description), paidBy, total, std::move(participants)) {}

double EqualSplit::shareFor(int userId) const {
    if (participants_.empty()) return 0.0;
    for (int id : participants_) {
        if (id == userId) return total_ / static_cast<double>(participants_.size());
    }
    return 0.0;
}

std::string EqualSplit::payloadCsv() const {
    // payload = "id;id;id"
    std::ostringstream os;
    for (size_t i = 0; i < participants_.size(); ++i) {
        if (i) os << ';';
        os << participants_[i];
    }
    return os.str();
}

// ---- ExactSplit ------------------------------------------------------------

namespace {
double sumValues(const std::map<int, double>& m) {
    double s = 0.0;
    for (const auto& kv : m) s += kv.second;
    return s;
}
std::vector<int> keysOf(const std::map<int, double>& m) {
    std::vector<int> keys;
    keys.reserve(m.size());
    for (const auto& kv : m) keys.push_back(kv.first);
    return keys;
}
} // namespace

ExactSplit::ExactSplit(std::string description, int paidBy,
                       std::map<int, double> amounts)
    : Expense(std::move(description), paidBy, sumValues(amounts),
              keysOf(amounts)),
      amounts_(std::move(amounts)) {}

double ExactSplit::shareFor(int userId) const {
    auto it = amounts_.find(userId);
    return it == amounts_.end() ? 0.0 : it->second;
}

std::string ExactSplit::payloadCsv() const {
    // payload = "id:amount;id:amount"
    std::ostringstream os;
    bool first = true;
    for (const auto& kv : amounts_) {
        if (!first) os << ';';
        first = false;
        os << kv.first << ':' << money(kv.second);
    }
    return os.str();
}

// ---- PercentSplit ----------------------------------------------------------

PercentSplit::PercentSplit(std::string description, int paidBy, double total,
                           std::map<int, double> percents)
    : Expense(std::move(description), paidBy, total, keysOf(percents)),
      percents_(std::move(percents)) {}

double PercentSplit::shareFor(int userId) const {
    auto it = percents_.find(userId);
    if (it == percents_.end()) return 0.0;
    return total_ * it->second / 100.0;
}

std::string PercentSplit::payloadCsv() const {
    // payload = "id:percent;id:percent"
    std::ostringstream os;
    bool first = true;
    for (const auto& kv : percents_) {
        if (!first) os << ';';
        first = false;
        os << kv.first << ':' << money(kv.second);
    }
    return os.str();
}

} // namespace splitwise
