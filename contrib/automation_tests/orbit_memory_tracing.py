"""
Copyright (c) 2021 The Orbit Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
"""

from absl import app

from core.orbit_e2e import E2ETestSuite
from test_cases.capture_window import Capture, VerifyTracksExist, SetAndCheckMemorySamplingPeriod, \
    VerifyTracksDoNotExist
from test_cases.connection_window import FilterAndSelectFirstProcess, ConnectToStadiaInstance

"""Smoke test for the memory tracing and visualization using pywinauto.

This automation script covers a basic workflow:
 - start Orbit, connect to a gamelet, and then select a process
 - enable memory tracing, set and validate the memory sampling period
 - enable memory tracing, take a capture and verify the presence of memory tracks
 - disable memory tracing, take a capture and verify that the memory tracks are gone
"""


def main(argv):
    test_cases = [
        ConnectToStadiaInstance(),
        FilterAndSelectFirstProcess(process_filter='hello_ggp'),
        SetAndCheckMemorySamplingPeriod(memory_sampling_period='030'),
        SetAndCheckMemorySamplingPeriod(memory_sampling_period=''),
        SetAndCheckMemorySamplingPeriod(memory_sampling_period='ab'),
        SetAndCheckMemorySamplingPeriod(memory_sampling_period='0'),
        Capture(collect_system_memory_usage=True),
        VerifyTracksExist(track_names="*Memory*"),
        Capture(),
        VerifyTracksDoNotExist(track_names="*Memory*")
    ]
    suite = E2ETestSuite(test_name="Collect System Memory Usage", test_cases=test_cases)
    suite.execute()


if __name__ == '__main__':
    app.run(main)
