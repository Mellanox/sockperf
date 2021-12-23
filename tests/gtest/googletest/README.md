# Google Test

This directory contains minimum set of needed source code of Google Test release-1.7.0
(see full package at https://github.com/google/googletest/releases/tag/release-1.7.0)
with following back ported commits:

1. commit 15dde751ff5ea0f21f2a7182186be4564f30adc7
Author: Arseny Aprelev <arseny.aprelev@gmail.com>
Date:   Mon Oct 1 16:47:09 2018 -0400

    Merge 2ce0685f76a4db403b7b2650433a584c150f2108 into 75e834700d19aa373b428c7c746f951737354c28

    Closes #1544
    With refinements and changes

    PiperOrigin-RevId: 215273083

2. commit 59f90a338bce2376b540ee239cf4e269bf6d68ad (HEAD)
Author: durandal <durandal@google.com>
Date:   Tue Oct 23 15:31:17 2018 -0400

    Googletest export

    Honor GTEST_SKIP() in SetUp().

    PiperOrigin-RevId: 218387359
