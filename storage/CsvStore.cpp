#include "CsvStore.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include "../engine/Expense.h"

namespace splitwise {

namespace {

// Split `s` on `delim` into exactly `maxParts` parts. The final part keeps any
// remaining delimiters (used so an expense's payload stays intact).
std::vector<std::string> split(const std::string& s, char delim, size_t maxParts) {
    std::vector<std::string> out;
    std::string cur;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == delim && out.size() + 1 < maxParts) {
            out.push_back(cur);
            cur.clear();
        } else {
            cur += s[i];
        }
    }
    out.push_back(cur);
    return out;
}

std::vector<std::string> splitAll(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, delim)) out.push_back(cur);
    return out;
}

// Rebuild the right Expense subclass from a stored line's fields.
std::unique_ptr<Expense> parseExpense(const std::string& type,
                                      const std::string& description,
                                      int paidBy, double total,
                                      const std::string& payload) {
    if (type == "EQUAL") {
        std::vector<int> participants;
        for (const std::string& tok : splitAll(payload, ';')) {
            if (!tok.empty()) participants.push_back(std::stoi(tok));
        }
        return std::unique_ptr<Expense>(
            new EqualSplit(description, paidBy, total, participants));
    }

    // EXACT and PERCENT share the "id:value;id:value" payload shape.
    std::map<int, double> values;
    for (const std::string& tok : splitAll(payload, ';')) {
        if (tok.empty()) continue;
        std::vector<std::string> kv = splitAll(tok, ':');
        if (kv.size() != 2) throw std::runtime_error("bad expense payload: " + tok);
        values[std::stoi(kv[0])] = std::stod(kv[1]);
    }

    if (type == "EXACT") {
        return std::unique_ptr<Expense>(new ExactSplit(description, paidBy, values));
    }
    if (type == "PERCENT") {
        return std::unique_ptr<Expense>(
            new PercentSplit(description, paidBy, total, values));
    }
    throw std::runtime_error("unknown expense type: " + type);
}

} // namespace

void CsvStore::save(const Group& group, const std::string& path) {
    std::ofstream out(path.c_str());
    if (!out) throw std::runtime_error("cannot open file for writing: " + path);

    out << "GROUP," << group.name() << '\n';
    for (const User& u : group.members()) {
        out << "USER," << u.id() << ',' << u.name() << '\n';
    }
    for (const auto& e : group.expenses()) {
        out << "EXPENSE," << e->toCsv() << '\n';
    }
}

Group CsvStore::load(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in) throw std::runtime_error("cannot open file: " + path);

    Group group("Untitled");
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back(); // Windows CRLF

        std::vector<std::string> head = split(line, ',', 2);
        const std::string& kind = head[0];

        if (kind == "GROUP") {
            group.setName(head.size() > 1 ? head[1] : "Untitled");
        } else if (kind == "USER") {
            std::vector<std::string> f = split(line, ',', 3); // USER,id,name
            if (f.size() < 3) throw std::runtime_error("bad USER line: " + line);
            group.addMember(User(std::stoi(f[1]), f[2]));
        } else if (kind == "EXPENSE") {
            // EXPENSE,TYPE,description,paidBy,total,payload  -> 6 fields
            std::vector<std::string> f = split(line, ',', 6);
            if (f.size() < 6) throw std::runtime_error("bad EXPENSE line: " + line);
            group.addExpense(parseExpense(f[1], f[2], std::stoi(f[3]),
                                          std::stod(f[4]), f[5]));
        } else {
            throw std::runtime_error("unknown record: " + line);
        }
    }
    return group;
}

} // namespace splitwise
