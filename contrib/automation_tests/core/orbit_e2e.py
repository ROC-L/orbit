"""
Copyright (c) 2020 The Orbit Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
"""

import logging
from typing import Iterable, Type

from absl import flags
from pywinauto import Application, timings
from pywinauto.base_wrapper import BaseWrapper

import orbit_testing as ot

flags.DEFINE_boolean('dev_mode', False, 'Dev mode - expect Orbit MainWindow to be opened, '
                                        'and do not close Orbit at the end')


class OrbitE2EError(RuntimeError):
    pass


class Fragment:
    def __init__(self):
        self._e2e_test = None

    e2e_test = property(lambda self: self._e2e_test)

    def execute(self, e2e_test):
        self._e2e_test = e2e_test
        self._execute()

    def expect_true(self, cond, description):
        if not cond:
            raise OrbitE2EError('Error executing test case %s, fragment %s. Condition did not hold: "%s"' %
                                (self.e2e_test.name, self.__class__.__name__, description))

    def expect_eq(self, left, right, description):
        self.expect_true(left == right, description)

    def _execute(self):
        """
        Override this in your fragment
        """
        pass


class E2ETest:
    def __init__(self, test_name: str, fragments: Iterable[Fragment], dev_mode: bool = False,
                 auto_connect: bool = True):
        try:
            self._application = Application(backend='uia').connect(title_re='orbitprofiler')
        except Exception:
            logging.error("Could not find Orbit application. Make sure to start Orbit before running E2E tests.")
            raise
        self._test_name = test_name
        self._dev_mode = dev_mode or flags.FLAGS.dev_mode
        self._fragments = fragments[:]
        self._auto_connect = auto_connect
        self._top_window = None

    name = property(lambda self: self._test_name)
    application = property(lambda self: self._application)
    dev_mode = property(lambda self: self._dev_mode)

    def top_window(self, force_update=False):
        if force_update:
            self._top_window = self._application.top_window()
        return self._top_window

    def set_up(self):
        timings.Timings.after_click_wait = 0.5
        ot.wait_for_orbit()
        logging.info("Setting up with dev_mode = %s", self.dev_mode)
        if not self.dev_mode:
            if self._auto_connect:
                ot.connect_to_gamelet(self.application)
        else:
            logging.info("DEV MODE: Skipped gamelet connection, assuming Main Window is active")
        self.top_window(True).set_focus()

    def tear_down(self):
        if not self._dev_mode:
            find_control(self.top_window(), "Button", "Close").click_input()
            logging.info('Closed Orbit.')
        else:
            logging.info("DEV MODE: Skipped closing Orbit")
        logging.info('Test "%s" executed without errors', self._test_name)
        timings.Timings.defaults()

    def execute(self):
        self.set_up()
        for fragment in self._fragments:
            logging.info('Executing fragment "%s"', fragment.__class__.__name__)
            fragment.execute(self)
        self.tear_down()


def find_control(parent: BaseWrapper, control_type, name=None, name_contains=None,
                 auto_id_leaf=None, qt_class=None) -> BaseWrapper:
    """
    Returns the first child of BaseWrapper that matches all of the search parameters.
    As soon as a matching child is encountered, this function returns.

    If no child is found, an exception is thrown.
    """
    desc = parent.descendants(control_type=control_type)

    for elem in desc:
        if (not qt_class or elem.class_name() == qt_class) and \
                (not name or elem.texts()[0] == name) and \
                (not name_contains or name_contains in elem.texts()[0]) and \
                (not auto_id_leaf or elem.automation_id().rsplit(".", 1)[-1]):
            return elem

    raise OrbitE2EError('Could not find element of type %s (name="%s", name_contains="%s", qt_class=%s)' %
                        (control_type, name, name_contains, qt_class))