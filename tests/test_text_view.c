//
// Written by tomer on 02-Oct-26.
//

#include <assert.h>
#include <stdlib.h>

#include "Harness.h"
#include "TextView.h"

static void test_text_view_equals(void) {
    TextView sv_literal[] = {
        text_view_value_of_literal(""),
        text_view_value_of_literal("a"),
        text_view_value_of_literal("ab"),
        text_view_value_of_literal("abc"),
        text_view_value_of_literal("bc"),
        text_view_value_of_literal("b"),
        text_view_value_of_literal("c"),
        text_view_value_of_literal("ac"),
        text_view_value_of_literal("babc"),
        text_view_value_of_literal("abcc"),
        text_view_value_of_literal("a\0a"),
        text_view_value_of_literal("b\0b"),
        text_view_value_of_literal("c\0"),
        text_view_value_of_literal("\0d"),
        text_view_value_of_literal("\0")
    };
    TextView sv_reg[] = {
        {.data = "", .length = 0},
        {.data = "a", .length = 1},
        {.data = "ab", .length = 2},
        {.data = "abc", .length = 3},
        {.data = "bc", .length = 2},
        {.data = "b", .length = 1},
        {.data = "c", .length = 1},
        {.data = "ac", .length = 2},
        {.data = "babc", .length = 4},
        {.data = "abcc", .length = 4},
        {.data = "a\0a", .length = 3},
        {.data = "b\0b", .length = 3},
        {.data = "c\0", .length = 2},
        {.data = "\0d", .length = 2},
        {.data = "\0", .length = 1}
    };
    CHECK(sv_literal[10].length == 3); // "a\0a"
    CHECK(sv_literal[11].length == 3); // "b\0b"
    CHECK(sv_literal[12].length == 2); // "c\0"
    CHECK(sv_literal[13].length == 2); // "\0d"
    CHECK(sv_literal[14].length == 1); // "\0"

    {
        TextView sv1 = text_view_value_of_literal("a\0b                     c  ");
        TextView sv2 = text_view_value_of_literal("a\0b                     c ");
        TextView sv3 = text_view_value_of_literal("a\0b                     c");
        CHECK(!text_view_equals(sv1, sv2));
        CHECK(!text_view_equals(sv2, sv3));
        CHECK(!text_view_equals(sv1, sv3));
    }

    _Static_assert(sizeof(sv_literal) == sizeof(sv_reg), "sv_literal and sv_reg must hold the same number of views");
    const int N = sizeof(sv_literal) / sizeof(TextView);
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j)
            if (i == j)
                CHECK(text_view_equals(sv_literal[i], sv_reg[j]));
            else
                CHECK(!text_view_equals(sv_literal[i], sv_reg[j]));

    {
        TextView sv1 = {.data = "abc", .length = 3};
        TextView sv2 = {.data = "ab", .length = 2};
        CHECK(!text_view_equals(sv1, sv2));
        sv1.length = 2;
        CHECK(text_view_equals(sv1, sv2));
    }
    {
        TextView sv1 = {.data = "abc", .length = 2};
        TextView sv2 = {.data = "ab", .length = 0};
        CHECK(!text_view_equals(sv1, sv2));
        sv1.length = 0;
        CHECK(text_view_equals(sv1, sv2));
    }
}
static void test_text_view_list_duplicate(void) {
    {
        TextView sv1 = {.data = "", .length = 0};
        TextView* sv_ptr = text_view_list_duplicate(&sv1);
        CHECK(text_view_equals(sv1, *sv_ptr));
        free(sv_ptr);
    }
    {
        TextView sv1 = {.data = "abc", .length = 0};
        TextView* sv_ptr = text_view_list_duplicate(&sv1);
        CHECK(text_view_equals(sv1, *sv_ptr));
        free(sv_ptr);
    }
    {
        TextView sv1 = {.data = "abc", .length = 2};
        TextView* sv_ptr = text_view_list_duplicate(&sv1);
        CHECK(text_view_equals(sv1, *sv_ptr));
        free(sv_ptr);
    }
    {
        TextView sv1 = {.data = "a\0bc", .length = 2};
        TextView* sv_ptr = text_view_list_duplicate(&sv1);
        CHECK(text_view_equals(sv1, *sv_ptr));
        free(sv_ptr);
    }
    {
        TextView sv1 = text_view_value_of_literal("a\0b                     c  ");
        TextView* sv_ptr = text_view_list_duplicate(&sv1);
        CHECK(text_view_equals(sv1, *sv_ptr));
        free(sv_ptr);
    }
    {
        TextView sv1 = text_view_value_of_literal("a\0b                     c  ");
        TextView sv2 = text_view_value_of_literal("a\0b                     c ");
        TextView sv3 = text_view_value_of_literal("a\0b                     c");

        TextView* sv_ptr1 = text_view_list_duplicate(&sv1);
        TextView* sv_ptr2 = text_view_list_duplicate(&sv2);
        TextView* sv_ptr3 = text_view_list_duplicate(&sv3);

        CHECK(text_view_equals(sv1, *sv_ptr1));
        CHECK(text_view_equals(sv2, *sv_ptr2));
        CHECK(text_view_equals(sv3, *sv_ptr3));
        free(sv_ptr1);
        free(sv_ptr2);
        free(sv_ptr3);
    }
}

int main(const int argc, char** const argv) {
    harness_init(argc, argv);
    RUN_TEST(test_text_view_equals);
    RUN_TEST(test_text_view_list_duplicate);
    return harness_finish();
}
