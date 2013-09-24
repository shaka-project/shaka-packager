#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from code import Code
import unittest

class CodeTest(unittest.TestCase):
  def testAppend(self):
    c = Code()
    c.Append('line')
    self.assertEquals('line', c.Render())

  def testBlock(self):
    c = Code()
    (c.Append('line')
      .Sblock('sblock')
        .Append('inner')
        .Append('moreinner')
        .Sblock('moresblock')
          .Append('inner')
        .Eblock('out')
        .Append('inner')
      .Eblock('out')
    )
    self.assertEquals(
      'line\n'
      'sblock\n'
      '  inner\n'
      '  moreinner\n'
      '  moresblock\n'
      '    inner\n'
      '  out\n'
      '  inner\n'
      'out',
      c.Render())

  def testConcat(self):
    b = Code()
    (b.Sblock('2')
        .Append('2')
      .Eblock('2')
    )
    c = Code()
    (c.Sblock('1')
        .Concat(b)
        .Append('1')
      .Eblock('1')
    )
    self.assertEquals(
      '1\n'
      '  2\n'
      '    2\n'
      '  2\n'
      '  1\n'
      '1',
      c.Render())
    d = Code()
    a = Code()
    a.Concat(d)
    self.assertEquals('', a.Render())
    a.Concat(c)
    self.assertEquals(
      '1\n'
      '  2\n'
      '    2\n'
      '  2\n'
      '  1\n'
      '1',
      a.Render())

  def testConcatErrors(self):
    c = Code()
    d = Code()
    d.Append('%s')
    self.assertRaises(TypeError, c.Concat, d)
    d = Code()
    d.Append('%(classname)s')
    self.assertRaises(TypeError, c.Concat, d)
    d = 'line of code'
    self.assertRaises(TypeError, c.Concat, d)

  def testSubstitute(self):
    c = Code()
    c.Append('%(var1)s %(var2)s %(var1)s')
    c.Substitute({'var1': 'one', 'var2': 'two'})
    self.assertEquals('one two one', c.Render())
    c.Append('%(var1)s %(var2)s %(var3)s')
    c.Append('%(var2)s %(var1)s %(var3)s')
    c.Substitute({'var1': 'one', 'var2': 'two', 'var3': 'three'})
    self.assertEquals(
        'one two one\n'
        'one two three\n'
        'two one three',
        c.Render())

  def testSubstituteErrors(self):
    # No unnamed placeholders allowed when substitute is run
    c = Code()
    c.Append('%s %s')
    self.assertRaises(TypeError, c.Substitute, ('var1', 'one'))
    c = Code()
    c.Append('%s %(var1)s')
    self.assertRaises(TypeError, c.Substitute, {'var1': 'one'})
    c = Code()
    c.Append('%s %(var1)s')
    self.assertRaises(TypeError, c.Substitute, {'var1': 'one'})
    c = Code()
    c.Append('%(var1)s')
    self.assertRaises(KeyError, c.Substitute, {'clearlynotvar1': 'one'})

  def testIsEmpty(self):
    c = Code()
    self.assertTrue(c.IsEmpty())
    c.Append('asdf')
    self.assertFalse(c.IsEmpty())

  def testComment(self):
    long_comment = ('This comment is eighty nine characters in longness, '
        'that is, to use another word, length')
    c = Code()
    c.Comment(long_comment)
    self.assertEquals(
        '// This comment is eighty nine characters '
        'in longness, that is, to use another\n'
        '// word, length',
        c.Render())
    c = Code()
    c.Sblock('sblock')
    c.Comment(long_comment)
    c.Eblock('eblock')
    c.Comment(long_comment)
    self.assertEquals(
        'sblock\n'
        '  // This comment is eighty nine characters '
        'in longness, that is, to use\n'
        '  // another word, length\n'
        'eblock\n'
        '// This comment is eighty nine characters in '
        'longness, that is, to use another\n'
        '// word, length',
        c.Render())
    long_word = 'x' * 100
    c = Code()
    c.Comment(long_word)
    self.assertEquals(
        '// ' + 'x' * 77 + '\n'
        '// ' + 'x' * 23,
        c.Render())

  def testCommentWithSpecialCharacters(self):
    c = Code()
    c.Comment('20% of 80%s')
    c.Substitute({})
    self.assertEquals('// 20% of 80%s', c.Render())
    d = Code()
    d.Append('90')
    d.Concat(c)
    self.assertEquals('90\n'
        '// 20% of 80%s',
        d.Render())

if __name__ == '__main__':
  unittest.main()
