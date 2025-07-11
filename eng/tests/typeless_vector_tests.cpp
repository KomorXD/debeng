#include <gtest/gtest.h>
#include "eng/containers/typeless_vec.hpp"

using namespace eng::cont;

TEST(TypelessVector, AppendPop) {
    TypelessVector vv = TypelessVector::create<int>();
    (void)vv.append(5);
    ASSERT_EQ(vv.at<int>(0), 5) << "Unexpected first element";

    (void)vv.append(7);
    (void)vv.append(9);
    (void)vv.append(11);
    ASSERT_EQ(vv.at<int>(1), 7) << "Unexpected 2nd element";
    ASSERT_EQ(vv.at<int>(2), 9) << "Unexpected 3rd element";
    ASSERT_EQ(vv.at<int>(3), 11) << "Unexpected 4th element";
    ASSERT_EQ(vv.count, 4) << "Unexpected element count";

    vv.pop();
    vv.pop();
    ASSERT_EQ(vv.count, 2) << "Unexpected element count";

    (void)vv.append(13);
    (void)vv.append(15);
    ASSERT_EQ(vv.at<int>(3), 15) << "Unexpected last element";

    vv.pop();
}

TEST(TypelessVector, Inserting) {
    TypelessVector vv = TypelessVector::create<int>();
    (void)vv.append(1);
    (void)vv.append(2);
    (void)vv.append(3);
    (void)vv.append(4);
    (void)vv.append(5);

    (void)vv.insert<int>(20, 0);
    (void)vv.insert<int>(30, 3);
    (void)vv.insert<int>(40, vv.count);

    ASSERT_EQ(vv.at<int>(0), 20) << "Incorrect 1st element";
    ASSERT_EQ(vv.at<int>(3), 30) << "Incorrect middle element";
    ASSERT_EQ(vv.at<int>(vv.count - 1), 40) << "Incorrect last element";
}

TEST(TypelessVector, Erasing) {
    TypelessVector vv = TypelessVector::create<int>();
    (void)vv.append(1);
    (void)vv.append(2);
    (void)vv.append(3);
    (void)vv.append(4);
    (void)vv.append(5);

    vv.erase<int>(2);
    vv.erase<int>(0);
    vv.erase<int>(vv.count - 1);

    ASSERT_EQ(vv.at<int>(0), 2) << "Incorrect 1st element";
    ASSERT_EQ(vv.at<int>(1), 4) << "Incorrect middle element";
    ASSERT_EQ(vv.at<int>(vv.count - 1), 4) << "Incorrect last element";
}

TEST(TypelessVector, AppendPopRaw) {
    TypelessVector vv = TypelessVector::create<int>();
    int vals[] = {5, 7, 9, 11, 13, 15};
    (void)vv.append_raw(&vals[0]);
    ASSERT_EQ(*(int *)vv.at_raw(0), 5) << "Unexpected first element";

    (void)vv.append_raw(&vals[1]);
    (void)vv.append_raw(&vals[2]);
    (void)vv.append_raw(&vals[3]);
    ASSERT_EQ(*(int *)vv.at_raw(1), 7) << "Unexpected 2nd element";
    ASSERT_EQ(*(int *)vv.at_raw(2), 9) << "Unexpected 3rd element";
    ASSERT_EQ(*(int *)vv.at_raw(3), 11) << "Unexpected 4th element";
    ASSERT_EQ(vv.count, 4) << "Unexpected element count";

    vv.pop();
    vv.pop();
    ASSERT_EQ(vv.count, 2) << "Unexpected element count";

    (void)vv.append_raw(&vals[4]);
    (void)vv.append_raw(&vals[5]);
    ASSERT_EQ(*(int *)vv.at_raw(3), 15) << "Unexpected last element";

    vv.pop();
}

TEST(TypelessVector, InsertingRaw) {
    TypelessVector vv = TypelessVector::create<int>();
    int vals[] = {1, 2, 3, 4, 5, 20, 30, 40};
    (void)vv.append_raw(&vals[0]);
    (void)vv.append_raw(&vals[1]);
    (void)vv.append_raw(&vals[2]);
    (void)vv.append_raw(&vals[3]);
    (void)vv.append_raw(&vals[4]);

    (void)vv.insert_raw(&vals[5], 0);
    (void)vv.insert_raw(&vals[6], 3);
    (void)vv.insert_raw(&vals[7], vv.count);

    ASSERT_EQ(*(int *)vv.at_raw(1), vals[0]) << "Insert broke storage";
    ASSERT_EQ(*(int *)vv.at_raw(0), 20) << "Incorrect 1st element";
    ASSERT_EQ(*(int *)vv.at_raw(3), 30) << "Incorrect middle element";
    ASSERT_EQ(*(int *)vv.at_raw(vv.count - 1), 40) << "Incorrect last element";
}

TEST(TypelessVector, ErasingRaw) {
    TypelessVector vv = TypelessVector::create<int>();
    int vals[] = {1, 2, 3, 4, 5, 6};
    (void)vv.append_raw(&vals[0]);
    (void)vv.append_raw(&vals[1]);
    (void)vv.append_raw(&vals[2]);
    (void)vv.append_raw(&vals[3]);
    (void)vv.append_raw(&vals[4]);
    (void)vv.append_raw(&vals[5]);

    vv.erase_raw(2);
    vv.erase_raw(0);
    vv.erase_raw(vv.count - 1);

    ASSERT_EQ(*(int *)vv.at_raw(0), vals[1]) << "Insert broke storage";
    ASSERT_EQ(*(int *)vv.at_raw(1), vals[3]) << "Incorrect 1st element";
    ASSERT_EQ(*(int *)vv.at_raw(2), vals[4]) << "Incorrect middle element";
    ASSERT_EQ(*(int *)vv.at_raw(vv.count - 1), vals[4])
        << "Incorrect last element";
}
