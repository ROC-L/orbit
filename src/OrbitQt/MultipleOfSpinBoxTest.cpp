// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmock/gmock-matchers.h>
#include <gtest/gtest.h>

#include "MultipleOfSpinBox.h"

namespace orbit_qt {
TEST(MultipleOfSpinBox, Validate) {
  MultipleOfSpinBox spin_box{nullptr};
  spin_box.setSingleStep(8);
  spin_box.setMinimum(16);
  spin_box.setMaximum(512);

  int pos = 0;

  QString sixteen = QString::fromStdString("16");
  EXPECT_EQ(spin_box.validate(sixteen, pos), QValidator::State::Acceptable);

  QString twenty_four = QString::fromStdString("24");
  EXPECT_EQ(spin_box.validate(twenty_four, pos), QValidator::State::Acceptable);

  QString thirty_two = QString::fromStdString("32");
  EXPECT_EQ(spin_box.validate(thirty_two, pos), QValidator::State::Acceptable);

  QString one_hundred_twenty_eight = QString::fromStdString("128");
  EXPECT_EQ(spin_box.validate(one_hundred_twenty_eight, pos), QValidator::State::Acceptable);

  QString five_hundred_twelve = QString::fromStdString("512");
  EXPECT_EQ(spin_box.validate(five_hundred_twelve, pos), QValidator::State::Acceptable);

  // Values during editing that are below the maximum, may still become valid
  QString zero = QString::fromStdString("0");
  EXPECT_EQ(spin_box.validate(zero, pos), QValidator::State::Intermediate);

  QString one = QString::fromStdString("1");
  EXPECT_EQ(spin_box.validate(one, pos), QValidator::State::Intermediate);

  QString two = QString::fromStdString("2");
  EXPECT_EQ(spin_box.validate(two, pos), QValidator::State::Intermediate);

  QString four = QString::fromStdString("4");
  EXPECT_EQ(spin_box.validate(four, pos), QValidator::State::Intermediate);

  QString five = QString::fromStdString("5");
  EXPECT_EQ(spin_box.validate(five, pos), QValidator::State::Intermediate);

  QString eight = QString::fromStdString("8");
  EXPECT_EQ(spin_box.validate(eight, pos), QValidator::State::Intermediate);

  QString twenty_three = QString::fromStdString("23");
  EXPECT_EQ(spin_box.validate(twenty_three, pos), QValidator::State::Intermediate);

  // Values that are already larger than the maximum, may not become valid anymore
  QString five_hundred_third_teen = QString::fromStdString("513");
  EXPECT_EQ(spin_box.validate(five_hundred_third_teen, pos), QValidator::State::Invalid);

  QString six_hundred = QString::fromStdString("600");
  EXPECT_EQ(spin_box.validate(six_hundred, pos), QValidator::State::Invalid);

  QString thousand = QString::fromStdString("1000");
  EXPECT_EQ(spin_box.validate(thousand, pos), QValidator::State::Invalid);

  QString thousand_twenty_four = QString::fromStdString("1024");
  EXPECT_EQ(spin_box.validate(thousand_twenty_four, pos), QValidator::State::Invalid);

  // Non-numbers are invalid
  QString chars = QString::fromStdString("abc");
  EXPECT_EQ(spin_box.validate(thousand_twenty_four, pos), QValidator::State::Invalid);

  QString numbers_and_chars = QString::fromStdString("8a");
  EXPECT_EQ(spin_box.validate(thousand_twenty_four, pos), QValidator::State::Invalid);

  QString special_characters = QString::fromStdString("1*asd");
  EXPECT_EQ(spin_box.validate(thousand_twenty_four, pos), QValidator::State::Invalid);

  QString floating_points = QString::fromStdString("16.0");
  EXPECT_EQ(spin_box.validate(thousand_twenty_four, pos), QValidator::State::Invalid);
}
}  // namespace orbit_qt
