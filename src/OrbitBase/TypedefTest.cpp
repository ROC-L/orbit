// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <absl/hash/hash_testing.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <tuple>
#include <utility>
#include <vector>

#include "OrbitBase/Typedef.h"

namespace {
struct MyTypeTag {};

struct Integer {
  [[nodiscard]] Integer Add(const Integer& other) const { return {value + other.value}; }
  int value{};
};

struct A {
  int value{};
};

struct B : public A {};

struct C {
  int value{};
  explicit C(const A& a) : value(a.value) {}
};

struct D {
  int value{};
  explicit D(A&& a) : value(std::move(a.value)) {}
  D(const A& a) = delete;
};

}  // namespace

static int Add(int i, int j) { return i + j; }

namespace orbit_base {

template <typename T>
using MyType = Typedef<MyTypeTag, T>;

template <typename T>
using MyConstType = Typedef<MyTypeTag, const T>;

TEST(TypedefTest, DefaultConstructorInitializesPrimitives) {
  MyType<int> wrapped;
  EXPECT_EQ(*wrapped, 0);
}

TEST(TypedefTest, CanInstantiate) {
  const int kConstInt = 1;
  MyType<int> wrapper_of_const(kConstInt);
  EXPECT_EQ(*wrapper_of_const, kConstInt);

  MyType<int> copy_of_wrapper(wrapper_of_const);
  EXPECT_EQ(*copy_of_wrapper, kConstInt);

  constexpr int kConstexprInt = 1;
  MyType<int> wrapper_of_constexpr(kConstexprInt);
  EXPECT_EQ(*wrapper_of_constexpr, kConstexprInt);

  int non_const = 1;
  MyType<int> wrapper_of_non_const(non_const);
  EXPECT_EQ(*wrapper_of_non_const, non_const);

  MyType<int> wrapper_of_literal(1);
  EXPECT_EQ(*wrapper_of_literal, 1);

  MyType<std::string> wrapper_of_string("foo");
  EXPECT_EQ(*wrapper_of_string, "foo");

  MyType<std::unique_ptr<int>> wrapper_of_unique_ptr(std::make_unique<int>(kConstInt));
  EXPECT_EQ(**wrapper_of_unique_ptr, kConstInt);

  {
    MyConstType<int> const_wrapper(kConstInt);
    MyType<int> from_const(const_wrapper);
    EXPECT_EQ(*from_const, kConstInt);
  }

  {
    MyType<int> non_const(kConstInt);
    MyConstType<int> from_non_const(non_const);
    EXPECT_EQ(*from_non_const, kConstInt);
  }

  MyType<std::mutex> wrapper_of_mutex(std::in_place);  // test it compiles
  std::ignore = wrapper_of_mutex;
}

TEST(TypedefTest, ImplicitConversionIsCorrect) {
  const int kValue = 1;

  {
    const MyType<B> wrapped_b(B{{kValue}});
    const MyType<A> wrapped_a(wrapped_b);
    EXPECT_EQ(wrapped_a->value, kValue);
  }

  {
    const MyType<B> wrapped_b(B{{kValue}});

    bool is_called = false;
    int value_called_on{};
    auto take_const_ref = [&is_called, &value_called_on](const MyType<A>& a) {
      is_called = true;
      value_called_on = a->value;
    };

    take_const_ref(wrapped_b);
    EXPECT_TRUE(is_called);
    EXPECT_EQ(value_called_on, kValue);
  }

  {
    const MyType<B> wrapped_b(B{{kValue}});

    bool is_called = false;
    int value_called_on{};
    auto take_const_ref = [&is_called, &value_called_on](const MyType<A> a) {
      is_called = true;
      value_called_on = a->value;
    };

    take_const_ref(wrapped_b);
    EXPECT_TRUE(is_called);
    EXPECT_EQ(value_called_on, kValue);
  }

  {
    MyType<B> wrapped_b(B{{kValue}});

    bool is_called = false;
    int value_called_on{};
    auto take_rvalue_ref = [&is_called, &value_called_on](MyType<A>&& a) {
      is_called = true;
      value_called_on = a->value;
    };

    take_rvalue_ref(std::move(wrapped_b));
    EXPECT_TRUE(is_called);
    EXPECT_EQ(value_called_on, kValue);
  }

  {
    MyType<A> wrapped_a(A{kValue});
    MyType<C> wrapped_c(wrapped_a);
    EXPECT_EQ(wrapped_c->value, kValue);
  }

  {
    MyType<A> wrapped_a(A{kValue});
    MyType<D> wrapped_c(std::move(wrapped_a));
    EXPECT_EQ(wrapped_c->value, kValue);
  }
}

TEST(TypedefTest, AssignmentIsCorrect) {
  const int kValue = 1;
  const int kValueOther = 2;
  {
    MyType<A> wrapped_a(A{kValue});
    MyType<A> wrapped_a_other(A{kValueOther});
    wrapped_a_other = wrapped_a;
    EXPECT_EQ(wrapped_a_other->value, kValue);
  }

  {
    MyType<A> wrapped_a(A{kValue});
    MyType<A> wrapped_a_other(A{kValueOther});
    wrapped_a_other = std::move(wrapped_a);
    EXPECT_EQ(wrapped_a_other->value, kValue);
  }

  {
    MyType<B> wrapped_b(B{{kValue}});
    MyType<A> wrapped_a_other(A{kValueOther});
    wrapped_a_other = wrapped_b;
    EXPECT_EQ(wrapped_a_other->value, kValue);
  }

  {
    MyType<B> wrapped_b(B{{kValue}});
    MyType<A> wrapped_a_other(A{kValueOther});
    wrapped_a_other = std::move(wrapped_b);
    EXPECT_EQ(wrapped_a_other->value, kValue);
  }

  {
    MyConstType<int> wrapped_a(kValue);
    MyConstType<int> wrapped_b(kValueOther);
    wrapped_b = wrapped_a;
    EXPECT_EQ(*wrapped_b, kValue);
  }

  {
    MyType<int> wrapped_a(kValue);
    MyConstType<int> wrapped_b(kValueOther);
    wrapped_b = wrapped_a;
    EXPECT_EQ(*wrapped_b, kValue);
  }

  {
    MyConstType<int> wrapped_a(kValue);
    MyType<int> wrapped_b(kValueOther);
    wrapped_b = wrapped_a;
    EXPECT_EQ(*wrapped_b, kValue);
  }
}

TEST(TypedefTest, CallIsCorrect) {
  const int kFirst = 1;
  const int kSecond = 2;
  const int kSum = kFirst + kSecond;

  const MyType<int> kFirstWrapped(kFirst);
  const MyType<int> kSecondWrapped(kSecond);

  {
    auto add = [](int i, int j) { return i + j; };
    const MyType<int> sum_wrapped = LiftAndApply(add, kFirstWrapped, kSecondWrapped);
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    auto add = [](int& i, int j) {
      int sum = i + j;
      i = j;
      return sum;
    };

    MyType<int> first(kFirst);
    MyType<int> second(kSecond);
    const MyType<int> sum_wrapped = LiftAndApply(add, first, second);
    EXPECT_EQ(*sum_wrapped, kSum);
    EXPECT_EQ(*first, kSecond);
    EXPECT_EQ(*second, kSecond);
  }

  {
    auto add = [](int&& i, int&& j) { return i + j; };

    MyType<int> first(kFirst);
    MyType<int> second(kSecond);
    const MyType<int> sum_wrapped = LiftAndApply(add, std::move(first), std::move(second));
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    auto add = [](const int& i, int&& j) { return i + j; };

    MyType<int> second(kSecond);
    const MyType<int> sum_wrapped = LiftAndApply(add, kFirstWrapped, std::move(second));
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    auto add = [](const std::unique_ptr<int>& i, const std::unique_ptr<int>& j) { return *i + *j; };
    MyType<std::unique_ptr<int>> first(std::make_unique<int>(kFirst));
    MyType<std::unique_ptr<int>> second(std::make_unique<int>(kSecond));
    const MyType<int> sum_wrapped = LiftAndApply(add, first, second);
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    auto add = [](const int& i, const int& j) { return i + j; };
    const MyType<int> sum_wrapped = LiftAndApply(add, kFirstWrapped, kSecondWrapped);
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    const MyType<int> sum_wrapped = LiftAndApply(Add, kFirstWrapped, kSecondWrapped);
    EXPECT_EQ(*sum_wrapped, kSum);
  }

  {
    bool was_called = false;
    int was_called_with = 0;
    auto returns_void = [&was_called, &was_called_with](int i) {
      was_called = true;
      was_called_with = i;
    };
    const MyType<void> void_wrapped = LiftAndApply(returns_void, kFirstWrapped);
    std::ignore = void_wrapped;
    EXPECT_TRUE(was_called);
    EXPECT_EQ(was_called_with, kFirst);
  }

  {
    MyType<Integer> first(Integer{kFirst});
    MyType<Integer> second(Integer{kSecond});
    MyType<Integer> sum_wrapped = LiftAndApply(&Integer::Add, first, second);
    EXPECT_EQ(sum_wrapped->value, kSum);
  }
}

TEST(Typedef, HashIsCorrect) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {MyType<int>(1), MyType<int>(0), MyType<int>(-1), MyType<int>(10)}));

  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {MyType<std::string>("A"), MyType<std::string>("B"), MyType<std::string>(""),
       MyType<std::string>("ABB")}));
}

TEST(Typedef, ComparisonIsCorrect) {
  constexpr int kLesser = 1;
  constexpr int kGreater = 2;
  EXPECT_EQ(MyType<int>(kLesser), MyType<int>(kLesser));
  EXPECT_NE(MyType<int>(kLesser), MyType<int>(kGreater));
  EXPECT_GE(MyType<int>(kLesser), MyType<int>(kLesser));
  EXPECT_GE(MyType<int>(kGreater), MyType<int>(kLesser));
  EXPECT_LE(MyType<int>(kLesser), MyType<int>(kLesser));
  EXPECT_LE(MyType<int>(kLesser), MyType<int>(kGreater));
  EXPECT_LT(MyType<int>(kLesser), MyType<int>(kGreater));
  EXPECT_GT(MyType<int>(kGreater), MyType<int>(kLesser));
}

struct WrapperWithPlusTag : PlusTag<WrapperWithPlusTag> {};

template <typename T>
using WrapperWithPlus = Typedef<WrapperWithPlusTag, T>;

constexpr int kAValue = 1;
constexpr int kBValue = 2;

TEST(Typedef, WrapperWithPlusHasPlus) {
  WrapperWithPlus<int> a(kAValue);
  WrapperWithPlus<int> b(kBValue);
  EXPECT_EQ(*(a + b), kAValue + kBValue);
}

TEST(Typedef, WrapperWithPlusHasPlusAndPromotes) {
  constexpr int kInt = 1;
  constexpr float kFloat = 0.5;
  WrapperWithPlus<int> a(kInt);
  WrapperWithPlus<float> b(kFloat);
  WrapperWithPlus<float> result = a + b;
  EXPECT_EQ(*result, kInt + kFloat);
}

TEST(Typedef, WrapperWithPlusHasPlusAndConvertsArgument) {
  constexpr std::chrono::nanoseconds kNanos(1000);
  constexpr std::chrono::microseconds kMicros(1);
  WrapperWithPlus<std::chrono::nanoseconds> a(kNanos);
  WrapperWithPlus<std::chrono::microseconds> b(kMicros);
  EXPECT_EQ(*(a + b), kNanos + kMicros);
}

struct MoveOnlyInt {
  explicit MoveOnlyInt(int i) : value(i) {}

  MoveOnlyInt(const MoveOnlyInt&) = delete;
  MoveOnlyInt& operator=(const MoveOnlyInt&) = delete;
  MoveOnlyInt(MoveOnlyInt&&) = default;

  [[nodiscard]] friend MoveOnlyInt operator+(const MoveOnlyInt& a, const MoveOnlyInt& b) {
    return MoveOnlyInt(a.value + b.value);
  }

  [[nodiscard]] friend MoveOnlyInt operator-(const MoveOnlyInt& a, const MoveOnlyInt& b) {
    return MoveOnlyInt(a.value - b.value);
  }

  [[nodiscard]] friend MoveOnlyInt operator*(const MoveOnlyInt& a, std::unique_ptr<int> times) {
    return MoveOnlyInt(a.value * (*times));
  }

  int value;
};

TEST(Typedef, WrapperWithPlusHasPlusForMoveOnlyType) {
  WrapperWithPlus<MoveOnlyInt> a_wrapped(std::in_place, kAValue);
  WrapperWithPlus<MoveOnlyInt> b_wrapped(std::in_place, kBValue);

  EXPECT_EQ((std::move(a_wrapped) + std::move(b_wrapped))->value, kAValue + kBValue);
}

struct DistanceTag {};
struct CoordinateTag : MinusTag<DistanceTag>, PlusTag<DistanceTag> {};

template <typename T>
using Distance = Typedef<DistanceTag, T>;

template <typename T>
using Coordinate = Typedef<CoordinateTag, T>;

TEST(Typedef, CoordinateHasMinusForMoveOnlyType) {
  Coordinate<MoveOnlyInt> a(std::in_place, kAValue);
  Coordinate<MoveOnlyInt> b(std::in_place, kBValue);

  Distance<MoveOnlyInt> distance = std::move(a) - std::move(b);

  EXPECT_EQ(distance->value, kAValue - kBValue);
}

TEST(Typedef, CoordinateHasPlusForMoveOnlyType) {
  {
    Coordinate<MoveOnlyInt> origin(std::in_place, kAValue);
    Distance<MoveOnlyInt> distance(std::in_place, kBValue);
    Coordinate<MoveOnlyInt> coordinate = std::move(origin) + std::move(distance);
    EXPECT_EQ(coordinate->value, kAValue + kBValue);
  }
  {
    Coordinate<MoveOnlyInt> origin(std::in_place, kAValue);
    Distance<MoveOnlyInt> distance(std::in_place, kBValue);
    Coordinate<MoveOnlyInt> coordinate = std::move(distance) + std::move(origin);
    EXPECT_EQ(coordinate->value, kAValue + kBValue);
  }
}

template <typename Scalar>
struct WrapperWithTimesScalarTag : TimesScalarTag<Scalar> {};

template <typename T, typename Scalar>
using WrapperWithTimesScalar = Typedef<WrapperWithTimesScalarTag<Scalar>, T>;

TEST(Typedef, WrapperWithTimesScalarIntTimesFloat) {
  WrapperWithTimesScalar<int, double> wrapped(2);
  {
    WrapperWithTimesScalar<double, double> half_of_wrapped = wrapped * 0.5;
    EXPECT_EQ(*half_of_wrapped, 2 * 0.5);
  }

  {
    WrapperWithTimesScalar<double, double> half_of_wrapped = 0.5 * wrapped;
    EXPECT_EQ(*half_of_wrapped, 2 * 0.5);
  }
}

TEST(Typedef, WrapperWithTimesScalarMoveOnly) {
  {
    auto times = std::make_unique<int>(kBValue);
    WrapperWithTimesScalar<MoveOnlyInt, std::unique_ptr<int>> wrapped(std::in_place, kAValue);
    WrapperWithTimesScalar<MoveOnlyInt, std::unique_ptr<int>> result =
        std::move(wrapped) * std::move(times);
    EXPECT_EQ(result->value, kAValue * kBValue);
  }

  {
    auto times = std::make_unique<int>(kBValue);
    WrapperWithTimesScalar<MoveOnlyInt, std::unique_ptr<int>> wrapped(std::in_place, kAValue);
    WrapperWithTimesScalar<MoveOnlyInt, std::unique_ptr<int>> result =
        std::move(times) * std::move(wrapped);
    EXPECT_EQ(result->value, kAValue * kBValue);
  }
}

}  // namespace orbit_base