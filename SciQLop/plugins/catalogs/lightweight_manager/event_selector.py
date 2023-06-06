from typing import List, Mapping

from PySide6.QtCore import Signal, QItemSelection
from PySide6.QtGui import Qt, QStandardItem, QStandardItemModel
from PySide6.QtWidgets import QComboBox, QListView

from SciQLop.backend import TimeRange
from .event import Event


class EventItem(QStandardItem):
    def __int__(self, *args, **kwargs):
        QStandardItem.__init__(self, *args, **kwargs)

    def set_range(self, new_range: TimeRange):
        self.setText(f"{new_range.datetime_start} -> {new_range.datetime_stop} ")


class EventSelector(QListView):
    event_selected = Signal(object)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.model = QStandardItemModel()
        self.setModel(self.model)
        self._events: Mapping[str, Event] = {}
        self.selectionModel().selectionChanged.connect(self._event_selected)

    def update_list(self, events: List[Event]):
        self._events = {}
        self.model.clear()
        for index, e in enumerate(sorted(events, key=lambda ev: ev.start + (ev.stop - ev.start))):
            item = EventItem()
            item.setData(e.uuid, Qt.UserRole)
            item.set_range(e.range)
            e.range_changed.connect(item.set_range)
            self.model.setItem(index, item)
            self._events[e.uuid] = e

    def _event_selected(self, selected: QItemSelection, deselected: QItemSelection):
        indexes = selected.indexes()
        if len(indexes) and len(self._events):
            item = self.model.itemFromIndex(indexes[0])
            self.event_selected.emit(self._events[item.data(Qt.UserRole)])

    @property
    def events(self):
        return self._events.values()


class PanelSelector(QComboBox):
    def __init__(self, parent=None):
        super(PanelSelector, self).__init__(parent)
        self.addItems(["None"])
        self.setSizeAdjustPolicy(QComboBox.SizeAdjustPolicy.AdjustToContents)

    def update_list(self, panels):
        selected = self.currentText()
        self.clear()
        self.addItems(["None"] + panels)
        self.setCurrentText(selected)
