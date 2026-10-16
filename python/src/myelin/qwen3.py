from __future__ import annotations

import struct
from dataclasses import dataclass

_INPUT_MAGIC = 0x51333549
_TEXT = 0
_IMAGE = 1


@dataclass(frozen=True)
class QwenText:
    value: str


@dataclass(frozen=True)
class QwenImage:
    encoded: bytes


QwenContent = QwenText | QwenImage


@dataclass(frozen=True)
class QwenInput:
    """Python representation of the public Qwen3 Input artifact contract."""

    content: tuple[QwenContent, ...]

    @classmethod
    def text(cls, value: str) -> QwenInput:
        return cls((QwenText(value),))

    def encode(self) -> bytes:
        payload = bytearray(struct.pack("<II", _INPUT_MAGIC, len(self.content)))
        for item in self.content:
            if isinstance(item, QwenText):
                field = item.value.encode("utf-8")
                kind = _TEXT
            elif isinstance(item, QwenImage):
                field = item.encoded
                kind = _IMAGE
            else:
                raise TypeError(f"unsupported Qwen content: {type(item).__name__}")
            payload.extend(struct.pack("<BI", kind, len(field)))
            payload.extend(field)
        return bytes(payload)


__all__ = ["QwenContent", "QwenImage", "QwenInput", "QwenText"]
