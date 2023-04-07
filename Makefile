# use g++ with libraries
# requires g++8.3 or higher
# any change in *.h.in file will cause all *.cpp files to compile
# to debug only one file, e.g. test_a.cpp, use `make test_a.o link`

#======== options =========
# compiler [g++|clang++|icpc|icpx]
opt_compiler = g++
# debug mode
opt_debug = true
# address sanitizer (only for g++ dynamic debug build)
opt_asan = $(opt_debug)
# c++ standard [c++11|gnu++11]
opt_std = c++11
# static link (not all libs supported)
opt_static = false
# use Boost lib
opt_boost = false
# use SQLiteCpp
opt_sqlitecpp = true
#==========================

$(info ) $(info ) $(info ) $(info ) $(info ) $(info )

# === Debug / Release ===
ifeq ($(opt_debug), true)
    $(info Build: Debug)
    debug_flag = -g
    ifeq ($(opt_compiler), g++)
        debug_flag = -g -ftrapv $(asan_flag)
    endif
else
    $(info Build: Release)
    release_flag = -O3 -D NDEBUG
endif

# Address Sanitizer
ifeq ($(opt_asan), true)
    ifeq ($(opt_compiler), g++)
        ifeq ($(opt_static), false)
            $(info Address Sanitizer: on)
            asan_flag = -fsanitize=address -static-libasan -D SLS_USE_ASAN
        else
            $(info Address Sanitizer: off)
        endif
    else
        $(info Address Sanitizer: off)
    endif
else
    $(info Address Sanitizer: off)
endif

# === Boost ===
ifeq ($(opt_boost), true)
    boost_flag = -D SLS_USE_BOOST
    ifeq ($(opt_static), false)
        $(info Boost: dynamic)
        boost_lib = -l:libboost_system.so -l:libboost_filesystem.so
    else
        $(info Boost: static)
        boost_lib = -l:libboost_system.a -l:libboost_filesystem.a
    endif
else
    $(info Boost: off)
endif

# === SQLiteCpp ===
ifeq ($(opt_sqlitecpp), true)
    opt_sqlite = false
    sqlitecpp_flag = -D SLS_USE_SQLITECPP -D SLS_USE_SQLITE
    $(info SQLiteCpp: static)
    sqlitecpp_lib = -l:libSQLiteCpp.a -l:libSQLiteCpp-sqlite3.a
else
    $(info SQLiteCpp: off)
endif


# === compiler flags ===
ifeq ($(opt_compiler), g++)
    compiler_flag = -std=$(opt_std) -Wall -Wno-cpp -Wno-reorder -Wno-misleading-indentation -fmax-errors=5 -fopenmp
endif

$(info  )$(info  )$(info  )$(info  )
# ---------------------------------------------------------

# all flags
flags = $(compiler_flag) $(debug_flag) $(release_flag) $(mkl_flag) $(boost_flag) $(arb_flag) $(sqlitecpp_flag)
# -pedantic # show more warnings

# all libs
ifeq ($(opt_static), true)
    static_flag = -static
endif
libs = $(static_flag) $(arpack_lib) $(mplapack_lib) $(gsl_lib) $(mkl_lib) $(lapacke_lib) $(cblas_lib) $(arb_lib) $(boost_lib) $(matfile_lib) $(sqlite_lib) $(sqlitecpp_lib) $(quad_math_lib) -l pthread -l dl

# subfolders of SLISC, including SLISC, e.g. "SLISC/:SLISC/prec/:...:SLISC/lin/"
in_paths = $(shell find SLISC -maxdepth 1 -type d -printf "../%p/:" | head -c -2)/

# number of cpu
Ncpu = $(shell getconf _NPROCESSORS_ONLN)

# default target
PhysWikiScan: main.o # link
	@printf "\n\n   --- link ---\n\n"
	$(opt_compiler) $(flags) -o PhysWikiScan main.o $(libs)

link: # force link
	$(opt_compiler) $(flags) -o PhysWikiScan main.o $(libs)

clean:
	rm -f *.o *.x

main.o: main.cpp lib/PhysWikiScan.h
	$(opt_compiler) $(flags) -c main.cpp
