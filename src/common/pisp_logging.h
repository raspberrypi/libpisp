#pragma once

#include <boost/log/trivial.hpp>

#define PISP_LOGGING_ENABLE 1

#define PISP_LOG(sev, stuff) \
do { \
    if (PISP_LOGGING_ENABLE) \
        BOOST_LOG_TRIVIAL(sev) << __FUNCTION__ << ": " << stuff; \
} while (0)

#define PISP_ASSERT(x) assert(x)

namespace PiSP
{

// Call this before you try and use any logging.

void logging_init();

}