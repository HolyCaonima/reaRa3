#include "support/TestHarness.h"

#include <ra3/io/PathCompare.h>

using ra3::io::comparePathNames;

RA3_TEST(PathCompare_EqualityIgnoresAsciiCase)
{
    RA3_CHECK_EQ(comparePathNames("Data\\Misc.BIG", "data\\misc.big"), 0);
    RA3_CHECK_EQ(comparePathNames("", ""), 0);
    RA3_CHECK_EQ(comparePathNames("abc", "ABD"), -1);
    RA3_CHECK_EQ(comparePathNames("ab", "a"), 1);
    RA3_CHECK_EQ(comparePathNames("a", "ab"), -1);
}

RA3_TEST(PathCompare_OrdersLikeTheGameNotLikeStricmp)
{
    RA3_CHECK_EQ(comparePathNames("z", "_"), 1);
    RA3_CHECK_EQ(comparePathNames("Z", "_"), -1);
    RA3_CHECK_EQ(comparePathNames("_", "Z"), 1);
    RA3_CHECK_EQ(comparePathNames("data\\x", "DATA/x"), 1);
    RA3_CHECK_EQ(comparePathNames("\xE9", "a"), -1);
}
