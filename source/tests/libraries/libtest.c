// source/tests/libraries/libtest.c
// Implementation of Test File for FFI Tests for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#ifdef _WIN32
__declspec(dllexport) long add(long a, long b) {
    return a + b;
}
#else
long add(long a, long b) {
    return a + b;
}
#endif