import json
import struct
import unittest
from copy import deepcopy
from pathlib import Path

import app


ROOT = Path(__file__).resolve().parent


def fc03_response(data: bytes, address: int = 1) -> bytes:
    return app.append_crc(bytes([address, 0x03, len(data)]) + data)


class Tuf2000mProfileTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        profiles = json.loads(
            (ROOT / "device_profiles.json").read_text(encoding="utf-8")
        )["profiles"]
        cls.profile = next(
            profile
            for profile in profiles
            if profile["id"] == "tuf2000m_ts2_flow_meter"
        )

    def test_version_and_serial_settings(self) -> None:
        self.assertEqual(app.APP_VERSION, "0.9.0")
        self.assertEqual(self.profile["baud"], 9600)
        self.assertEqual(self.profile["serial_config"], "8N1")

    def test_complete_request_frames_match_manual_derived_crc(self) -> None:
        expected = [
            "01 03 00 00 00 06 C5 C8",
            "01 03 00 47 00 01 34 1F",
            "01 03 00 5B 00 03 74 18",
            "01 03 00 60 00 02 C4 15",
            "01 03 00 DC 00 02 05 F1",
            "01 03 05 A1 00 01 D5 24",
        ]
        actual = []
        for block in self.profile["read"]["blocks"]:
            request = app.replace_template_tokens(block["request_no_crc"], 1)
            actual.append(app.bytes_to_hex(app.append_crc(request)))
        self.assertEqual(actual, expected)

    def test_high_word_first_measurement_candidates(self) -> None:
        data = (
            struct.pack(">f", 12.5)
            + struct.pack(">f", 0.0)
            + struct.pack(">f", 1.25)
        )
        frame = fc03_response(data)
        block = self.profile["read"]["blocks"][0]
        parsed = {value.name: value for value in app.ProfileParser.parse_spec(block, frame)}
        self.assertEqual(parsed["flow_rate_high_word_first"].value, 12.5)
        self.assertEqual(parsed["velocity_high_word_first"].value, 1.25)

    def test_low_word_first_candidate_reorders_registers(self) -> None:
        ieee = struct.pack(">f", 57.0)
        low_word_first = ieee[2:4] + ieee[0:2]
        frame = fc03_response(low_word_first)
        block = self.profile["read"]["blocks"][4]
        parsed = {value.name: value for value in app.ProfileParser.parse_spec(block, frame)}
        self.assertEqual(parsed["inner_diameter_low_word_first"].value, 57.0)
        self.assertEqual(parsed["inner_diameter_low_word_first"].status, "OK")
        self.assertEqual(parsed["inner_diameter_high_word_first"].status, "RANGE WARNING")

    def test_tuf_error_bits_are_decoded(self) -> None:
        frame = fc03_response(bytes([0x00, 0x09]))
        block = self.profile["read"]["blocks"][1]
        parsed = app.ProfileParser.parse_spec(block, frame)[0]
        self.assertIn("no received signal", parsed.value)
        self.assertIn("empty pipe", parsed.value)
        self.assertEqual(parsed.status, "DEVICE WARNING/ERROR")

    def test_signal_quality_and_raw_strengths(self) -> None:
        data = bytes([0x02, 75, 0x02, 0xEE, 0x02, 0xD0])
        frame = fc03_response(data)
        block = self.profile["read"]["blocks"][2]
        parsed = {value.name: value for value in app.ProfileParser.parse_spec(block, frame)}
        self.assertEqual(parsed["working_step"].value, 2.0)
        self.assertEqual(parsed["signal_quality"].value, 75.0)
        self.assertEqual(parsed["upstream_signal_strength_raw"].value, 750.0)
        self.assertEqual(parsed["downstream_signal_strength_raw"].value, 720.0)

    def test_multi_block_read_keeps_every_tx_rx_and_parsed_value(self) -> None:
        responses = [
            fc03_response(
                struct.pack(">f", 12.5)
                + struct.pack(">f", 0.0)
                + struct.pack(">f", 1.25)
            ),
            fc03_response(bytes([0x00, 0x00])),
            fc03_response(bytes([0x02, 75, 0x02, 0xEE, 0x02, 0xD0])),
            fc03_response(struct.pack(">f", 100.0)),
            fc03_response(struct.pack(">f", 57.0)),
            fc03_response(bytes([0x00, 0x01])),
        ]

        class FakeTransport:
            def __init__(self) -> None:
                self.requests = []

            def transaction(self, **kwargs):
                self.requests.append(kwargs["request"])
                frame = responses[len(self.requests) - 1]
                return {
                    "ok": True,
                    "rx": frame,
                    "frame": frame,
                    "status": "CRC OK",
                    "elapsed_ms": 1,
                }

        tool = app.UsbRs485SensorTool.__new__(app.UsbRs485SensorTool)
        tool.transport = FakeTransport()
        tool.last_records = []
        tool.read_raw_text = None
        tool.set_operation_progress = lambda *_args: None
        tool.append_text_widget = lambda *_args: None
        tool.call_ui = lambda _func, *_args: None
        profile = deepcopy(self.profile)
        profile["read"]["block_delay_ms"] = 0

        record = tool.read_profile_address(
            profile,
            1,
            port="COM_TEST",
            timeout=100,
            retries=1,
            post_delay=0,
        )

        self.assertTrue(record["crc_ok"])
        self.assertEqual(record["successful_blocks"], 6)
        self.assertEqual(record["total_blocks"], 6)
        self.assertEqual(len(record["transactions"]), 6)
        self.assertEqual(len(record["values"]), 14)
        self.assertEqual(
            app.bytes_to_hex(tool.transport.requests[0]),
            "01 03 00 00 00 06 C5 C8",
        )


if __name__ == "__main__":
    unittest.main()
