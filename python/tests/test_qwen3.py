import struct

from myelin import QwenInput


def test_qwen_text_input_matches_native_serialization_contract() -> None:
    payload = QwenInput.text("hello").encode()
    assert struct.unpack_from("<II", payload) == (0x51333549, 1)
    assert struct.unpack_from("<BI", payload, 8) == (0, 5)
    assert payload[13:] == b"hello"
