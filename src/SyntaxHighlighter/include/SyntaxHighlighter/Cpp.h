// Copyright (c) 2021 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNTAX_HIGHLIGHTER_CPP_H_
#define SYNTAX_HIGHLIGHTER_CPP_H_

#include <QRegularExpression>
#include <QSyntaxHighlighter>

namespace orbit_syntax_highlighter {

//  This a syntax highlighter for C++.
//  It derives from QSyntaxHighlighter, so check out QSyntaxHighlighter's
//  documentation on how to use it. There are no additional settings or
//  APIs.

class Cpp : public QSyntaxHighlighter {
  Q_OBJECT
  void highlightBlock(const QString& code) override;

 public:
  explicit Cpp();

 private:
  QRegularExpression comment_regex_;
  QRegularExpression open_comment_regex_;
  QRegularExpression end_comment_regex_;
  QRegularExpression no_end_comment_regex_;
  QRegularExpression function_regex_;
  QRegularExpression number_regex_;
  QRegularExpression constant_regex_;
  QRegularExpression keyword_regex_;
  QRegularExpression preprocessor_regex_;
  QRegularExpression include_file_regex_;
  QRegularExpression string_regex_;
  QRegularExpression open_string_regex_;
  QRegularExpression end_string_regex_;
  QRegularExpression no_end_string_regex_;
  QRegularExpression no_lowercase_regex_;
  QRegularExpression capitalized_regex_;
};

}  // namespace orbit_syntax_highlighter

#endif  // SYNTAX_HIGHLIGHTER_CPP_H_
