//
//  Created by irantha on 9/10/23.
//

#ifndef assertion_error_h
#define assertion_error_h

#include <stdexcept>
#include <string>

namespace Levelz::Async::Test {

struct AssertionError: std::runtime_error {
    explicit AssertionError(const std::string& description)
    : std::runtime_error { description }
    {
    }
    
    static void check(bool isOkay, const std::string& description = "assert failed")
    {
        if (!isOkay)
            throw AssertionError {description};
    }
    
};

}


#endif /* assertion_error_h */
