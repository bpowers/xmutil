cmake_minimum_required(VERSION 3.16.0)

project(xmutil CXX C)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(tinyxml2
  third_party/tinyxml2/tinyxml2.cpp
)

target_compile_options(tinyxml2 PRIVATE -fPIC)


add_library(utf
  third_party/libutf/rune.c
  third_party/libutf/runestrcat.c
  third_party/libutf/runestrchr.c
  third_party/libutf/runestrcmp.c
  third_party/libutf/runestrcpy.c
  third_party/libutf/runestrdup.c
  third_party/libutf/runestrecpy.c
  third_party/libutf/runestrlen.c
  third_party/libutf/runestrncat.c
  third_party/libutf/runestrncmp.c
  third_party/libutf/runestrncpy.c
  third_party/libutf/runestrrchr.c
  third_party/libutf/runestrstr.c
  third_party/libutf/runetype.c
  third_party/libutf/utfecpy.c
  third_party/libutf/utflen.c
  third_party/libutf/utfnlen.c
  third_party/libutf/utfrrune.c
  third_party/libutf/utfrune.c
  third_party/libutf/utfutf.c
)

target_compile_options(utf PRIVATE -fPIC)

add_library(xmutil
  src/Function/TableFunction.cpp
  src/Function/Level.cpp
  src/Function/State.cpp
  src/Function/Function.cpp
  src/Xmile/XMILEGenerator.cpp
  src/Vensim/VensimView.cpp
  src/Vensim/VensimParseFunctions.cpp
  src/Vensim/VYacc.tab.cpp
  src/Vensim/VensimLex.cpp
  src/Vensim/VensimParse.cpp
  src/Model.cpp
  src/ContextInfo.cpp
  src/XMUtil.cpp
  src/Symbol/Symbol.cpp
  src/Symbol/SymbolTableBase.cpp
  src/Symbol/SymbolList.cpp
  src/Symbol/Units.cpp
  src/Symbol/NotUsed_SymAllocList.cpp
  src/Symbol/SymbolNameSpace.cpp
  src/Symbol/SymbolListList.cpp
  src/Symbol/LeftHandSide.cpp
  src/Symbol/ExpressionList.cpp
  src/Symbol/Expression.cpp
  src/Symbol/Equation.cpp
  src/Symbol/Variable.cpp
  src/Symbol/UnitExpression.cpp
)

target_compile_options(xmutil PRIVATE -fvisibility=hidden)
target_link_libraries(xmutil tinyxml2 utf)
target_include_directories(xmutil PUBLIC third_party)
