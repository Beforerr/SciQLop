from glob import glob
import os
from typing import List
from PySide6.QtCore import Signal
from PySide6.QtWidgets import QFrame, QVBoxLayout, QLabel, QSizePolicy, QWidget, QGridLayout, QBoxLayout, QSpacerItem
from ..common import HLine, apply_size_policy, increase_font_size
from ..common.flow_layout import FlowLayout
from .card import Card


class CardsCollection(QFrame):
    _cards: List[Card]
    show_detailed_description = Signal(QWidget)
    _last_row: int = 0
    _last_col: int = 0
    _columns: int = 6

    def __init__(self, parent=None):
        super().__init__(parent)
        self._cards = []
        self._layout = QGridLayout()  # , hspacing=10, vspacing=10)
        self._layout.setContentsMargins(10, 10, 10, 10)
        self.setLayout(self._layout)
        self.refresh_ui()

    def _place_card(self, card: Card):
        self._layout.addWidget(card, self._last_row, self._last_col)
        self._last_col += 1
        if self._last_col == self._columns:
            self._last_col = 0
            self._last_row += 1

    def _clear_layout(self):
        self._last_row = 0
        self._last_col = 0
        count = self._layout.count()
        for i in range(count):
            self._layout.takeAt(0)
        self._layout.addItem(QSpacerItem(0, 0, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Maximum), 0,
                             self._columns, -1, 1)

    def add_card(self, card: Card):
        self._cards.append(card)
        self._place_card(card)
        card.clicked.connect(lambda: self.show_detailed_description.emit(card))

    def refresh_ui(self):
        self._clear_layout()
        for card in self._cards:
            self._place_card(card)

    def clear(self):
        self._cards.clear()
        self.refresh_ui()

    def mousePressEvent(self, event):
        if not self.childAt(event.position().toPoint()):
            list(map(lambda c: c.set_selected(False), self._cards))
            self.show_detailed_description.emit(None)
        super().mousePressEvent(event)


class WelcomeSection(QFrame):
    show_detailed_description = Signal(QWidget)

    def __init__(self, name: str, parent=None):
        super().__init__(parent)
        self.setStyleSheet(f".{self.__class__.__name__}{{border: 1px solid black; border-radius: 2px;}}")
        self._layout = QVBoxLayout()
        self.setLayout(self._layout)
        self._layout.addWidget(apply_size_policy(increase_font_size(QLabel(name), 1.2, ), QSizePolicy.Policy.Expanding,
                                                 QSizePolicy.Policy.Maximum))
        self._layout.addWidget(apply_size_policy(HLine(), QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Maximum))
