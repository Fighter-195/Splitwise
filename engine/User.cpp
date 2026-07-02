#include "User.h"

namespace splitwise {

User::User(int id, std::string name)
    : id_(id), name_(std::move(name)) {}

int User::id() const { return id_; }

const std::string& User::name() const { return name_; }

} // namespace splitwise
