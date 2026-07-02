#ifndef SPLITWISE_LEDGER_H
#define SPLITWISE_LEDGER_H

#include <map>
#include <vector>

#include "Group.h"

namespace splitwise {

// A single settling payment: `from` pays `amount` to `to`.
struct Payment {
    int from;
    int to;
    double amount;
};

// Computes who owes whom for a group. Holds a reference to the group, so it
// must not outlive it.
class Ledger {
public:
    explicit Ledger(const Group& group);

    // Net balance per user id:
    //   > 0  => the group owes them money (they are a creditor)
    //   < 0  => they owe the group money (they are a debtor)
    // Balances always sum to ~0.
    std::map<int, double> balances() const;

    // A minimal set of payments that settles every balance. Greedy: repeatedly
    // match the biggest debtor against the biggest creditor.
    std::vector<Payment> settleUp() const;

private:
    const Group& group_;
};

} // namespace splitwise

#endif // SPLITWISE_LEDGER_H
