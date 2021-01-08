"""
Copyright (c) 2020 The Orbit Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
"""

from pywinauto.base_wrapper import BaseWrapper
from pywinauto.controls.uiawrapper import UIAWrapper, UIAElementInfo

from core.orbit_e2e import find_control


class Track:
    def __init__(self, control: BaseWrapper):
        self._container = control
        self._title = find_control(control, 'TabItem')
        self._content = find_control(control, 'Group')

    container = property(lambda self: self._container)
    title = property(lambda self: self._title)
    content = property(lambda self: self._content)


class DataViewPanel:
    def __init__(self, control: BaseWrapper):
        self._panel = control
        self._table = find_control(control, "Tree", "DataView")
        self._refresh_button = find_control(control, "Button", "Refresh", raise_on_failure=False)
        self._filter = find_control(control,  "Edit", "Filter")

    panel = property(lambda self: self._panel)
    table = property(lambda self: self._table)
    refresh_button = property(lambda self: self._refresh_button)
    filter = property(lambda self: self._filter)

    def get_row_count(self):
        return self.table.iface_grid.CurrentRowCount

    def get_column_count(self):
        return self.table.iface_grid.CurrentColumnCount

    def get_item_at(self, row: int, col: int) -> BaseWrapper:
        """
        This uses the low-level API to return the element at row x col.
        pywinauto does not seem to provide row/col access for TreeView, even though the element supports the grid pattern.
        Using get_item does not return items that are currently hidden. Unfortunately this also means that columns
        hidden due to scrolling are NOT handled correctly, and get_item(col + row * col_count) does not always yield
        the correct result...

        We really should not need to use the low-level API like this.
        """
        element_info = UIAElementInfo(self.table.iface_grid.GetItem(row, col))
        wrapper = UIAWrapper(element_info)
        return wrapper

    def find_first_item_row(self, text: str, column: int, partial_match=False) -> int:
        row_count = self.get_row_count()
        for i in range(row_count):
            item = self.get_item_at(i, column)
            item_text = item.texts()[0]
            if item_text == text or partial_match and text in item_text:
                return i

        return None
