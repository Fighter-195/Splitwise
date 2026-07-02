#include "Ledger.h"

#include <cmath>

namespace splitwise {

Ledger::Ledger(const Group& group) : group_(group) {}

std::map<int, double> Ledger::balances() const {
    std::map<int, double> net;
    for (const User& u : group_.members()) {
        net[u.id()] = 0.0;
    }

    for (const auto& e : group_.expenses()) {
        // Whoever paid is owed the full amount they fronted...
        net[e->paidBy()] += e->total();
        // ...and every participant owes their share.
        for (const User& u : group_.members()) {
            net[u.id()] -= e->shareFor(u.id());
        }
    }
    return net;
}

std::vector<Payment> Ledger::settleUp() const {
    const double kEps = 0.005; // ignore sub-cent residues

    std::vector<std::pair<int, double>> creditors; // (id, +amount owed to them)
    std::vector<std::pair<int, double>> debtors;   // (id, +amount they owe)

    for (const auto& kv : balances()) {
        if (kv.second > kEps) {
            creditors.push_back({kv.first, kv.second});
        } else if (kv.second < -kEps) {
            debtors.push_back({kv.first, -kv.second});
        }
    }

    std::vector<Payment> payments;

    // Greedily match the largest debtor to the largest creditor. Each step
    // zeroes out at least one person, so this terminates quickly.
    while (!creditors.empty() && !debtors.empty()) {
        size_t ci = 0, di = 0;
        for (size_t i = 1; i < creditors.size(); ++i)
            if (creditors[i].second > creditors[ci].second) ci = i;
        for (size_t i = 1; i < debtors.size(); ++i)
            if (debtors[i].second > debtors[di].second) di = i;

        double amount = std::min(creditors[ci].second, debtors[di].second);
        payments.push_back({debtors[di].first, creditors[ci].first, amount});

        creditors[ci].second -= amount;
        debtors[di].second -= amount;

        if (creditors[ci].second <= kEps) creditors.erase(creditors.begin() + ci);
        if (debtors[di].second <= kEps) debtors.erase(debtors.begin() + di);
    }

    return payments;
}

} // namespace splitwise
