#ifndef SPLITWISE_GROUP_H
#define SPLITWISE_GROUP_H

#include <memory>
#include <string>
#include <vector>

#include "Expense.h"
#include "User.h"

namespace splitwise {

// A named group of people that share expenses (e.g. a trip, a flat).
// Owns its members and expenses.
class Group {
public:
    explicit Group(std::string name);

    const std::string& name() const { return name_; }
    void setName(std::string name) { name_ = std::move(name); }

    // Adds a member. Throws std::invalid_argument if the id already exists.
    void addMember(const User& user);
    bool hasUser(int id) const;
    const User* findUser(int id) const; // nullptr if not found

    const std::vector<User>& members() const { return members_; }

    // Takes ownership of the expense.
    void addExpense(std::unique_ptr<Expense> expense);
    const std::vector<std::unique_ptr<Expense>>& expenses() const {
        return expenses_;
    }

    // Smallest unused positive id (convenience for the UI).
    int nextUserId() const;

private:
    std::string name_;
    std::vector<User> members_;
    std::vector<std::unique_ptr<Expense>> expenses_;
};

} // namespace splitwise

#endif // SPLITWISE_GROUP_H
