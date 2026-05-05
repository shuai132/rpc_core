from __future__ import annotations

from typing import Protocol


class _Cancelable(Protocol):
    def cancel(self) -> object:
        ...


class Dispose:
    def __init__(self) -> None:
        self._items: set[_Cancelable] = set()

    def add(self, item: _Cancelable) -> None:
        self._items.add(item)

    def remove(self, item: _Cancelable) -> None:
        self._items.discard(item)

    def dispose(self) -> None:
        items = list(self._items)
        self._items.clear()
        for item in items:
            item.cancel()

    def __enter__(self) -> "Dispose":
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        self.dispose()
