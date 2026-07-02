// Splitwise (C++) — menu-driven command-line front-end.
//
// This file is pure "view": it reads input and prints output, but every real
// decision (splitting a bill, computing balances, settling up) is delegated to
// the engine classes. Swap this file for a GUI and the engine is untouched.

#include <cmath>
#include <cstdio>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../engine/Group.h"
#include "../engine/Ledger.h"
#include "../storage/CsvStore.h"

using namespace splitwise;

namespace {

// ---- input helpers ---------------------------------------------------------

std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    if (!std::getline(std::cin, line)) {
        std::cout << "\n";
        return ""; // EOF
    }
    return line;
}

// Read an integer within [lo, hi]. Re-prompts on bad input.
int readInt(const std::string& prompt, int lo, int hi) {
    while (true) {
        std::string s = readLine(prompt);
        std::istringstream is(s);
        int v;
        char extra;
        if ((is >> v) && !(is >> extra) && v >= lo && v <= hi) return v;
        std::cout << "  ! please enter a number between " << lo << " and " << hi
                  << "\n";
    }
}

// Read a positive amount of money.
double readAmount(const std::string& prompt) {
    while (true) {
        std::string s = readLine(prompt);
        std::istringstream is(s);
        double v;
        char extra;
        if ((is >> v) && !(is >> extra) && v > 0.0) return v;
        std::cout << "  ! please enter a positive amount (e.g. 250 or 99.50)\n";
    }
}

std::string money(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

// ---- display helpers -------------------------------------------------------

std::string nameOf(const Group& g, int id) {
    const User* u = g.findUser(id);
    return u ? u->name() : ("#" + std::to_string(id));
}

void listMembers(const Group& g) {
    if (g.members().empty()) {
        std::cout << "  (no members yet)\n";
        return;
    }
    for (const User& u : g.members()) {
        std::cout << "  [" << u.id() << "] " << u.name() << "\n";
    }
}

void listExpenses(const Group& g) {
    if (g.expenses().empty()) {
        std::cout << "  (no expenses yet)\n";
        return;
    }
    int n = 1;
    for (const auto& e : g.expenses()) {
        std::cout << "  " << n++ << ". " << e->description() << "  ("
                  << money(e->total()) << ", " << e->type() << ", paid by "
                  << nameOf(g, e->paidBy()) << ")\n";
        for (int pid : e->participants()) {
            std::cout << "        - " << nameOf(g, pid) << " owes "
                      << money(e->shareFor(pid)) << "\n";
        }
    }
}

void showBalances(const Group& g) {
    Ledger ledger(g);
    std::map<int, double> bal = ledger.balances();
    bool any = false;
    for (const auto& kv : bal) {
        if (std::fabs(kv.second) < 0.005) continue;
        any = true;
        if (kv.second > 0) {
            std::cout << "  " << nameOf(g, kv.first) << " is owed "
                      << money(kv.second) << "\n";
        } else {
            std::cout << "  " << nameOf(g, kv.first) << " owes "
                      << money(-kv.second) << "\n";
        }
    }
    if (!any) std::cout << "  all settled up \xE2\x9C\x93\n";
}

void showSettlement(const Group& g) {
    Ledger ledger(g);
    std::vector<Payment> plan = ledger.settleUp();
    if (plan.empty()) {
        std::cout << "  nothing to settle \xE2\x9C\x93\n";
        return;
    }
    std::cout << "  minimal payments to settle up:\n";
    for (const Payment& p : plan) {
        std::cout << "    " << nameOf(g, p.from) << " -> " << nameOf(g, p.to)
                  << " : " << money(p.amount) << "\n";
    }
}

// ---- actions ---------------------------------------------------------------

void addMember(Group& g) {
    std::string name = readLine("  member name: ");
    if (name.empty()) {
        std::cout << "  ! name cannot be empty\n";
        return;
    }
    int id = g.nextUserId();
    g.addMember(User(id, name));
    std::cout << "  added " << name << " (id " << id << ")\n";
}

// Let the user pick some members by id. Returns their ids (validated).
std::vector<int> pickParticipants(const Group& g) {
    std::cout << "  participants:\n";
    listMembers(g);
    std::cout << "  enter ids separated by spaces, or 'all':\n";
    std::string s = readLine("  > ");

    std::vector<int> chosen;
    if (s == "all") {
        for (const User& u : g.members()) chosen.push_back(u.id());
        return chosen;
    }
    std::istringstream is(s);
    int id;
    while (is >> id) {
        if (g.hasUser(id)) {
            bool dup = false;
            for (int c : chosen) if (c == id) dup = true;
            if (!dup) chosen.push_back(id);
        } else {
            std::cout << "  ! ignoring unknown id " << id << "\n";
        }
    }
    return chosen;
}

void addExpense(Group& g) {
    if (g.members().size() < 2) {
        std::cout << "  ! add at least 2 members first\n";
        return;
    }

    std::cout << "  who paid?\n";
    listMembers(g);
    int payer = readInt("  payer id: ", 1, g.nextUserId());
    if (!g.hasUser(payer)) {
        std::cout << "  ! no member with id " << payer << "\n";
        return;
    }

    std::string desc = readLine("  description: ");
    if (desc.empty()) desc = "expense";
    // Keep the CSV one-record-per-line format intact.
    for (char& c : desc) if (c == ',' || c == '\n' || c == '\r') c = ' ';

    std::cout << "  split type:  1) equal   2) exact amounts   3) percentages\n";
    int type = readInt("  choice: ", 1, 3);

    if (type == 1) {
        std::vector<int> parts = pickParticipants(g);
        if (parts.empty()) {
            std::cout << "  ! no valid participants\n";
            return;
        }
        double total = readAmount("  total amount: ");
        g.addExpense(std::unique_ptr<Expense>(
            new EqualSplit(desc, payer, total, parts)));
        std::cout << "  added equal-split expense\n";
        return;
    }

    if (type == 2) { // exact
        std::vector<int> parts = pickParticipants(g);
        if (parts.empty()) {
            std::cout << "  ! no valid participants\n";
            return;
        }
        std::map<int, double> amounts;
        for (int id : parts) {
            amounts[id] = readAmount("  amount for " + nameOf(g, id) + ": ");
        }
        g.addExpense(std::unique_ptr<Expense>(
            new ExactSplit(desc, payer, amounts)));
        std::cout << "  added exact-split expense (total "
                  << money([&] { double s = 0; for (auto& kv : amounts) s += kv.second; return s; }())
                  << ")\n";
        return;
    }

    // type == 3, percent
    std::vector<int> parts = pickParticipants(g);
    if (parts.empty()) {
        std::cout << "  ! no valid participants\n";
        return;
    }
    double total = readAmount("  total amount: ");
    std::map<int, double> percents;
    double sum = 0.0;
    for (int id : parts) {
        double p = readAmount("  percent for " + nameOf(g, id) + " (%): ");
        percents[id] = p;
        sum += p;
    }
    if (std::fabs(sum - 100.0) > 0.01) {
        std::cout << "  ! percentages add up to " << money(sum)
                  << "%, not 100% — expense not added\n";
        return;
    }
    g.addExpense(std::unique_ptr<Expense>(
        new PercentSplit(desc, payer, total, percents)));
    std::cout << "  added percent-split expense\n";
}

void saveGroup(const Group& g) {
    std::string path = readLine("  save to file [group.csv]: ");
    if (path.empty()) path = "group.csv";
    try {
        CsvStore::save(g, path);
        std::cout << "  saved to " << path << "\n";
    } catch (const std::exception& ex) {
        std::cout << "  ! " << ex.what() << "\n";
    }
}

bool loadGroup(Group& g) {
    std::string path = readLine("  load from file [group.csv]: ");
    if (path.empty()) path = "group.csv";
    try {
        g = CsvStore::load(path);
        std::cout << "  loaded group '" << g.name() << "' with "
                  << g.members().size() << " members, " << g.expenses().size()
                  << " expenses\n";
        return true;
    } catch (const std::exception& ex) {
        std::cout << "  ! " << ex.what() << "\n";
        return false;
    }
}

void printMenu(const Group& g) {
    std::cout << "\n==== Splitwise (C++) — group: " << g.name() << " ====\n"
              << "  1) add member\n"
              << "  2) add expense\n"
              << "  3) list members\n"
              << "  4) list expenses\n"
              << "  5) show balances\n"
              << "  6) settle up (minimal payments)\n"
              << "  7) save to file\n"
              << "  8) load from file\n"
              << "  9) rename group\n"
              << "  0) exit\n";
}

} // namespace

int main() {
    Group group("My Group");
    std::cout << "Splitwise (C++) — split shared expenses fairly.\n";

    while (true) {
        printMenu(group);
        int choice = readInt("  choice: ", 0, 9);
        std::cout << "\n";
        switch (choice) {
            case 1: addMember(group); break;
            case 2: addExpense(group); break;
            case 3: listMembers(group); break;
            case 4: listExpenses(group); break;
            case 5: showBalances(group); break;
            case 6: showSettlement(group); break;
            case 7: saveGroup(group); break;
            case 8: loadGroup(group); break;
            case 9: {
                std::string n = readLine("  new group name: ");
                if (!n.empty()) group.setName(n);
                break;
            }
            case 0:
                std::cout << "  bye!\n";
                return 0;
        }
    }
}
