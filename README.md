# About ModernCLI

Moderncli is a package to collect and centralize testing for many of my inline
C++ header libraries which I used to duplicate or vendor in other packages.
These headers are being gathered under a common package that can then be
installed and used as a core dependency. Hence, one interpretation for this
package is "Modern C++" Library Interfaces. Moderncli is meant to support C++
17 or later.

Moderncli requires CMake to build. It should build and work with GCC (9 or
later), with Clang (14? or later), and MSVC.  Besides GNU/Linux and BSD
systems, Moderncli is portable to and can support the MingW platform target as
installed by Debian when selecting posix runtime, and even native MS Visual
Studio builds. The minimum requirement is a C++17 compiler (or later) that
supports the C++ filesystem header and runtime library.

## Dependencies

Moderncli and applications also often make use of the C++ library libfmt. This
is preferred over the use of std::format because it is much smaller when used
in multiple applications as a shared library, which std::format does not
support, and it is better optimized. However if you do not use the Moderncli
print.hpp header in your own applications you do not require libfmt either.

Moderncli also uses openssl for crypto operations and any crypto related
headers. Generally it should only require libcrypto from openssl. In addition,
C++ thread support may have to be enabled to use C++ thread operations. The
cmake/features.cmake file shows how to test for and enable these dependencies
correctly from CMake.

## Distributions

Distributions of this package are provided as detached source tarballs made
from a tagged release from our internal source repository. These stand-alone
detached tarballs can be used to make packages for many GNU/Linux systems, and
for BSD ports. They may also be used to build and install the software directly
on a target platform.

The latest release source tarball is found at
https://www.tychosoft.com/tychosoft/-/packages/generic/moderncli which provides
access to past releases as well.

## Participation

This project is offered as free (as in freedom) software for public use and has
a public home page at https://www.tychosoft.com/tychosoft/moderncli which has
an issue tracker where people can submit public bug reports, and a wiki for
hosting project documentation. We are not maintaining a public git repo nor do
we have any production or development related resources hosted on external
sites. Patches may be submitted and attached to an issue in the issue tracker.
Support requests and other kinds of inquiries may also be sent privately thru
email to tychosoft@gmail.com. Other details about participation may be found in
the Contributing page.

## Testing

There are testing programs for each header. These run simple tests that will be
expanded to improve code coverage over time. The test programs are the only
built target making this library by itself, and the test programs in this
package work with the cmake ctest framework. They may also be used as simple
examples of how a given header works. There is also a **lint** target that can
be used to verify code changes.
