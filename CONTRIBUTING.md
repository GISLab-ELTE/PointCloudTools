Coding conventions
=================

Formatting
----------

- **Length** of a line should not be longer than **120** characters. In the rare
  cases when we need a longer line (e.g. in a preprocessor macro) use backslash
  character to mark a continuation.
- **Tab characters** are **not** welcome in the source. Replace them with
  spaces.
- **Indentation** is 4 characters. Continuation indent is also 4 characters.
- **Parameter list** is a frequent place, when a line tends to be longer than
  the limit. Break the line and align parameters, as seen in the example below.

  ```cpp
  void aMethodWithManyParameters(
      const std::string& str1,
      const std::string& str2,
      const std::int32_t id);
  ```
- **Namespaces** are not indented.

  ```cpp
  namespace CloudTools
  {
  namespace DEM
  {
  /* ... */
  } // DEM
  } // CloudTools
  ```
- **Blocks** should follow the K&R style, where related opening and closing 
  brackets for namespaces, classes and function blocks should be placed to the 
  same column; while other compound statements should have their opening braces 
  at the same line as their respective control statements.
- **Class declarations** should contain the nested types, data members and 
  methods in this order. Therefore multiple `public`, `protected` and
  `private` parts are possible (also in this order). The keywords `public`, 
  `protected`, `private` are not indented, the member declarations are indented 
  as usual (with 4 spaces). Inside a visibility class declare types first.

  ```cpp
  class Operation
  {
  public:
    typedef std::function<bool(float, const std::string&)> ProgressType;
  
  private:
    bool _isPrepared = false;
    bool _isExecuted = false;
  
  public:
    virtual ~Operation() { }
    bool isPrepared() const { return _isPrepared; }
    bool isExecuted() const { return _isExecuted; }
    void prepare(bool force = false);
    void execute(bool force = false);
  
  protected:
    virtual void onPrepare() = 0;
    virtual void onExecute() = 0;
  };
  ```
- **Friend** declarations, if any, should be placed before the public
  visibility items, before the public keyword.
- The pointer and reference qualifier `*` and `&` letters should come
  **directly** after the type, followed by a space and then the variable's name:
  `int* ptr`, `const MyType& rhs`, `std::unique_ptr<T>&& data`.

Naming
------

- **File names** should contain ASCII characters and written in upper CamelCase 
  (with a few exceptions like `main.cpp`). Avoid other characters, like dash (-).
  Header file extension is `.h`, source file extension is `.cpp`.
- **Class and Type names** are written in upper CamelCase. Avoid underscore in class
  or type names. Pointers to major types should be typedef-ed, and should be
  called according the pointed type with a `Ptr` suffix.
- **Function names** start with lowercase letter, and have a capital letter for
  each new major tag. We do not require different names for static methods, or
  global functions.
- **Class member names** with private or protected visibility start with 
  underscore character following a lowercase letter, and have a capital letter 
  for each new major tag. Do not use other underscores in member names.
- **Function parameter names** start with a lowercase letter, and have a capital 
  letter for each new major tag. Do not use other underscores in parameter names.
- **Namespace names** are written in lower case. The content of main modules are 
  organized in a two-level namespace hierarchy: namespaces `CloudTools` and `AHN` 
  contain another namespace which describes the main module (e.g. `CloudTools::DEM`, 
  `AHN::Buildings`).

Headers
-------

- **Include guards** are mandatory for all headers, the `#pragma once` 
  preprocessor directive is used.  While it is non-standard, it is widely supported 
  by all major (modern) compilers.
- **Order of the inclusion of headers** - either in source files or in other
  header files - should be the following: First include standard C++ headers,
  then Boost headers, then other supporting library headers (GDAL, MPI, etc.), 
  then your implementing headers. Among the own headers of `CloudTools`, list the
  ones from other modules first.

  ```cpp
  #include <iostream>
  #include <iomanip>
  #include <fstream>
  #include <ctime>
  #include <chrono>

  #include <boost/program_options.hpp>
  #include <boost/filesystem.hpp>
  #include <gdal.h>

  #include <CloudTools.Common/IO/IO.h>
  #include <CloudTools.Common/IO/Reporter.h>
  #include "IOMode.h"
  #include "Process.h"
  ```
- **Never** apply `using namespace` directive in headers.
