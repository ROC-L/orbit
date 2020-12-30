"""
Copyright (c) 2020 The Orbit Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
"""
"""Add a function iterator in Orbit using pywinauto.

Before this script is run there needs to be a gamelet reserved and
"hello_ggp_standalone" has to be started.

The script requires absl and pywinauto. Since pywinauto requires the bitness of
the python installation to match the bitness of the program under test it needs
to by run from 64 bit python.

This automation script covers a basic workflow:
 - start Orbit
 - connect to a gamelet
 - select a process and load debug symbols
 - instrument a function
 - take a capture
 - in the "Live" tab, add an iterator for the instrumented function
"""

from absl import app

from core.orbit_e2e import E2ETestSuite
from fragments.connection_window import FilterAndSelectFirstProcess, ConnectToStadiaInstance
from fragments.symbols_tab import LoadSymbols, FilterAndHookFunction
from fragments.capture_window import Capture
from fragments.live_tab import AddIterator


def main(argv):
    tests = [
        ConnectToStadiaInstance(),
        FilterAndSelectFirstProcess(process_filter="hello_ggp"),
        LoadSymbols(module_search_string="hello_ggp"),
        FilterAndHookFunction(function_search_string='DrawFrame'),
        Capture(),
        AddIterator(function_name="DrawFrame")
    ]
    suite = E2ETestSuite(test_name="Add Iterator", tests=tests)
    suite.execute()


if __name__ == '__main__':
    app.run(main)
