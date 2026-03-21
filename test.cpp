#define MU_IMPLEMENTATION
#include "mu.h"

#include <cstdio>

struct VisitState
{
    size_t count;
    size_t sum;
};

static bool visit_collect(size_t value, void* user)
{
    VisitState* state = (VisitState*)user;
    state->count += 1;
    state->sum += value;
    return true;
}

int main()
{
    int  total  = 0;
    int  passed = 0;
    bool failed = false;

    auto check = [&](bool condition, const char* label) {
        ++total;
        if(condition)
        {
            ++passed;
            std::printf("[PASS] %s\n", label);
        }
        else
        {
            failed = true;
            std::printf("[FAIL] %s\n", label);
        }
    };

    // baseline creation / set / test / reset / clear / fill / copy
    mu_bitset* bs = mu_bitset_create_with_capacity(130);
    check(bs != nullptr, "create_with_capacity(130)");
    mu_bitset_set(bs, 63);
    mu_bitset_set(bs, 64);
    mu_bitset_set(bs, 129);
    mu_bitset_enable_bit(bs, 0);

    mu_bitset_print(bs);
    std::printf("Initial bs = ");
    mu_bitset_print(bs);
    std::printf("\n");
    check(mu_bitset_test(bs, 0), "set/test bit 0");
    check(mu_bitset_test(bs, 63), "set/test bit 63");
    check(mu_bitset_test(bs, 64), "set/test bit 64");
    check(mu_bitset_test(bs, 129), "set/test bit 129");
    check(!mu_bitset_test(bs, 65), "unset bit 65 remains false");

    mu_bitset_reset(bs, 64);
    check(!mu_bitset_test(bs, 64), "reset bit 64");
    mu_bitset_disable_bit(bs, 63);
    check(!mu_bitset_get_bit(bs, 63), "disable/get bit 63");
    mu_bitset_set_bit(bs, 63, true);
    check(mu_bitset_get_bit(bs, 63), "set_bit(true) sets bit 63");
    mu_bitset_set_bit(bs, 63, false);
    check(!mu_bitset_get_bit(bs, 63), "set_bit(false) resets bit 63");

    mu_bitset_clear(bs);
    check(!mu_bitset_test(bs, 0), "clear resets bit 0");
    check(!mu_bitset_test(bs, 63), "clear resets bit 63");
    check(!mu_bitset_test(bs, 129), "clear resets bit 129");

    mu_bitset_fill(bs);
    check(mu_bitset_test(bs, 0), "fill sets bit 0");
    check(mu_bitset_test(bs, 63), "fill sets bit 63");

    mu_bitset* copy = mu_bitset_copy(bs);
    check(copy != nullptr, "copy bitset");
    check(mu_bitset_test(copy, 0) == mu_bitset_test(bs, 0), "copy parity bit 0");
    check(mu_bitset_test(copy, 63) == mu_bitset_test(bs, 63), "copy parity bit 63");
    check(mu_bitset_test(copy, 129) == mu_bitset_test(bs, 129), "copy parity bit 129");

    // grow / resize / trim
    mu_bitset* g = mu_bitset_create();
    check(g != nullptr, "create empty bitset g");
    check(mu_bitset_grow(g, 3), "grow g to 3 words");
    check(mu_bitset_test(g, 0) == false, "grow zero-initializes new words");
    mu_bitset_set(g, 130);
    check(mu_bitset_test(g, 130), "set/test bit 130 in g");
    check(!mu_bitset_grow(g, 2), "grow rejects shrinking request");

    check(mu_bitset_resize_words(g, 6), "resize g to 6 words");
    check(mu_bitset_test(g, 130), "resize keeps existing bits");
    check(mu_bitset_resize_words(g, 2), "resize g down to 2 words");
    check(!mu_bitset_test(g, 130), "downsize drops out-of-range bits");
    mu_bitset_set(g, 1);
    check(mu_bitset_trim(g), "trim g");
    check(mu_bitset_test(g, 1), "trim keeps remaining set bits");

    // prepare sets for set-algebra APIs
    mu_bitset* a = mu_bitset_create();
    mu_bitset* b = mu_bitset_create();
    check(a != nullptr, "create bitset a");
    check(b != nullptr, "create bitset b");
    mu_bitset_set(a, 1);
    mu_bitset_set(a, 3);
    mu_bitset_set(a, 64);
    mu_bitset_set(a, 130);

    mu_bitset_set(b, 3);
    mu_bitset_set(b, 5);
    mu_bitset_set(b, 64);
    mu_bitset_set(b, 200);

    // count / min / max / empty
    check(mu_bitset_count(a) == 4, "count(a) == 4");
    check(mu_bitset_minimum(a) == 1, "minimum(a) == 1");
    check(mu_bitset_maximum(a) == 130, "maximum(a) == 130");
    check(!mu_bitset_empty(a), "a is not empty");
    mu_bitset* e = mu_bitset_create();
    check(e != nullptr, "create bitset e");
    check(mu_bitset_empty(e), "e is empty");
    check(mu_bitset_minimum(e) == SIZE_MAX, "minimum(e) == SIZE_MAX");
    check(mu_bitset_maximum(e) == 0, "maximum(e) == 0");

    // equality with different backing sizes but same logical set bits
    mu_bitset* eq1 = mu_bitset_create();
    mu_bitset* eq2 = mu_bitset_create();
    check(eq1 != nullptr, "create bitset eq1");
    check(eq2 != nullptr, "create bitset eq2");
    mu_bitset_set(eq1, 5);
    mu_bitset_set(eq2, 5);
    check(mu_bitset_resize_words(eq2, 5), "resize eq2 to 5 words");
    check(mu_bitset_equal(eq1, eq2), "equal handles trailing zero words");
    mu_bitset_set(eq2, 66);
    check(!mu_bitset_equal(eq1, eq2), "equal detects difference");

    // disjoint / intersect / contains_all
    check(!mu_bitsets_disjoint(a, b), "a and b are not disjoint");
    check(mu_bitsets_intersect(a, b), "a and b intersect");
    mu_bitset* d = mu_bitset_create();
    check(d != nullptr, "create bitset d");
    mu_bitset_set(d, 999);
    check(mu_bitsets_disjoint(a, d), "a and d are disjoint");
    check(!mu_bitsets_intersect(a, d), "a and d do not intersect");

    mu_bitset* u = mu_bitset_copy(a);
    check(u != nullptr, "copy a into u");
    check(mu_bitset_inplace_union(u, b), "inplace_union(u, b)");
    check(mu_bitset_contains_all(u, a), "u contains all bits of a");
    check(mu_bitset_contains_all(u, b), "u contains all bits of b");
    check(!mu_bitset_contains_all(a, b), "a does not contain all bits of b");
    check(mu_bitset_union_count(a, b) == 6, "union_count(a, b) == 6");

    // intersection and intersection_count
    mu_bitset* i = mu_bitset_copy(a);
    check(i != nullptr, "copy a into i");
    mu_bitset_inplace_intersection(i, b);
    check(mu_bitset_count(i) == 2, "count(intersection) == 2");
    check(mu_bitset_test(i, 3), "intersection contains bit 3");
    check(mu_bitset_test(i, 64), "intersection contains bit 64");
    check(mu_bitset_intersection_count(a, b) == 2, "intersection_count(a, b) == 2");

    // difference and difference_count
    mu_bitset* diff = mu_bitset_copy(a);
    check(diff != nullptr, "copy a into diff");
    mu_bitset_inplace_difference(diff, b);
    check(mu_bitset_count(diff) == 2, "count(difference) == 2");
    check(mu_bitset_test(diff, 1), "difference contains bit 1");
    check(mu_bitset_test(diff, 130), "difference contains bit 130");
    check(mu_bitset_difference_count(a, b) == 2, "difference_count(a, b) == 2");

    // symmetric difference and symmetric_difference_count
    mu_bitset* sx = mu_bitset_copy(a);
    check(sx != nullptr, "copy a into sx");
    check(mu_bitset_inplace_symmetric_difference(sx, b), "inplace_symmetric_difference(sx, b)");
    check(mu_bitset_count(sx) == 4, "count(symmetric_difference) == 4");
    check(mu_bitset_test(sx, 1), "symmetric difference contains bit 1");
    check(mu_bitset_test(sx, 5), "symmetric difference contains bit 5");
    check(mu_bitset_test(sx, 130), "symmetric difference contains bit 130");
    check(mu_bitset_test(sx, 200), "symmetric difference contains bit 200");
    check(mu_bitset_symmetric_difference_count(a, b) == 4, "symmetric_difference_count(a, b) == 4");

    // out-parameter bitwise operators
    mu_bitset* out = mu_bitset_create();
    check(out != nullptr, "create bitset out");
    check(mu_bitset_or(out, a, b), "bitset_or(out, a, b)");
    check(mu_bitset_count(out) == 6, "or count == 6");
    check(mu_bitset_xor(out, a, b), "bitset_xor(out, a, b)");
    check(mu_bitset_count(out) == 4, "xor count == 4");
    check(mu_bitset_and(out, a, b), "bitset_and(out, a, b)");
    check(mu_bitset_count(out) == 2, "and count == 2");

    check(mu_bitset_not(out, a), "bitset_not(out, a)");
    check(!mu_bitset_get_bit(out, 1), "not clears previously set bit");
    check(mu_bitset_get_bit(out, 2), "not sets previously unset bit");

    // assign operators
    mu_bitset* asg = mu_bitset_copy(a);
    check(asg != nullptr, "copy a into asg");
    check(mu_bitset_or_assign(asg, b), "or_assign(asg, b)");
    check(mu_bitset_count(asg) == 6, "or_assign count == 6");
    check(mu_bitset_and_assign(asg, b), "and_assign(asg, b)");
    check(mu_bitset_count(asg) == 4, "and_assign count == 4");
    check(mu_bitset_xor_assign(asg, b), "xor_assign(asg, b)");
    check(mu_bitset_count(asg) == 0, "xor_assign count == 0");

    // traversal helpers
    VisitState all = {0, 0};
    mu_bitset_traverse(a, visit_collect, &all);
    check(all.count == 4, "traverse visits all set bits");
    check(all.sum == (1 + 3 + 64 + 130), "traverse sum is correct");

    VisitState range = {0, 0};
    mu_bitset_traverse_range(a, visit_collect, &range, 2, 64);
    check(range.count == 2, "traverse_range count in [2,66)");
    check(range.sum == (3 + 64), "traverse_range sum in [2,66)");

    mu_bitset_free(asg);
    mu_bitset_free(out);
    mu_bitset_free(sx);
    mu_bitset_free(diff);
    mu_bitset_free(i);
    mu_bitset_free(u);
    mu_bitset_free(d);
    mu_bitset_free(eq2);
    mu_bitset_free(eq1);
    mu_bitset_free(e);
    mu_bitset_free(b);
    mu_bitset_free(a);
    mu_bitset_free(g);
    mu_bitset_free(copy);
    mu_bitset_free(bs);

    std::printf("\nSummary: %d/%d checks passed\n", passed, total);
    if(failed)
    {
        std::puts("test.cpp: some mu_bitset tests failed");
        return 1;
    }

    std::puts("test.cpp: all mu_bitset tests passed");
    return 0;
}
