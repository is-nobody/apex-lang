#ifdef _WIN32
__declspec(dllexport) long add(long a, long b) {
    return a + b;
}
#else
long add(long a, long b) {
    return a + b;
}
#endif