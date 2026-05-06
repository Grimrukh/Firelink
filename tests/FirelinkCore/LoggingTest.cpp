#include <doctest/doctest.h>

#include <FirelinkCore/Logging.h>

/// @brief Tests C++20 (format), FirelinkCore logging, and FirelinkDSR process name.
TEST_CASE("FirelinkCore: Logging works")
{
    Firelink::Debug("Firelink logging success.");
    Firelink::Info("Firelink logging success.");
    Firelink::Warning("Firelink logging success.");
    Firelink::Error("Firelink logging success.");
}
