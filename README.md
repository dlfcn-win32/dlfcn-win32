dlfcn-win32 [![Build status](https://ci.appveyor.com/api/projects/status/celmbkqj0k41fk04/branch/master?svg=true)](https://ci.appveyor.com/project/dlfcn-win32/dlfcn-win32/branch/master)
===========

dlfcn-win32 is an implementation of dlfcn for Windows.

dlfcn is a set of functions that allows runtime dynamic library loading. It is
standardized in the POSIX. Windows also provide similar routines, but not in a
POSIX-compatible way. This library attempts to implement a wrapper around the
Windows functions to make programs written for POSIX that use dlfcn work in
Windows without any modifications.

It follows the standard as described here:

* http://www.opengroup.org/onlinepubs/009695399/basedefs/dlfcn.h.html
* http://www.opengroup.org/onlinepubs/009695399/functions/dlerror.html
* http://www.opengroup.org/onlinepubs/009695399/functions/dlsym.html
* http://www.opengroup.org/onlinepubs/009695399/functions/dlclose.html
* http://www.opengroup.org/onlinepubs/009695399/functions/dlopen.html

Using This Library
------------------

### Using CMake 
Once the library has been installed, add to your project `CMakeLists.txt` : 
~~~cmake
...
find_package(dlfcn-win32 REQUIRED)
...
target_link_libraries(<target> dlfcn-win32::dl)
...
~~~
If you want to use this library in a cross-platform project, a convenient way 
to proceed is to define the CMake variable [`CMAKE_DL_LIBS`](https://cmake.org/cmake/help/latest/variable/CMAKE_DL_LIBS.html)
(that is normally empty on Windows) and then use it for linking: 
~~~cmake
...
if (WIN32)
  find_package(dlfcn-win32 REQUIRED)
  set(CMAKE_DL_LIBS dlfcn-win32::dl)
endif ()  
...
target_link_libraries(<target> ${CMAKE_DL_LIBS})
...
~~~

When cross-compiling you might want to set [`CMAKE_CROSSCOMPILING_EMULATOR`](https://cmake.org/cmake/help/latest/variable/CMAKE_CROSSCOMPILING_EMULATOR.html) to the path of wine to run tests.

Authors
-------

Contributors:
* [Pali Rohar (`pali`)](https://github.com/pali)
* [Timothy Gu (`TimothyGu`)](https://github.com/TimothyGu)
* [Ramiro Polla (`ramiropolla`)](https://github.com/ramiropolla)
* [Ralf Habacker (`rhabacker`)](https://github.com/rhabacker)
* [`xantares`](https://github.com/xantares)
* [`driver1998`](https://github.com/driver1998)
* [Angelo Haller (`szanni`)](https://github.com/szanni)

Mantainer:
* [Silvio Traversaro (`traversaro`)](https://github.com/traversaro)

License
-------

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
