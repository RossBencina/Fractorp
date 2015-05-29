#ifndef INCLUDED_EMDEER_H
#define INCLUDED_EMDEER_H

/// emdeer.h for use with emdeer.py

#include <cstdio>

/// The `MD_BEGIN_OUTPUT` macro is used to delimit sections of the
/// stdio program output. emdeer.py splits the output into blocks at
/// these boundaries and then weaves the output blocks into the .md file
/// when a tag surrounded by square brackets is encountered. e.g.:
/// ```
/// /// [SECTION_NAME]
/// ```
/// Where `SECTION_NAME` is the actual text of the string passed to
/// the `MD_BEGIN_OUTPUT()` macro.

#define MD_BEGIN_OUTPUT(SECTION_NAME) std::printf("\n# [" SECTION_NAME "] #\n\n")

#endif /* INCLUDED_EMDEER_H */