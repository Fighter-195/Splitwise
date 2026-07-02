#ifndef SPLITWISE_CSVSTORE_H
#define SPLITWISE_CSVSTORE_H

#include <string>

#include "../engine/Group.h"

namespace splitwise {

// Loads / saves a Group to a plain CSV-style text file.
//
// File layout (one record per line, first field is the record type):
//   GROUP,<name>
//   USER,<id>,<name>
//   EXPENSE,<TYPE>,<description>,<paidBy>,<total>,<payload>
class CsvStore {
public:
    // Throws std::runtime_error if the file cannot be opened for writing.
    static void save(const Group& group, const std::string& path);

    // Throws std::runtime_error on a missing or malformed file.
    static Group load(const std::string& path);
};

} // namespace splitwise

#endif // SPLITWISE_CSVSTORE_H
