import tempfile
import ctypes
import threading
import unittest
from pathlib import Path
from unittest import mock

import myelin
from myelin import Buffer, Runtime, Session
from myelin.runtime import _Api, _Diagnostic


class RuntimeTest(unittest.TestCase):
    def test_package_exposes_version(self) -> None:
        self.assertRegex(myelin.__version__, r"^(?:\d+\.\d+\.\d+|0\+unknown)$")

    def test_runtime_library_exports_symbols_to_native_providers(self) -> None:
        loader = ctypes.CDLL
        with mock.patch("myelin.runtime.ctypes.CDLL", wraps=loader) as load:
            _Api()
        self.assertEqual(
            load.call_args.kwargs["mode"],
            getattr(ctypes, "RTLD_GLOBAL", ctypes.DEFAULT_MODE),
        )

    def test_typed_buffer_round_trip(self) -> None:
        buffer = Buffer.from_bytes(b"abcdef", (2, 3))
        self.assertEqual(buffer.shape, (2, 3))
        self.assertEqual(buffer.read(1, 3), b"bcd")
        buffer.write(b"XY", 2)
        self.assertEqual(bytes(buffer), b"abXYef")
        buffer.close()

    def test_lifecycle_and_argument_validation(self) -> None:
        with Runtime() as runtime:
            with runtime.session() as session:
                with self.assertRaisesRegex(ValueError, "must not be empty"):
                    session.load("")
                with self.assertRaisesRegex(ValueError, "backend must not be empty"):
                    session.load("artifact.mln", "")

    def test_closed_runtime_rejects_sessions(self) -> None:
        runtime = Runtime()
        runtime.close()
        with self.assertRaisesRegex(RuntimeError, "runtime is closed"):
            runtime.session()

    def test_direct_session_is_owned_by_runtime(self) -> None:
        runtime = Runtime()
        session = Session(runtime)
        runtime.close()
        with self.assertRaisesRegex(RuntimeError, "session is closed"):
            session.devices()
        with self.assertRaisesRegex(RuntimeError, "runtime is closed"):
            Session(runtime)

    def test_failed_worker_start_does_not_break_close(self) -> None:
        with Runtime() as runtime:
            session = runtime.session()
            with mock.patch.object(threading.Thread, "start", side_effect=RuntimeError("start failed")):
                with self.assertRaisesRegex(RuntimeError, "start failed"):
                    session.invoke(b"")
            session.close()

    def test_runtime_close_cancels_worker_without_holding_runtime_lock(self) -> None:
        runtime = Runtime()
        session = runtime.session()
        entered = threading.Event()
        cancelled = threading.Event()
        errors: list[BaseException] = []
        library = session._api.library
        cancel = library.mln_session_cancel

        def invoke(*_: object) -> int:
            entered.set()
            if not cancelled.wait(5):
                raise AssertionError("close did not cancel the invocation")

            with self.assertRaisesRegex(RuntimeError, "runtime is closed"):
                runtime.devices()
            with self.assertRaisesRegex(RuntimeError, "session is closed"):
                session.load("ignored.mln")
            return 0

        def cancel_worker(handle: object) -> None:
            cancel(handle)
            cancelled.set()

        def consume() -> None:
            try:
                session.invoke(b"")
            except BaseException as error:
                errors.append(error)

        with mock.patch.object(library, "mln_session_invoke", side_effect=invoke), \
                mock.patch.object(library, "mln_session_cancel", side_effect=cancel_worker):
            consumer = threading.Thread(target=consume, daemon=True)
            consumer.start()
            self.assertTrue(entered.wait(5))
            closer = threading.Thread(target=runtime.close, daemon=True)
            closer.start()
            closer.join(5)
            self.assertFalse(closer.is_alive(), "runtime close deadlocked")
            consumer.join(5)
            self.assertFalse(consumer.is_alive(), "invocation did not finish")
        self.assertEqual(errors, [])

    def test_callback_cannot_destroy_its_active_session(self) -> None:
        with Runtime() as runtime:
            with runtime.session() as session:
                def invoke(*_: object) -> int:
                    with self.assertRaisesRegex(RuntimeError, "invocation callback"):
                        session.close()
                    with self.assertRaisesRegex(RuntimeError, "invocation callback"):
                        runtime.close()
                    return 0

                with mock.patch.object(session._api.library, "mln_session_invoke", side_effect=invoke):
                    self.assertEqual(session.invoke(b""), b"")

    def test_diagnostic_callback_cannot_destroy_its_session(self) -> None:
        with Runtime() as runtime:
            with runtime.session() as session:
                def handler(_: object) -> str:
                    with self.assertRaisesRegex(RuntimeError, "invocation callback"):
                        session.close()
                    with self.assertRaisesRegex(RuntimeError, "invocation callback"):
                        runtime.close()
                    return "continue"

                session.set_diagnostic_handler(handler)
                diagnostic = _Diagnostic(b"test", 1, b"warning", b"", 0)
                self.assertEqual(session._report_diagnostic(ctypes.pointer(diagnostic), None), 0)

    def test_runtime_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "myelin.toml"
            path.write_text(
                '[runtime]\nmemory = "1GiB"\nscheduler = "adaptive"\n'
                '[backend.local]\ndevice = "local"\nordinal = 0\nthreads = 2\n',
                encoding="utf-8",
            )
            with Runtime(path):
                pass

    def test_devices_are_reported_by_iree(self) -> None:
        with Runtime() as runtime:
            devices = runtime.devices()
            self.assertTrue(any(device.driver == "local-task" for device in devices))

    def test_backend_owns_its_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "myelin.toml"
            path.write_text(
                '[backend.cuda]\ndevice = "cuda"\nordinal = 0\nthreads = 2\n',
                encoding="utf-8",
            )
            with self.assertRaisesRegex(RuntimeError, "unknown runtime configuration key"):
                Runtime(path)

    def test_configuration_rejects_wrong_types_and_overflow(self) -> None:
        invalid = (
            '[runtime]\nmemory = 1073741824\n',
            '[runtime]\nscheduler = 123\n',
            '[backend.local]\nordinal = 4294967296\n',
            '[backend.local]\nthreads = "many"\n',
            'runtime = 7\n',
            '[runtime]\nvariants = 0\n',
            '[runtime]\nprofile = "capture"\n',
            '[runtime.profile]\noutput = 7\n',
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "myelin.toml"
            for document in invalid:
                with self.subTest(document=document):
                    path.write_text(document, encoding="utf-8")
                    with self.assertRaises(RuntimeError):
                        Runtime(path)

    def test_external_backend_preserves_its_configuration(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "myelin.toml"
            path.write_text(
                '[backend.accelerator]\ndevice = "accelerator"\nordinal = 1\n'
                'queue = "realtime"\npriority = 4\n',
                encoding="utf-8",
            )
            with Runtime(path):
                pass


if __name__ == "__main__":
    unittest.main()
