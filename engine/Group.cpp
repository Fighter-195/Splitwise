#include "Group.h"

#include <stdexcept>

namespace splitwise {

Group::Group(std::string name) : name_(std::move(name)) {}

void Group::addMember(const User& user) {
    if (hasUser(user.id())) {
        throw std::invalid_argument("a member with this id already exists");
    }
    members_.push_back(user);
}

bool Group::hasUser(int id) const {
    return findUser(id) != nullptr;
}

const User* Group::findUser(int id) const {
    for (const User& u : members_) {
        if (u.id() == id) return &u;
    }
    return nullptr;
}

void Group::addExpense(std::unique_ptr<Expense> expense) {
    expenses_.push_back(std::move(expense));
}

int Group::nextUserId() const {
    int maxId = 0;
    for (const User& u : members_) {
        if (u.id() > maxId) maxId = u.id();
    }
    return maxId + 1;
}

} // namespace splitwise
