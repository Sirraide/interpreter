#include <interpreter/interp.hh>

#define r(x) _registers_[x + 1]
#define f(...) ((u64(*)(__VA_ARGS__))function.handle)
#define u5 u64, u64, u64, u64, u64
#define u10 u5, u5
#define r5(x) r(x), r(x + 2), r(x + 3), r(x + 4), r(x + 5)
#define r10(x) r5(x), r5(x + 5)

void interp::interpreter::do_library_call_unsafe(interp::interpreter::library_function& function) {
    switch (function.num_params) {
        case 0: r(0) = f() (); break;
        case 1: r(0) = f(u64) (r(1)); break;
        case 2: r(0) = f(u64, u64) (r(1), r(2)); break;
        case 3: r(0) = f(u64, u64, u64) (r(1), r(2), r(3)); break;
        case 4: r(0) = f(u64, u64, u64, u64) (r(1), r(2), r(3), r(4)); break;
        case 5: r(0) = f(u5) (r5(1)); break;
        case 6: r(0) = f(u5, u64) (r5(1), r(6)); break;
        case 7: r(0) = f(u5, u64, u64) (r5(1), r(6), r(7)); break;
        case 8: r(0) = f(u5, u64, u64, u64) (r5(1), r(6), r(7), r(8)); break;
        case 9: r(0) = f(u5, u64, u64, u64, u64) (r5(1), r(6), r(7), r(8), r(9)); break;
        case 10: r(0) = f(u10) (r10(1)); break;
        case 11: r(0) = f(u10, u64) (r10(1), r(11)); break;
        case 12: r(0) = f(u10, u64, u64) (r10(1), r(11), r(12)); break;
        case 13: r(0) = f(u10, u64, u64, u64) (r10(1), r(11), r(12), r(13)); break;
        case 14: r(0) = f(u10, u64, u64, u64, u64) (r10(1), r(11), r(12), r(13), r(14)); break;
        case 15: r(0) = f(u10, u5) (r10(1), r5(11)); break;
        case 16: r(0) = f(u10, u5, u64) (r10(1), r5(11), r(16)); break;
        case 17: r(0) = f(u10, u5, u64, u64) (r10(1), r5(11), r(16), r(17)); break;
        case 18: r(0) = f(u10, u5, u64, u64, u64) (r10(1), r5(11), r(16), r(17), r(18)); break;
        case 19: r(0) = f(u10, u5, u64, u64, u64, u64) (r10(1), r5(11), r(16), r(17), r(18), r(19)); break;
        case 20: r(0) = f(u10, u10) (r10(1), r10(2)); break;
        case 21: r(0) = f(u10, u10, u64) (r10(1), r10(2), r(21)); break;
        case 22: r(0) = f(u10, u10, u64, u64) (r10(1), r10(2), r(21), r(22)); break;
        case 23: r(0) = f(u10, u10, u64, u64, u64) (r10(1), r10(2), r(21), r(22), r(23)); break;
        case 24: r(0) = f(u10, u10, u64, u64, u64, u64) (r10(1), r10(2), r(21), r(22), r(23), r(24)); break;
        case 25: r(0) = f(u10, u10, u5) (r10(1), r10(2), r5(21)); break;
        case 26: r(0) = f(u10, u10, u5, u64) (r10(1), r10(2), r5(21), r(26)); break;
        case 27: r(0) = f(u10, u10, u5, u64, u64) (r10(1), r10(2), r5(21), r(26), r(27)); break;
        case 28: r(0) = f(u10, u10, u5, u64, u64, u64) (r10(1), r10(2), r5(21), r(26), r(27), r(28)); break;
        case 29: r(0) = f(u10, u10, u5, u64, u64, u64, u64) (r10(1), r10(2), r5(21), r(26), r(27), r(28), r(29)); break;
        case 30: r(0) = f(u10, u10, u10) (r10(1), r10(2), r10(3)); break;
        case 31: r(0) = f(u10, u10, u10, u64) (r10(1), r10(2), r10(3), r(31)); break;
        case 32: r(0) = f(u10, u10, u10, u64, u64) (r10(1), r10(2), r10(3), r(31), r(32)); break;
        case 33: r(0) = f(u10, u10, u10, u64, u64, u64) (r10(1), r10(2), r10(3), r(31), r(32), r(33)); break;
        case 34: r(0) = f(u10, u10, u10, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r(31), r(32), r(33), r(34)); break;
        case 35: r(0) = f(u10, u10, u10, u5) (r10(1), r10(2), r10(3), r5(31)); break;
        case 36: r(0) = f(u10, u10, u10, u5, u64) (r10(1), r10(2), r10(3), r5(31), r(36)); break;
        case 37: r(0) = f(u10, u10, u10, u5, u64, u64) (r10(1), r10(2), r10(3), r5(31), r(36), r(37)); break;
        case 38: r(0) = f(u10, u10, u10, u5, u64, u64, u64) (r10(1), r10(2), r10(3), r5(31), r(36), r(37), r(38)); break;
        case 39: r(0) = f(u10, u10, u10, u5, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r5(31), r(36), r(37), r(38), r(39)); break;
        case 40: r(0) = f(u10, u10, u10, u10) (r10(1), r10(2), r10(3), r10(4)); break;
        case 41: r(0) = f(u10, u10, u10, u10, u64) (r10(1), r10(2), r10(3), r10(4), r(41)); break;
        case 42: r(0) = f(u10, u10, u10, u10, u64, u64) (r10(1), r10(2), r10(3), r10(4), r(41), r(42)); break;
        case 43: r(0) = f(u10, u10, u10, u10, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r(41), r(42), r(43)); break;
        case 44: r(0) = f(u10, u10, u10, u10, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r(41), r(42), r(43), r(44)); break;
        case 45: r(0) = f(u10, u10, u10, u10, u5) (r10(1), r10(2), r10(3), r10(4), r5(41)); break;
        case 46: r(0) = f(u10, u10, u10, u10, u5, u64) (r10(1), r10(2), r10(3), r10(4), r5(41), r(46)); break;
        case 47: r(0) = f(u10, u10, u10, u10, u5, u64, u64) (r10(1), r10(2), r10(3), r10(4), r5(41), r(46), r(47)); break;
        case 48: r(0) = f(u10, u10, u10, u10, u5, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r5(41), r(46), r(47), r(48)); break;
        case 49: r(0) = f(u10, u10, u10, u10, u5, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r5(41), r(46), r(47), r(48), r(49)); break;
        case 50: r(0) = f(u10, u10, u10, u10, u10) (r10(1), r10(2), r10(3), r10(4), r10(5)); break;
        case 51: r(0) = f(u10, u10, u10, u10, u10, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r(51)); break;
        case 52: r(0) = f(u10, u10, u10, u10, u10, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r(51), r(52)); break;
        case 53: r(0) = f(u10, u10, u10, u10, u10, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r(51), r(52), r(53)); break;
        case 54: r(0) = f(u10, u10, u10, u10, u10, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r(51), r(52), r(53), r(54)); break;
        case 55: r(0) = f(u10, u10, u10, u10, u10, u5) (r10(1), r10(2), r10(3), r10(4), r10(5), r5(51)); break;
        case 56: r(0) = f(u10, u10, u10, u10, u10, u5, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r5(51), r(56)); break;
        case 57: r(0) = f(u10, u10, u10, u10, u10, u5, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r5(51), r(56), r(57)); break;
        case 58: r(0) = f(u10, u10, u10, u10, u10, u5, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r5(51), r(56), r(57), r(58)); break;
        case 59: r(0) = f(u10, u10, u10, u10, u10, u5, u64, u64, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r5(51), r(56), r(57), r(58), r(59)); break;
        case 60: r(0) = f(u10, u10, u10, u10, u10, u10) (r10(1), r10(2), r10(3), r10(4), r10(5), r10(6)); break;
        case 61: r(0) = f(u10, u10, u10, u10, u10, u10, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r10(6), r(61)); break;
        case 62: r(0) = f(u10, u10, u10, u10, u10, u10, u64, u64) (r10(1), r10(2), r10(3), r10(4), r10(5), r10(6), r(61), r(62)); break;
        default: throw error("Cannot call a function with more than 62 arguments");
    }
}