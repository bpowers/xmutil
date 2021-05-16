/* A Bison parser, made by GNU Bison 3.7.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30704

/* Bison version string.  */
#define YYBISON_VERSION "3.7.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the variable and function names.  */
#define yyparse vpyyparse
#define yylex vpyylex
#define yyerror vpyyerror
#define yydebug vpyydebug
#define yynerrs vpyynerrs
#define yylval vpyylval
#define yychar vpyychar

/* First part of user prologue.  */
#line 7 "VYacc.y"

#include "../Symbol/Parse.h"
#include "VensimParseFunctions.h"
extern int vpyylex(void);
extern void vpyyerror(char const *);
#define YYSTYPE ParseUnion

#line 86 "VYacc.tab.cpp"

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void *)0)
#endif
#endif

#include "VYacc.tab.hpp"
/* Symbol kind.  */
enum yysymbol_kind_t {
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,               /* "end of file"  */
  YYSYMBOL_YYerror = 1,             /* error  */
  YYSYMBOL_YYUNDEF = 2,             /* "invalid token"  */
  YYSYMBOL_VPTT_dataequals = 3,     /* VPTT_dataequals  */
  YYSYMBOL_VPTT_with_lookup = 4,    /* VPTT_with_lookup  */
  YYSYMBOL_VPTT_map = 5,            /* VPTT_map  */
  YYSYMBOL_VPTT_equiv = 6,          /* VPTT_equiv  */
  YYSYMBOL_VPTT_groupstar = 7,      /* VPTT_groupstar  */
  YYSYMBOL_VPTT_and = 8,            /* VPTT_and  */
  YYSYMBOL_VPTT_macro = 9,          /* VPTT_macro  */
  YYSYMBOL_VPTT_end_of_macro = 10,  /* VPTT_end_of_macro  */
  YYSYMBOL_VPTT_or = 11,            /* VPTT_or  */
  YYSYMBOL_VPTT_not = 12,           /* VPTT_not  */
  YYSYMBOL_VPTT_hold_backward = 13, /* VPTT_hold_backward  */
  YYSYMBOL_VPTT_look_forward = 14,  /* VPTT_look_forward  */
  YYSYMBOL_VPTT_except = 15,        /* VPTT_except  */
  YYSYMBOL_VPTT_na = 16,            /* VPTT_na  */
  YYSYMBOL_VPTT_interpolate = 17,   /* VPTT_interpolate  */
  YYSYMBOL_VPTT_raw = 18,           /* VPTT_raw  */
  YYSYMBOL_VPTT_test_input = 19,    /* VPTT_test_input  */
  YYSYMBOL_VPTT_the_condition = 20, /* VPTT_the_condition  */
  YYSYMBOL_VPTT_implies = 21,       /* VPTT_implies  */
  YYSYMBOL_VPTT_ge = 22,            /* VPTT_ge  */
  YYSYMBOL_VPTT_le = 23,            /* VPTT_le  */
  YYSYMBOL_VPTT_ne = 24,            /* VPTT_ne  */
  YYSYMBOL_VPTT_tabbed_array = 25,  /* VPTT_tabbed_array  */
  YYSYMBOL_VPTT_eqend = 26,         /* VPTT_eqend  */
  YYSYMBOL_VPTT_number = 27,        /* VPTT_number  */
  YYSYMBOL_VPTT_literal = 28,       /* VPTT_literal  */
  YYSYMBOL_VPTT_symbol = 29,        /* VPTT_symbol  */
  YYSYMBOL_VPTT_units_symbol = 30,  /* VPTT_units_symbol  */
  YYSYMBOL_VPTT_function = 31,      /* VPTT_function  */
  YYSYMBOL_32_ = 32,                /* '%'  */
  YYSYMBOL_33_ = 33,                /* '|'  */
  YYSYMBOL_34_ = 34,                /* '-'  */
  YYSYMBOL_35_ = 35,                /* '+'  */
  YYSYMBOL_36_ = 36,                /* '='  */
  YYSYMBOL_37_ = 37,                /* '<'  */
  YYSYMBOL_38_ = 38,                /* '>'  */
  YYSYMBOL_39_ = 39,                /* '*'  */
  YYSYMBOL_40_ = 40,                /* '/'  */
  YYSYMBOL_41_ = 41,                /* '^'  */
  YYSYMBOL_42_ = 42,                /* '~'  */
  YYSYMBOL_43_ = 43,                /* '('  */
  YYSYMBOL_44_ = 44,                /* ')'  */
  YYSYMBOL_45_ = 45,                /* ','  */
  YYSYMBOL_46_ = 46,                /* ':'  */
  YYSYMBOL_47_ = 47,                /* '['  */
  YYSYMBOL_48_ = 48,                /* ']'  */
  YYSYMBOL_49_ = 49,                /* '!'  */
  YYSYMBOL_50_ = 50,                /* '?'  */
  YYSYMBOL_51_ = 51,                /* ';'  */
  YYSYMBOL_YYACCEPT = 52,           /* $accept  */
  YYSYMBOL_fulleq = 53,             /* fulleq  */
  YYSYMBOL_macrostart = 54,         /* macrostart  */
  YYSYMBOL_55_1 = 55,               /* $@1  */
  YYSYMBOL_macroend = 56,           /* macroend  */
  YYSYMBOL_eqn = 57,                /* eqn  */
  YYSYMBOL_lhs = 58,                /* lhs  */
  YYSYMBOL_var = 59,                /* var  */
  YYSYMBOL_sublist = 60,            /* sublist  */
  YYSYMBOL_symlist = 61,            /* symlist  */
  YYSYMBOL_subdef = 62,             /* subdef  */
  YYSYMBOL_unitsrange = 63,         /* unitsrange  */
  YYSYMBOL_urangenum = 64,          /* urangenum  */
  YYSYMBOL_number = 65,             /* number  */
  YYSYMBOL_units = 66,              /* units  */
  YYSYMBOL_interpmode = 67,         /* interpmode  */
  YYSYMBOL_exceptlist = 68,         /* exceptlist  */
  YYSYMBOL_mapsymlist = 69,         /* mapsymlist  */
  YYSYMBOL_maplist = 70,            /* maplist  */
  YYSYMBOL_exprlist = 71,           /* exprlist  */
  YYSYMBOL_exp = 72,                /* exp  */
  YYSYMBOL_tablevals = 73,          /* tablevals  */
  YYSYMBOL_xytablevals = 74,        /* xytablevals  */
  YYSYMBOL_xytablevec = 75,         /* xytablevec  */
  YYSYMBOL_tablepairs = 76          /* tablepairs  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;

#ifdef short
#undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
#include <limits.h> /* INFRINGES ON USER NAME SPACE */
#if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#define YY_STDINT_H
#endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
#if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#define YYPTRDIFF_T __PTRDIFF_TYPE__
#define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
#elif defined PTRDIFF_MAX
#ifndef ptrdiff_t
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#endif
#define YYPTRDIFF_T ptrdiff_t
#define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
#else
#define YYPTRDIFF_T long
#define YYPTRDIFF_MAXIMUM LONG_MAX
#endif
#endif

#ifndef YYSIZE_T
#ifdef __SIZE_TYPE__
#define YYSIZE_T __SIZE_TYPE__
#elif defined size_t
#define YYSIZE_T size_t
#elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned
#endif
#endif

#define YYSIZE_MAXIMUM \
  YY_CAST(YYPTRDIFF_T, (YYPTRDIFF_MAXIMUM < YY_CAST(YYSIZE_T, -1) ? YYPTRDIFF_MAXIMUM : YY_CAST(YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST(YYPTRDIFF_T, sizeof(X))

/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#define YY_(Msgid) dgettext("bison-runtime", Msgid)
#endif
#endif
#ifndef YY_
#define YY_(Msgid) Msgid
#endif
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__
#define YYUSE(E) ((void)(E))
#else
#define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && !defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                                            \
  _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuninitialized\"") \
      _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#define YY_ASSERT(E) ((void)(0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

#ifdef YYSTACK_USE_ALLOCA
#if YYSTACK_USE_ALLOCA
#ifdef __GNUC__
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if !defined _ALLOCA_H && !defined EXIT_SUCCESS
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
/* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif

#ifdef YYSTACK_ALLOC
/* Pacify GCC's 'empty if-body' warning.  */
#define YYSTACK_FREE(Ptr) \
  do { /* empty */        \
    ;                     \
  } while (0)
#ifndef YYSTACK_ALLOC_MAXIMUM
/* The OS might guarantee only one guard page at the bottom of the stack,
   and a page size can be as small as 4096 bytes.  So we cannot safely
   invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
   to allow for a few compiler-allocated temporary stack slots.  */
#define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#endif
#else
#define YYSTACK_ALLOC YYMALLOC
#define YYSTACK_FREE YYFREE
#ifndef YYSTACK_ALLOC_MAXIMUM
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && !defined EXIT_SUCCESS && \
     !((defined YYMALLOC || defined malloc) && (defined YYFREE || defined free)))
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#ifndef YYMALLOC
#define YYMALLOC malloc
#if !defined malloc && !defined EXIT_SUCCESS
void *malloc(YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#ifndef YYFREE
#define YYFREE free
#if !defined free && !defined EXIT_SUCCESS
void free(void *);      /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#endif
#endif /* !defined yyoverflow */

#if (!defined yyoverflow && (!defined __cplusplus || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc {
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
#define YYSTACK_GAP_MAXIMUM (YYSIZEOF(union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
#define YYSTACK_BYTES(N) ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE)) + YYSTACK_GAP_MAXIMUM)

#define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
#define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
  do {                                                                 \
    YYPTRDIFF_T yynewbytes;                                            \
    YYCOPY(&yyptr->Stack_alloc, Stack, yysize);                        \
    Stack = &yyptr->Stack_alloc;                                       \
    yynewbytes = yystacksize * YYSIZEOF(*Stack) + YYSTACK_GAP_MAXIMUM; \
    yyptr += yynewbytes / YYSIZEOF(*yyptr);                            \
  } while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
#ifndef YYCOPY
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(Dst, Src, Count) __builtin_memcpy(Dst, Src, YY_CAST(YYSIZE_T, (Count)) * sizeof(*(Src)))
#else
#define YYCOPY(Dst, Src, Count)         \
  do {                                  \
    YYPTRDIFF_T yyi;                    \
    for (yyi = 0; yyi < (Count); yyi++) \
      (Dst)[yyi] = (Src)[yyi];          \
  } while (0)
#endif
#endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL 16
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST 316

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS 52
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 25
/* YYNRULES -- Number of rules.  */
#define YYNRULES 97
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 228

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK 286

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX) \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  49, 2,  2,  2,  32, 2,  2,  43, 44, 39, 35, 45, 34, 2,  40, 2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    46, 51, 37, 36, 38, 50, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  47, 2,  48, 41, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  2,  2,  2,  2,  33, 2,  42, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2, 2,
    2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2, 3, 4, 5,
    6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint8 yyrline[] = {
    0,   83,  83,  84,  85,  86,  87,  88,  89,  90,  94,  94,  98,  105, 106, 107, 108, 109, 110, 111,
    112, 117, 118, 119, 123, 124, 128, 132, 133, 134, 135, 138, 139, 140, 141, 145, 146, 147, 148, 149,
    153, 154, 157, 158, 159, 163, 164, 165, 166, 171, 172, 173, 174, 178, 179, 183, 184, 185, 186, 191,
    192, 197, 198, 199, 200, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218,
    219, 220, 221, 222, 223, 224, 225, 226, 227, 231, 232, 234, 239, 240, 245, 246, 251, 252};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] = {"\"end of file\"",
                                      "error",
                                      "\"invalid token\"",
                                      "VPTT_dataequals",
                                      "VPTT_with_lookup",
                                      "VPTT_map",
                                      "VPTT_equiv",
                                      "VPTT_groupstar",
                                      "VPTT_and",
                                      "VPTT_macro",
                                      "VPTT_end_of_macro",
                                      "VPTT_or",
                                      "VPTT_not",
                                      "VPTT_hold_backward",
                                      "VPTT_look_forward",
                                      "VPTT_except",
                                      "VPTT_na",
                                      "VPTT_interpolate",
                                      "VPTT_raw",
                                      "VPTT_test_input",
                                      "VPTT_the_condition",
                                      "VPTT_implies",
                                      "VPTT_ge",
                                      "VPTT_le",
                                      "VPTT_ne",
                                      "VPTT_tabbed_array",
                                      "VPTT_eqend",
                                      "VPTT_number",
                                      "VPTT_literal",
                                      "VPTT_symbol",
                                      "VPTT_units_symbol",
                                      "VPTT_function",
                                      "'%'",
                                      "'|'",
                                      "'-'",
                                      "'+'",
                                      "'='",
                                      "'<'",
                                      "'>'",
                                      "'*'",
                                      "'/'",
                                      "'^'",
                                      "'~'",
                                      "'('",
                                      "')'",
                                      "','",
                                      "':'",
                                      "'['",
                                      "']'",
                                      "'!'",
                                      "'?'",
                                      "';'",
                                      "$accept",
                                      "fulleq",
                                      "macrostart",
                                      "$@1",
                                      "macroend",
                                      "eqn",
                                      "lhs",
                                      "var",
                                      "sublist",
                                      "symlist",
                                      "subdef",
                                      "unitsrange",
                                      "urangenum",
                                      "number",
                                      "units",
                                      "interpmode",
                                      "exceptlist",
                                      "mapsymlist",
                                      "maplist",
                                      "exprlist",
                                      "exp",
                                      "tablevals",
                                      "xytablevals",
                                      "xytablevec",
                                      "tablepairs",
                                      YY_NULLPTR};

static const char *yysymbol_name(yysymbol_kind_t yysymbol) {
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] = {0,   256, 257, 258, 259, 260, 261, 262, 263, 264, 265, 266, 267,
                                        268, 269, 270, 271, 272, 273, 274, 275, 276, 277, 278, 279, 280,
                                        281, 282, 283, 284, 285, 286, 37,  124, 45,  43,  61,  60,  62,
                                        42,  47,  94,  126, 40,  41,  44,  58,  91,  93,  33,  63,  59};
#endif

#define YYPACT_NINF (-208)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-1)

#define yytable_value_is_error(Yyn) 0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] = {
    66,  -208, -208, -208, -208, 50,   21,   -208, -208, 9,    0,    126,  16,   -2,   27,  -208, -208, 20,   260,
    3,   130,  -208, -208, 31,   -208, -208, -208, 54,   88,   -208, 108,  -1,   93,   82,  -208, -208, -208, -21,
    -10, -19,  32,   260,  -208, -208, -208, 31,   116,  260,  260,  260,  138,  201,  143, -208, 10,   201,  -208,
    144, 183,  91,   153,  -208, 111,  172,  174,  175,  -208, 31,   260,  187,  45,   90,  -208, -208, 188,  -208,
    128, -208, 177,  -208, -208, -208, -21,  -21,  -10,  185,  240,  207,  207,  112,  260, 260,  260,  260,  260,
    260, 260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  260,  -208, -208, 182, 91,   -208, -208, 91,
    189, -208, -33,  199,  -208, 204,  205,  -208, 222,  206,  -208, -10,  -208, -208, 208, -208, 77,   -208, 167,
    154, 241,  25,   25,   25,   207,  207,  25,   25,   25,   185,  185,  185,  46,   201, 201,  91,   209,  -208,
    91,  -208, 190,  211,  95,   224,  -208, 115,  -10,  -208, -208, 216,  217,  91,   215, -208, 27,   -208, 233,
    237, -10,  -208, 121,  51,   -208, 226,  91,   155,  227,  242,  244,  -10,  -208, 247, 249,  251,  252,  -208,
    27,  -208, -208, 250,  91,   253,  256,  -208, 169,  -208, 255,  -208, 91,   -208, 91,  257,  261,  91,   267,
    262, 264,  131,  91,   265,  259,  266,  135,  127,  91,   268,  174,  175,  270,  265, 140,  175,  271,  265};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    0,  3,  10, 12, 2,  24, 0,  4,  5,  0, 18, 21, 0, 0,  0,  25, 1,  0,  0,  0,  0,  51, 52, 0,  49, 50, 23, 22, 0,
    31, 0,  59, 27, 0,  45, 9,  8,  0,  0, 0,  35, 0, 66, 65, 68, 24, 0,  0,  0,  0,  67, 17, 0,  20, 13, 61, 42, 0,
    0,  0,  0,  94, 0,  0,  92, 89, 53, 0, 0,  0,  0, 0,  19, 28, 0,  26, 0,  41, 0,  40, 7,  6,  0,  0,  0,  84, 0,
    86, 87, 0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0, 0,  0,  0,  0,  0,  0,  64, 43, 44, 0,  0,  14, 15, 0,  0,  54,
    0,  0,  55, 0,  60, 33, 0,  29, 48, 0, 47, 46, 0, 72, 0,  70, 0,  83, 82, 80, 78, 81, 74, 73, 85, 77, 79, 75, 76,
    88, 0,  62, 63, 0,  0,  95, 0,  11, 0, 0,  0,  0, 30, 0,  0,  71, 69, 0,  0,  0,  0,  32, 0,  57, 0,  0,  0,  38,
    0,  0,  96, 0,  0,  0,  0,  0,  0,  0, 36, 0,  0, 0,  0,  56, 0,  34, 39, 0,  0,  0,  0,  97, 0,  37, 0,  16, 0,
    58, 0,  0,  0,  0,  0,  0,  0,  0,  0, 0,  0,  0, 0,  0,  0,  0,  93, 90, 0,  0,  0,  91, 0,  0};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] = {-208, -208, -208, -208, -208, -208, -208, 309, -18, -155, -208, -208, -83,
                                       -20,  -35,  -208, -208, -208, -208, -60,  11,  137, -208, 98,   -207};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] = {-1, 6,  7,  12, 8,   9,  10, 50, 15, 33, 31, 39, 78,
                                        79, 40, 26, 27, 120, 72, 54, 55, 62, 63, 64, 65};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] = {
    61,  128, 76,  18,  70,  66,  216, 52,  116, 34,  221, 153, 105, 179, 80,  41,  225, 56,  106, 42,  221, 16,  37,
    81,  57,  58,  130, 29,  53,  51,  43,  44,  45,  91,  46,  198, 19,  47,  48,  109, 77,  30,  159, 20,  71,  28,
    49,  126, 127, 115, 34,  17,  85,  35,  91,  105, 32,  92,  87,  88,  89,  106, 36,  37,  101, 102, 103, 38,  93,
    94,  95,  82,  83,  1,   118, 2,   3,   174, 14,  84,  96,  97,  98,  99,  100, 101, 102, 103, 119, 182, 150, 163,
    4,   151, 59,  5,   13,  14,  185, 67,  193, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
    146, 147, 148, 56,  121, 91,  161, 105, 92,  169, 57,  58,  74,  106, 164, 75,  68,  166, 122, 93,  94,  95,  69,
    170, 21,  22,  23,  73,  24,  25,  177, 96,  97,  98,  99,  100, 101, 102, 103, 56,  111, 131, 56,  188, 86,  172,
    57,  58,  173, 57,  58,  183, 82,  83,  184, 59,  107, 124, 59,  200, 91,  213, 60,  92,  214, 114, 90,  205, 219,
    206, 213, 104, 209, 226, 93,  94,  95,  215, 101, 102, 103, 110, 61,  222, 189, 74,  96,  97,  98,  99,  100, 101,
    102, 103, 91,  108, 162, 92,  203, 74,  91,  112, 123, 92,  113, 114, 117, 125, 93,  94,  95,  103, 149, 154, 93,
    94,  95,  152, 155, 167, 96,  97,  98,  99,  100, 101, 102, 103, 98,  99,  100, 101, 102, 103, 91,  156, 157, 41,
    160, 165, 158, 42,  168, 171, 175, 178, 176, 180, 93,  94,  95,  181, 43,  44,  45,  187, 46,  41,  190, 47,  48,
    42,  98,  99,  100, 101, 102, 103, 49,  129, 196, 191, 43,  44,  45,  194, 46,  192, 195, 47,  48,  197, 201, 199,
    202, 204, 210, 207, 49,  217, 208, 211, 212, 59,  11,  0,   218, 186, 223, 224, 220, 227};

static const yytype_int16 yycheck[] = {
    20,  84,  37,  3,   5,  23, 213, 4,   68,  30, 217, 44, 45,  168, 33,  12, 223, 27,  51, 16,  227, 0,   43,
    42,  34,  35,  86,  29, 25, 18,  27,  28,  29, 8,   31, 190, 36,  34,  35, 59,  50,  43, 125, 43,  45,  29,
    43,  82,  83,  67,  30, 42, 41,  33,  8,   45, 29,  11, 47,  48,  49,  51, 42,  43,  39, 40,  41,  47,  22,
    23,  24,  39,  40,  7,  29, 9,   10,  160, 47, 47,  34, 35,  36,  37,  38, 39,  40,  41, 43,  172, 110, 45,
    26,  113, 43,  29,  46, 47, 47,  45,  183, 90, 91,  92, 93,  94,  95,  96, 97,  98,  99, 100, 101, 102, 103,
    104, 105, 106, 27,  29, 8,  44,  45,  11,  29, 34,  35, 45,  51,  149, 48, 43,  152, 43, 22,  23,  24,  29,
    43,  13,  14,  15,  49, 17, 18,  165, 34,  35, 36,  37, 38,  39,  40,  41, 27,  44,  44, 27,  178, 43,  45,
    34,  35,  48,  34,  35, 45, 39,  40,  48,  43, 27,  44, 43,  194, 8,   45, 47,  11,  48, 45,  43,  202, 48,
    204, 45,  43,  207, 48, 22, 23,  24,  212, 39, 40,  41, 43,  217, 218, 44, 45,  34,  35, 36,  37,  38,  39,
    40,  41,  8,   27,  44, 11, 44,  45,  8,   44, 29,  11, 45,  45,  34,  45, 22,  23,  24, 41,  45,  29,  22,
    23,  24,  43,  29,  44, 34, 35,  36,  37,  38, 39,  40, 41,  36,  37,  38, 39,  40,  41, 8,   45,  29,  12,
    45,  45,  49,  16,  46, 34, 43,  45,  44,  29, 22,  23, 24,  29,  27,  28, 29,  44,  31, 12,  46,  34,  35,
    16,  36,  37,  38,  39, 40, 41,  43,  44,  34, 44,  27, 28,  29,  43,  31, 48,  44,  34, 35,  44,  44,  48,
    43,  45,  34,  45,  43, 45, 44,  44,  43,  43, 0,   -1, 45,  175, 45,  44, 217, 45};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  7,  9,  10, 26, 29, 53, 54, 56, 57, 58, 59, 55, 46, 47, 60, 0,  42, 3,  36, 43, 13, 14, 15, 17, 18, 67, 68, 29,
    29, 43, 62, 29, 61, 30, 33, 42, 43, 47, 63, 66, 12, 16, 27, 28, 29, 31, 34, 35, 43, 59, 72, 4,  25, 71, 72, 27, 34,
    35, 43, 47, 65, 73, 74, 75, 76, 60, 45, 43, 29, 5,  45, 70, 49, 45, 48, 66, 50, 64, 65, 33, 42, 39, 40, 47, 72, 43,
    72, 72, 72, 43, 8,  11, 22, 23, 24, 34, 35, 36, 37, 38, 39, 40, 41, 43, 45, 51, 27, 27, 65, 43, 44, 44, 45, 45, 60,
    71, 34, 29, 43, 69, 29, 43, 29, 44, 45, 66, 66, 64, 44, 71, 44, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
    72, 72, 72, 72, 45, 65, 65, 43, 44, 29, 29, 45, 29, 49, 64, 45, 44, 44, 45, 65, 45, 65, 44, 46, 29, 43, 34, 45, 48,
    64, 43, 44, 65, 45, 61, 29, 29, 64, 45, 48, 47, 73, 44, 65, 44, 46, 44, 48, 64, 43, 44, 34, 44, 61, 48, 65, 44, 43,
    44, 45, 65, 65, 45, 44, 65, 34, 44, 43, 45, 48, 65, 76, 45, 45, 48, 75, 76, 65, 45, 44, 76, 48, 45};

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] = {0,  52, 53, 53, 53, 53, 53, 53, 53, 53, 55, 54, 56, 57, 57, 57, 57, 57, 57, 57,
                                   57, 58, 58, 58, 59, 59, 60, 61, 61, 61, 61, 62, 62, 62, 62, 63, 63, 63, 63, 63,
                                   64, 64, 65, 65, 65, 66, 66, 66, 66, 67, 67, 67, 67, 68, 68, 69, 69, 69, 69, 70,
                                   70, 71, 71, 71, 71, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72, 72,
                                   72, 72, 72, 72, 72, 72, 72, 72, 72, 73, 73, 73, 74, 74, 75, 75, 76, 76};

/* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] = {0, 2, 1, 1, 1, 1, 4, 4, 3, 3, 0, 6, 1, 3, 4, 4,  10, 3, 1,  4, 3, 1, 2, 2, 1,
                                   2, 3, 1, 2, 3, 4, 1, 5, 3, 7, 1, 6, 8, 5, 7, 1,  1,  1, 2,  2, 1, 3, 3, 3, 1,
                                   1, 1, 1, 2, 3, 1, 5, 3, 7, 0, 2, 1, 3, 3, 2, 1,  1,  1, 1,  4, 3, 4, 3, 3, 3,
                                   3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 3, 2, 2, 3, 1, 15, 17, 1, 15, 1, 3, 5, 7};

enum { YYENOMEM = -2 };

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = YYEMPTY)

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab

#define YYRECOVERING() (!!yyerrstatus)

#define YYBACKUP(Token, Value)                      \
  do                                                \
    if (yychar == YYEMPTY) {                        \
      yychar = (Token);                             \
      yylval = (Value);                             \
      YYPOPSTACK(yylen);                            \
      yystate = *yyssp;                             \
      goto yybackup;                                \
    } else {                                        \
      yyerror(YY_("syntax error: cannot back up")); \
      YYERROR;                                      \
    }                                               \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* Enable debugging if requested.  */
#if YYDEBUG

#ifndef YYFPRINTF
#include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#define YYFPRINTF fprintf
#endif

#define YYDPRINTF(Args) \
  do {                  \
    if (yydebug)        \
      YYFPRINTF Args;   \
  } while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
#define YY_LOCATION_PRINT(File, Loc) ((void)0)
#endif

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location) \
  do {                                                \
    if (yydebug) {                                    \
      YYFPRINTF(stderr, "%s ", Title);                \
      yy_symbol_print(stderr, Kind, Value);           \
      YYFPRINTF(stderr, "\n");                        \
    }                                                 \
  } while (0)

/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void yy_symbol_value_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep) {
  FILE *yyoutput = yyo;
  YYUSE(yyoutput);
  if (!yyvaluep)
    return;
#ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT(yyo, yytoknum[yykind], *yyvaluep);
#endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void yy_symbol_print(FILE *yyo, yysymbol_kind_t yykind, YYSTYPE const *const yyvaluep) {
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep);
  YYFPRINTF(yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void yy_stack_print(yy_state_t *yybottom, yy_state_t *yytop) {
  YYFPRINTF(stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++) {
    int yybot = *yybottom;
    YYFPRINTF(stderr, " %d", yybot);
  }
  YYFPRINTF(stderr, "\n");
}

#define YY_STACK_PRINT(Bottom, Top)    \
  do {                                 \
    if (yydebug)                       \
      yy_stack_print((Bottom), (Top)); \
  } while (0)

/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void yy_reduce_print(yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule) {
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++) {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]), &yyvsp[(yyi + 1) - (yynrhs)]);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)              \
  do {                                     \
    if (yydebug)                           \
      yy_reduce_print(yyssp, yyvsp, Rule); \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
#define YYDPRINTF(Args) ((void)0)
#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
#define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void yydestruct(const char *yymsg, yysymbol_kind_t yykind, YYSTYPE *yyvaluep) {
  YYUSE(yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT(yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;

/*----------.
| yyparse.  |
`----------*/

int yyparse(void) {
  yy_state_fast_t yystate = 0;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus = 0;

  /* Refer to the stacks through separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* Their size.  */
  YYPTRDIFF_T yystacksize = YYINITDEPTH;

  /* The state stack: array, bottom, top.  */
  yy_state_t yyssa[YYINITDEPTH];
  yy_state_t *yyss = yyssa;
  yy_state_t *yyssp = yyss;

  /* The semantic value stack: array, bottom, top.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF((stderr, "Entering state %d\n", yystate));
  YY_ASSERT(0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST(yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT(yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
  {
    /* Get the current used size of the three stacks, in elements.  */
    YYPTRDIFF_T yysize = yyssp - yyss + 1;

#if defined yyoverflow
    {
      /* Give user a chance to reallocate the stack.  Use copies of
         these so that the &'s don't force the real ones into
         memory.  */
      yy_state_t *yyss1 = yyss;
      YYSTYPE *yyvs1 = yyvs;

      /* Each stack pointer address is followed by the size of the
         data in use in that stack, in bytes.  This used to be a
         conditional around just the two extra args, but that might
         be undefined if yyoverflow is a macro.  */
      yyoverflow(YY_("memory exhausted"), &yyss1, yysize * YYSIZEOF(*yyssp), &yyvs1, yysize * YYSIZEOF(*yyvsp),
                 &yystacksize);
      yyss = yyss1;
      yyvs = yyvs1;
    }
#else /* defined YYSTACK_RELOCATE */
    /* Extend the stack our own way.  */
    if (YYMAXDEPTH <= yystacksize)
      goto yyexhaustedlab;
    yystacksize *= 2;
    if (YYMAXDEPTH < yystacksize)
      yystacksize = YYMAXDEPTH;

    {
      yy_state_t *yyss1 = yyss;
      union yyalloc *yyptr = YY_CAST(union yyalloc *, YYSTACK_ALLOC(YY_CAST(YYSIZE_T, YYSTACK_BYTES(yystacksize))));
      if (!yyptr)
        goto yyexhaustedlab;
      YYSTACK_RELOCATE(yyss_alloc, yyss);
      YYSTACK_RELOCATE(yyvs_alloc, yyvs);
#undef YYSTACK_RELOCATE
      if (yyss1 != yyssa)
        YYSTACK_FREE(yyss1);
    }
#endif

    yyssp = yyss + yysize - 1;
    yyvsp = yyvs + yysize - 1;

    YY_IGNORE_USELESS_CAST_BEGIN
    YYDPRINTF((stderr, "Stack size increased to %ld\n", YY_CAST(long, yystacksize)));
    YY_IGNORE_USELESS_CAST_END

    if (yyss + yystacksize - 1 <= yyssp)
      YYABORT;
  }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default(yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY) {
    YYDPRINTF((stderr, "Reading a token\n"));
    yychar = yylex();
  }

  if (yychar <= YYEOF) {
    yychar = YYEOF;
    yytoken = YYSYMBOL_YYEOF;
    YYDPRINTF((stderr, "Now at end of input.\n"));
  } else if (yychar == YYerror) {
    /* The scanner already issued an error message, process directly
       to error recovery.  But do not keep the error token as
       lookahead, it is too special and may lead us to an endless
       loop in error recovery. */
    yychar = YYUNDEF;
    yytoken = YYSYMBOL_YYerror;
    goto yyerrlab1;
  } else {
    yytoken = YYTRANSLATE(yychar);
    YY_SYMBOL_PRINT("Next token is", yytoken, &yylval, &yylloc);
  }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0) {
    if (yytable_value_is_error(yyn))
      goto yyerrlab;
    yyn = -yyn;
    goto yyreduce;
  }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;

/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;

/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1 - yylen];

  YY_REDUCE_PRINT(yyn);
  switch (yyn) {
  case 2: /* fulleq: VPTT_eqend  */
#line 83 "VYacc.y"
  {
    return VPTT_eqend;
  }
#line 1309 "VYacc.tab.cpp"
  break;

  case 3: /* fulleq: VPTT_groupstar  */
#line 84 "VYacc.y"
  {
    return VPTT_groupstar;
  }
#line 1315 "VYacc.tab.cpp"
  break;

  case 4: /* fulleq: macrostart  */
#line 85 "VYacc.y"
  {
    return '|';
  }
#line 1321 "VYacc.tab.cpp"
  break;

  case 5: /* fulleq: macroend  */
#line 86 "VYacc.y"
  {
    return '|';
  }
#line 1327 "VYacc.tab.cpp"
  break;

  case 6: /* fulleq: eqn '~' unitsrange '~'  */
#line 87 "VYacc.y"
  {
    vpyy_addfulleq((yyvsp[-3].eqn), (yyvsp[-1].uni));
    return '~';
  }
#line 1333 "VYacc.tab.cpp"
  break;

  case 7: /* fulleq: eqn '~' unitsrange '|'  */
#line 88 "VYacc.y"
  {
    vpyy_addfulleq((yyvsp[-3].eqn), (yyvsp[-1].uni));
    return '|';
  }
#line 1339 "VYacc.tab.cpp"
  break;

  case 8: /* fulleq: eqn '~' '~'  */
#line 89 "VYacc.y"
  {
    vpyy_addfulleq((yyvsp[-2].eqn), NULL);
    return '~';
  }
#line 1345 "VYacc.tab.cpp"
  break;

  case 9: /* fulleq: eqn '~' '|'  */
#line 90 "VYacc.y"
  {
    vpyy_addfulleq((yyvsp[-2].eqn), NULL);
    return '|';
  }
#line 1351 "VYacc.tab.cpp"
  break;

  case 10: /* $@1: %empty  */
#line 94 "VYacc.y"
  {
    vpyy_macro_start();
  }
#line 1357 "VYacc.tab.cpp"
  break;

  case 11: /* macrostart: VPTT_macro $@1 VPTT_symbol '(' exprlist ')'  */
#line 94 "VYacc.y"
  {
    vpyy_macro_expression((yyvsp[-3].sym), (yyvsp[-1].exl));
  }
#line 1363 "VYacc.tab.cpp"
  break;

  case 12: /* macroend: VPTT_end_of_macro  */
#line 98 "VYacc.y"
  {
    (yyval.tok) = (yyvsp[0].tok);
    vpyy_macro_end();
  }
#line 1369 "VYacc.tab.cpp"
  break;

  case 13: /* eqn: lhs '=' exprlist  */
#line 105 "VYacc.y"
  {
    (yyval.eqn) = vpyy_addeq((yyvsp[-2].lhs), NULL, (yyvsp[0].exl), '=');
  }
#line 1375 "VYacc.tab.cpp"
  break;

  case 14: /* eqn: lhs '(' tablevals ')'  */
#line 106 "VYacc.y"
  {
    (yyval.eqn) = vpyy_add_lookup((yyvsp[-3].lhs), NULL, (yyvsp[-1].tbl), 0);
  }
#line 1381 "VYacc.tab.cpp"
  break;

  case 15: /* eqn: lhs '(' xytablevals ')'  */
#line 107 "VYacc.y"
  {
    (yyval.eqn) = vpyy_add_lookup((yyvsp[-3].lhs), NULL, (yyvsp[-1].tbl), 1);
  }
#line 1387 "VYacc.tab.cpp"
  break;

  case 16: /* eqn: lhs '=' VPTT_with_lookup '(' exp ',' '(' tablevals ')' ')'  */
#line 108 "VYacc.y"
  {
    (yyval.eqn) = vpyy_add_lookup((yyvsp[-9].lhs), (yyvsp[-5].exn), (yyvsp[-2].tbl), 0);
  }
#line 1393 "VYacc.tab.cpp"
  break;

  case 17: /* eqn: lhs VPTT_dataequals exp  */
#line 109 "VYacc.y"
  {
    (yyval.eqn) = vpyy_addeq((yyvsp[-2].lhs), (yyvsp[0].exn), NULL, VPTT_dataequals);
  }
#line 1399 "VYacc.tab.cpp"
  break;

  case 18: /* eqn: lhs  */
#line 110 "VYacc.y"
  {
    (yyval.eqn) = vpyy_add_lookup((yyvsp[0].lhs), NULL, NULL, 0);
  }
#line 1405 "VYacc.tab.cpp"
  break;

  case 19: /* eqn: VPTT_symbol ':' subdef maplist  */
#line 111 "VYacc.y"
  {
    (yyval.eqn) = vpyy_addeq(vpyy_addexceptinterp(vpyy_var_expression((yyvsp[-3].sym), NULL), NULL, 0),
                             (Expression *)vpyy_symlist_expression((yyvsp[-1].sml), (yyvsp[0].sml)), NULL, ':');
  }
#line 1411 "VYacc.tab.cpp"
  break;

  case 20: /* eqn: lhs '=' VPTT_tabbed_array  */
#line 112 "VYacc.y"
  {
    (yyval.eqn) = vpyy_addeq((yyvsp[-2].lhs), (yyvsp[0].exn), NULL, '=');
  }
#line 1417 "VYacc.tab.cpp"
  break;

  case 21: /* lhs: var  */
#line 117 "VYacc.y"
  {
    (yyval.lhs) = vpyy_addexceptinterp((yyvsp[0].var), NULL, 0);
  }
#line 1423 "VYacc.tab.cpp"
  break;

  case 22: /* lhs: var exceptlist  */
#line 118 "VYacc.y"
  {
    (yyval.lhs) = vpyy_addexceptinterp((yyvsp[-1].var), (yyvsp[0].sll), 0);
  }
#line 1429 "VYacc.tab.cpp"
  break;

  case 23: /* lhs: var interpmode  */
#line 119 "VYacc.y"
  {
    (yyval.lhs) = vpyy_addexceptinterp((yyvsp[-1].var), NULL, (yyvsp[0].tok));
  }
#line 1435 "VYacc.tab.cpp"
  break;

  case 24: /* var: VPTT_symbol  */
#line 123 "VYacc.y"
  {
    (yyval.var) = vpyy_var_expression((yyvsp[0].sym), NULL);
  }
#line 1441 "VYacc.tab.cpp"
  break;

  case 25: /* var: VPTT_symbol sublist  */
#line 124 "VYacc.y"
  {
    (yyval.var) = vpyy_var_expression((yyvsp[-1].sym), (yyvsp[0].sml));
  }
#line 1447 "VYacc.tab.cpp"
  break;

  case 26: /* sublist: '[' symlist ']'  */
#line 128 "VYacc.y"
  {
    (yyval.sml) = (yyvsp[-1].sml);
  }
#line 1453 "VYacc.tab.cpp"
  break;

  case 27: /* symlist: VPTT_symbol  */
#line 132 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist(NULL, (yyvsp[0].sym), 0, NULL);
  }
#line 1459 "VYacc.tab.cpp"
  break;

  case 28: /* symlist: VPTT_symbol '!'  */
#line 133 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist(NULL, (yyvsp[-1].sym), 1, NULL);
  }
#line 1465 "VYacc.tab.cpp"
  break;

  case 29: /* symlist: symlist ',' VPTT_symbol  */
#line 134 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist((yyvsp[-2].sml), (yyvsp[0].sym), 0, NULL);
  }
#line 1471 "VYacc.tab.cpp"
  break;

  case 30: /* symlist: symlist ',' VPTT_symbol '!'  */
#line 135 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist((yyvsp[-3].sml), (yyvsp[-1].sym), 1, NULL);
  }
#line 1477 "VYacc.tab.cpp"
  break;

  case 31: /* subdef: VPTT_symbol  */
#line 138 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist(NULL, (yyvsp[0].sym), 0, NULL);
  }
#line 1483 "VYacc.tab.cpp"
  break;

  case 32: /* subdef: '(' VPTT_symbol '-' VPTT_symbol ')'  */
#line 139 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist(NULL, (yyvsp[-3].sym), 0, (yyvsp[-1].sym));
  }
#line 1489 "VYacc.tab.cpp"
  break;

  case 33: /* subdef: subdef ',' VPTT_symbol  */
#line 140 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist((yyvsp[-2].sml), (yyvsp[0].sym), 0, NULL);
  }
#line 1495 "VYacc.tab.cpp"
  break;

  case 34: /* subdef: subdef ',' '(' VPTT_symbol '-' VPTT_symbol ')'  */
#line 141 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist((yyvsp[-6].sml), (yyvsp[-3].sym), 0, (yyvsp[-1].sym));
  }
#line 1501 "VYacc.tab.cpp"
  break;

  case 35: /* unitsrange: units  */
#line 145 "VYacc.y"
  {
    (yyval.uni) = (yyvsp[0].uni);
  }
#line 1507 "VYacc.tab.cpp"
  break;

  case 36: /* unitsrange: units '[' urangenum ',' urangenum ']'  */
#line 146 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsrange((yyvsp[-5].uni), (yyvsp[-3].num), (yyvsp[-1].num), -1);
  }
#line 1513 "VYacc.tab.cpp"
  break;

  case 37: /* unitsrange: units '[' urangenum ',' urangenum ',' urangenum ']'  */
#line 147 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsrange((yyvsp[-7].uni), (yyvsp[-5].num), (yyvsp[-3].num), (yyvsp[-1].num));
  }
#line 1519 "VYacc.tab.cpp"
  break;

  case 38: /* unitsrange: '[' urangenum ',' urangenum ']'  */
#line 148 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsrange(NULL, (yyvsp[-3].num), (yyvsp[-1].num), -1);
  }
#line 1525 "VYacc.tab.cpp"
  break;

  case 39: /* unitsrange: '[' urangenum ',' urangenum ',' urangenum ']'  */
#line 149 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsrange(NULL, (yyvsp[-5].num), (yyvsp[-3].num), (yyvsp[-1].num));
  }
#line 1531 "VYacc.tab.cpp"
  break;

  case 40: /* urangenum: number  */
#line 153 "VYacc.y"
  {
    (yyval.num) = (yyvsp[0].num);
  }
#line 1537 "VYacc.tab.cpp"
  break;

  case 41: /* urangenum: '?'  */
#line 154 "VYacc.y"
  {
    (yyval.num) = -1e30;
  }
#line 1543 "VYacc.tab.cpp"
  break;

  case 42: /* number: VPTT_number  */
#line 157 "VYacc.y"
  {
    (yyval.num) = (yyvsp[0].num);
  }
#line 1549 "VYacc.tab.cpp"
  break;

  case 43: /* number: '-' VPTT_number  */
#line 158 "VYacc.y"
  {
    (yyval.num) = -(yyvsp[0].num);
  }
#line 1555 "VYacc.tab.cpp"
  break;

  case 44: /* number: '+' VPTT_number  */
#line 159 "VYacc.y"
  {
    (yyval.num) = (yyvsp[0].num);
  }
#line 1561 "VYacc.tab.cpp"
  break;

  case 45: /* units: VPTT_units_symbol  */
#line 163 "VYacc.y"
  {
    (yyval.uni) = (yyvsp[0].uni);
  }
#line 1567 "VYacc.tab.cpp"
  break;

  case 46: /* units: units '/' units  */
#line 164 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsdiv((yyvsp[-2].uni), (yyvsp[0].uni));
  }
#line 1573 "VYacc.tab.cpp"
  break;

  case 47: /* units: units '*' units  */
#line 165 "VYacc.y"
  {
    (yyval.uni) = vpyy_unitsmult((yyvsp[-2].uni), (yyvsp[0].uni));
  }
#line 1579 "VYacc.tab.cpp"
  break;

  case 48: /* units: '(' units ')'  */
#line 166 "VYacc.y"
  {
    (yyval.uni) = (yyvsp[-1].uni);
  }
#line 1585 "VYacc.tab.cpp"
  break;

  case 49: /* interpmode: VPTT_interpolate  */
#line 171 "VYacc.y"
  {
    (yyval.tok) = (yyvsp[0].tok);
  }
#line 1591 "VYacc.tab.cpp"
  break;

  case 50: /* interpmode: VPTT_raw  */
#line 172 "VYacc.y"
  {
    (yyval.tok) = (yyvsp[0].tok);
  }
#line 1597 "VYacc.tab.cpp"
  break;

  case 51: /* interpmode: VPTT_hold_backward  */
#line 173 "VYacc.y"
  {
    (yyval.tok) = (yyvsp[0].tok);
  }
#line 1603 "VYacc.tab.cpp"
  break;

  case 52: /* interpmode: VPTT_look_forward  */
#line 174 "VYacc.y"
  {
    (yyval.tok) = (yyvsp[0].tok);
  }
#line 1609 "VYacc.tab.cpp"
  break;

  case 53: /* exceptlist: VPTT_except sublist  */
#line 178 "VYacc.y"
  {
    (yyval.sll) = vpyy_chain_sublist(NULL, (yyvsp[0].sml));
  }
#line 1615 "VYacc.tab.cpp"
  break;

  case 54: /* exceptlist: exceptlist ',' sublist  */
#line 179 "VYacc.y"
  {
    vpyy_chain_sublist((yyvsp[-2].sll), (yyvsp[0].sml));
    (yyval.sll) = (yyvsp[-2].sll);
  }
#line 1621 "VYacc.tab.cpp"
  break;

  case 55: /* mapsymlist: VPTT_symbol  */
#line 183 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist(NULL, (yyvsp[0].sym), 0, NULL);
  }
#line 1627 "VYacc.tab.cpp"
  break;

  case 56: /* mapsymlist: '(' VPTT_symbol ':' symlist ')'  */
#line 184 "VYacc.y"
  {
    (yyval.sml) = vpyy_mapsymlist(NULL, (yyvsp[-3].sym), (yyvsp[-1].sml));
  }
#line 1633 "VYacc.tab.cpp"
  break;

  case 57: /* mapsymlist: mapsymlist ',' VPTT_symbol  */
#line 185 "VYacc.y"
  {
    (yyval.sml) = vpyy_symlist((yyvsp[-2].sml), (yyvsp[0].sym), 0, NULL);
  }
#line 1639 "VYacc.tab.cpp"
  break;

  case 58: /* mapsymlist: mapsymlist ',' '(' VPTT_symbol ':' symlist ')'  */
#line 186 "VYacc.y"
  {
    (yyval.sml) = vpyy_mapsymlist((yyvsp[-6].sml), (yyvsp[-3].sym), (yyvsp[-1].sml));
  }
#line 1645 "VYacc.tab.cpp"
  break;

  case 59: /* maplist: %empty  */
#line 191 "VYacc.y"
  {
    (yyval.sml) = NULL;
  }
#line 1651 "VYacc.tab.cpp"
  break;

  case 60: /* maplist: VPTT_map mapsymlist  */
#line 192 "VYacc.y"
  {
    (yyval.sml) = (yyvsp[0].sml);
  }
#line 1657 "VYacc.tab.cpp"
  break;

  case 61: /* exprlist: exp  */
#line 197 "VYacc.y"
  {
    (yyval.exl) = vpyy_chain_exprlist(NULL, (yyvsp[0].exn));
  }
#line 1663 "VYacc.tab.cpp"
  break;

  case 62: /* exprlist: exprlist ',' exp  */
#line 198 "VYacc.y"
  {
    (yyval.exl) = vpyy_chain_exprlist((yyvsp[-2].exl), (yyvsp[0].exn));
  }
#line 1669 "VYacc.tab.cpp"
  break;

  case 63: /* exprlist: exprlist ';' exp  */
#line 199 "VYacc.y"
  {
    (yyval.exl) = vpyy_chain_exprlist((yyvsp[-2].exl), (yyvsp[0].exn));
  }
#line 1675 "VYacc.tab.cpp"
  break;

  case 64: /* exprlist: exprlist ';'  */
#line 200 "VYacc.y"
  {
    (yyval.exl) = (yyvsp[-1].exl);
  }
#line 1681 "VYacc.tab.cpp"
  break;

  case 65: /* exp: VPTT_number  */
#line 204 "VYacc.y"
  {
    (yyval.exn) = vpyy_num_expression((yyvsp[0].num));
  }
#line 1687 "VYacc.tab.cpp"
  break;

  case 66: /* exp: VPTT_na  */
#line 205 "VYacc.y"
  {
    (yyval.exn) = vpyy_num_expression(-1E38);
  }
#line 1693 "VYacc.tab.cpp"
  break;

  case 67: /* exp: var  */
#line 206 "VYacc.y"
  {
    (yyval.exn) = (Expression *)(yyvsp[0].var);
  }
#line 1699 "VYacc.tab.cpp"
  break;

  case 68: /* exp: VPTT_literal  */
#line 207 "VYacc.y"
  {
    (yyval.exn) = vpyy_literal_expression((yyvsp[0].lit));
  }
#line 1705 "VYacc.tab.cpp"
  break;

  case 69: /* exp: var '(' exp ')'  */
#line 208 "VYacc.y"
  {
    (yyval.exn) = vpyy_lookup_expression((yyvsp[-3].var), (yyvsp[-1].exn));
  }
#line 1711 "VYacc.tab.cpp"
  break;

  case 70: /* exp: '(' exp ')'  */
#line 209 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('(', (yyvsp[-1].exn), NULL);
  }
#line 1717 "VYacc.tab.cpp"
  break;

  case 71: /* exp: VPTT_function '(' exprlist ')'  */
#line 210 "VYacc.y"
  {
    (yyval.exn) = vpyy_function_expression((yyvsp[-3].fnc), (yyvsp[-1].exl));
  }
#line 1723 "VYacc.tab.cpp"
  break;

  case 72: /* exp: VPTT_function '(' ')'  */
#line 211 "VYacc.y"
  {
    (yyval.exn) = vpyy_function_expression((yyvsp[-2].fnc), NULL);
  }
#line 1729 "VYacc.tab.cpp"
  break;

  case 73: /* exp: exp '+' exp  */
#line 212 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('+', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1735 "VYacc.tab.cpp"
  break;

  case 74: /* exp: exp '-' exp  */
#line 213 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('-', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1741 "VYacc.tab.cpp"
  break;

  case 75: /* exp: exp '*' exp  */
#line 214 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('*', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1747 "VYacc.tab.cpp"
  break;

  case 76: /* exp: exp '/' exp  */
#line 215 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('/', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1753 "VYacc.tab.cpp"
  break;

  case 77: /* exp: exp '<' exp  */
#line 216 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('<', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1759 "VYacc.tab.cpp"
  break;

  case 78: /* exp: exp VPTT_le exp  */
#line 217 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_le, (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1765 "VYacc.tab.cpp"
  break;

  case 79: /* exp: exp '>' exp  */
#line 218 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('>', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1771 "VYacc.tab.cpp"
  break;

  case 80: /* exp: exp VPTT_ge exp  */
#line 219 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_ge, (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1777 "VYacc.tab.cpp"
  break;

  case 81: /* exp: exp VPTT_ne exp  */
#line 220 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_ne, (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1783 "VYacc.tab.cpp"
  break;

  case 82: /* exp: exp VPTT_or exp  */
#line 221 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_or, (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1789 "VYacc.tab.cpp"
  break;

  case 83: /* exp: exp VPTT_and exp  */
#line 222 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_and, (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1795 "VYacc.tab.cpp"
  break;

  case 84: /* exp: VPTT_not exp  */
#line 223 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression(VPTT_not, (yyvsp[0].exn), NULL);
  }
#line 1801 "VYacc.tab.cpp"
  break;

  case 85: /* exp: exp '=' exp  */
#line 224 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('=', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1807 "VYacc.tab.cpp"
  break;

  case 86: /* exp: '-' exp  */
#line 225 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('-', NULL, (yyvsp[0].exn));
  }
#line 1813 "VYacc.tab.cpp"
  break;

  case 87: /* exp: '+' exp  */
#line 226 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('+', NULL, (yyvsp[0].exn));
  }
#line 1819 "VYacc.tab.cpp"
  break;

  case 88: /* exp: exp '^' exp  */
#line 227 "VYacc.y"
  {
    (yyval.exn) = vpyy_operator_expression('^', (yyvsp[-2].exn), (yyvsp[0].exn));
  }
#line 1825 "VYacc.tab.cpp"
  break;

  case 89: /* tablevals: tablepairs  */
#line 231 "VYacc.y"
  {
    (yyval.tbl) = (yyvsp[0].tbl);
  }
#line 1831 "VYacc.tab.cpp"
  break;

  case 90: /* tablevals: '[' '(' number ',' number ')' '-' '(' number ',' number ')' ']' ',' tablepairs  */
#line 233 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablerange((yyvsp[0].tbl), (yyvsp[-12].num), (yyvsp[-10].num), (yyvsp[-6].num), (yyvsp[-4].num));
  }
#line 1837 "VYacc.tab.cpp"
  break;

  case 91: /* tablevals: '[' '(' number ',' number ')' '-' '(' number ',' number ')' ',' tablepairs ']' ',' tablepairs
            */
#line 235 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablerange((yyvsp[0].tbl), (yyvsp[-14].num), (yyvsp[-12].num), (yyvsp[-8].num), (yyvsp[-6].num));
  }
#line 1843 "VYacc.tab.cpp"
  break;

  case 92: /* xytablevals: xytablevec  */
#line 239 "VYacc.y"
  {
    (yyval.tbl) = (yyvsp[0].tbl);
  }
#line 1849 "VYacc.tab.cpp"
  break;

  case 93: /* xytablevals: '[' '(' number ',' number ')' '-' '(' number ',' number ')' ']' ',' xytablevec  */
#line 241 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablerange((yyvsp[0].tbl), (yyvsp[-12].num), (yyvsp[-10].num), (yyvsp[-6].num), (yyvsp[-4].num));
  }
#line 1855 "VYacc.tab.cpp"
  break;

  case 94: /* xytablevec: number  */
#line 245 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablevec(NULL, (yyvsp[0].num));
  }
#line 1861 "VYacc.tab.cpp"
  break;

  case 95: /* xytablevec: xytablevec ',' number  */
#line 246 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablevec((yyvsp[-2].tbl), (yyvsp[0].num));
  }
#line 1867 "VYacc.tab.cpp"
  break;

  case 96: /* tablepairs: '(' number ',' number ')'  */
#line 251 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablepair(NULL, (yyvsp[-3].num), (yyvsp[-1].num));
  }
#line 1873 "VYacc.tab.cpp"
  break;

  case 97: /* tablepairs: tablepairs ',' '(' number ',' number ')'  */
#line 252 "VYacc.y"
  {
    (yyval.tbl) = vpyy_tablepair((yyvsp[-6].tbl), (yyvsp[-3].num), (yyvsp[-1].num));
  }
#line 1879 "VYacc.tab.cpp"
  break;

#line 1883 "VYacc.tab.cpp"

  default:
    break;
  }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT("-> $$ =", YY_CAST(yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK(yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp ? yytable[yyi] : yydefgoto[yylhs]);
  }

  goto yynewstate;

/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE(yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus) {
    ++yynerrs;
    yyerror(YY_("syntax error"));
  }

  if (yyerrstatus == 3) {
    /* If just tried and failed to reuse lookahead token after an
       error, discard it.  */

    if (yychar <= YYEOF) {
      /* Return failure if at end of input.  */
      if (yychar == YYEOF)
        YYABORT;
    } else {
      yydestruct("Error: discarding", yytoken, &yylval);
      yychar = YYEMPTY;
    }
  }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;

/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK(yylen);
  yylen = 0;
  YY_STACK_PRINT(yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;

/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3; /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;) {
    yyn = yypact[yystate];
    if (!yypact_value_is_default(yyn)) {
      yyn += YYSYMBOL_YYerror;
      if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror) {
        yyn = yytable[yyn];
        if (0 < yyn)
          break;
      }
    }

    /* Pop the current state because it cannot handle the error token.  */
    if (yyssp == yyss)
      YYABORT;

    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp);
    YYPOPSTACK(1);
    yystate = *yyssp;
    YY_STACK_PRINT(yyss, yyssp);
  }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Shift the error token.  */
  YY_SYMBOL_PRINT("Shifting", YY_ACCESSING_SYMBOL(yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;

/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror(YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif

/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY) {
    /* Make sure we have latest lookahead translation.  See comments at
       user semantic actions for why this is necessary.  */
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval);
  }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss) {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp);
    YYPOPSTACK(1);
  }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE(yyss);
#endif

  return yyresult;
}

#line 258 "VYacc.y"
