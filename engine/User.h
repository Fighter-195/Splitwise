#ifndef SPLITWISE_USER_H
#define SPLITWISE_USER_H

#include <string>

namespace splitwise {

// A single person in a group. Immutable once created.
class User {
public:
    User(int id, std::string name);

    int id() const;
    const std::string& name() const;

private:
    int id_;
    std::string name_;
};

} // namespace splitwise

#endif // SPLITWISE_USER_H
