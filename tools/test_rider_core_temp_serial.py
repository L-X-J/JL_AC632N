#!/usr/bin/env python3
"""Verify the Rider UART contract and ASCII-only diagnostic sources."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAKEFILE = ROOT / "apps/rider_core_temp/board/bd19/Makefile"
BOARD_CONFIG = ROOT / "apps/rider_core_temp/board/bd19/board_ac632n_rider_cfg.h"
GLOBAL_BUILD_CONFIG = (
    ROOT
    / "apps/rider_core_temp/board/bd19/board_ac632n_rider_global_build_cfg.h"
)
DEBUG_DOC = ROOT / "apps/rider_core_temp/DEBUG.md"
UART_CLOCK_HZ = 24_000_000
UART_BAUDRATE = 115_200


def _target_sources():
    """Return C sources listed by the target Makefile."""
    sources = []
    for raw_line in MAKEFILE.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip().rstrip("\\").strip()
        if line.startswith("../../../../") and line.endswith(".c"):
            source = (MAKEFILE.parent / line).resolve()
            if source not in sources:
                sources.append(source)
    return sources


def _non_ascii_string_lines_from_bytes(data):
    """Find raw or escaped non-ASCII values inside C string literals."""
    line = 1
    string_line = None
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    escaped = False
    bad_lines = set()
    index = 0

    while index < len(data):
        value = data[index]
        next_value = data[index + 1] if index + 1 < len(data) else None

        if in_line_comment:
            if value == 10:
                in_line_comment = False
                line += 1
            index += 1
            continue

        if in_block_comment:
            if value == 42 and next_value == 47:
                in_block_comment = False
                index += 2
            else:
                if value == 10:
                    line += 1
                index += 1
            continue

        if not in_string and not in_char:
            if value == 47 and next_value == 47:
                in_line_comment = True
                index += 2
                continue
            if value == 47 and next_value == 42:
                in_block_comment = True
                index += 2
                continue
            if value == 34:
                in_string = True
                string_line = line
            elif value == 39:
                in_char = True
            elif value == 10:
                line += 1
            index += 1
            continue

        if in_char:
            if escaped:
                escaped = False
            elif value == 92:
                escaped = True
            elif value == 39:
                in_char = False
            if value == 10:
                line += 1
            index += 1
            continue

        if escaped:
            if value in (120, 117, 85):  # \xNN, \uNNNN, \UNNNNNNNN
                if value == 120:
                    # C consumes every following hexadecimal digit in a \x escape.
                    match = re.match(rb"[0-9A-Fa-f]+", data[index + 1:])
                else:
                    width = {117: 4, 85: 8}[value]
                    match = re.match(rb"[0-9A-Fa-f]{%d}" % width,
                                     data[index + 1:])
                if match:
                    if int(match.group(0), 16) > 0x7F:
                        bad_lines.add(string_line)
                    index += 1 + len(match.group(0))
                    escaped = False
                    continue
            elif 48 <= value <= 55:  # octal escape
                match = re.match(rb"[0-7]{0,2}", data[index + 1:])
                digits = bytes([value]) + (match.group(0) if match else b"")
                if int(digits, 8) > 0x7F:
                    bad_lines.add(string_line)
                index += len(digits)
                escaped = False
                continue
            if value == 10:
                line += 1
            escaped = False
            index += 1
            continue

        if value == 92:
            escaped = True
        elif value == 34:
            in_string = False
            string_line = None
        elif value == 10:
            line += 1
        elif value >= 0x80:
            bad_lines.add(string_line)
        index += 1

    return sorted(bad_lines)


def _non_ascii_string_lines(source):
    """Read a source file and return its non-ASCII C string literal lines."""
    return _non_ascii_string_lines_from_bytes(source.read_bytes())


class RiderSerialContractTests(unittest.TestCase):
    def test_uart_configuration(self):
        """Keep the documented PA0 115200 8N1 debug contract stable."""
        config = BOARD_CONFIG.read_text(encoding="utf-8")
        self.assertRegex(config, r"#define\s+TCFG_UART0_ENABLE\s+ENABLE_THIS_MOUDLE")
        self.assertRegex(config, r"#define\s+TCFG_UART0_TX_PORT\s+IO_PORTA_00")
        self.assertRegex(config, r"#define\s+TCFG_UART0_RX_PORT\s+NO_CONFIG_PORT")
        self.assertRegex(config, r"#define\s+TCFG_UART0_BAUDRATE\s+115200\b")

    def test_boot_debug_matches_application_uart(self):
        """Keep boot/OTA and application diagnostics on PA0 at 115200."""
        config = GLOBAL_BUILD_CONFIG.read_text(encoding="utf-8")
        self.assertRegex(config, r"#define\s+CONFIG_UBOOT_DEBUG_PIN\s+PA00\b")
        self.assertRegex(
            config,
            r"#define\s+CONFIG_UBOOT_DEBUG_BAUD_RATE\s+115200\b",
        )

    def test_uart_divider_is_within_tolerance(self):
        """Check the SDK UART divider math stays within normal UART tolerance."""
        divider = ((UART_CLOCK_HZ + UART_BAUDRATE // 2) // UART_BAUDRATE) // 4 - 1
        actual_baudrate = UART_CLOCK_HZ // (4 * (divider + 1))
        self.assertEqual(divider, 51)
        self.assertEqual(actual_baudrate, 115_384)
        self.assertLess(abs(actual_baudrate - UART_BAUDRATE) / UART_BAUDRATE, 0.02)

    def test_debug_document_matches_uart_contract(self):
        """Keep the standalone wiring guide aligned with the board header."""
        document = DEBUG_DOC.read_text(encoding="utf-8")
        self.assertIn("PA0", document)
        self.assertIn("115200 / 8N1", document)
        self.assertIn("boot/OTA", document)
        self.assertIn("必须重新编译并烧录固件", document)

    def test_non_ascii_literal_scanner_catches_escaped_values(self):
        """Cover C escape syntax so an escaped binary byte cannot bypass the check."""
        self.assertEqual(
            _non_ascii_string_lines_from_bytes(b'const char *s = "\\x80";\n'),
            [1],
        )
        self.assertEqual(
            _non_ascii_string_lines_from_bytes(b'const char *s = "\\200";\n'),
            [1],
        )
        self.assertEqual(
            _non_ascii_string_lines_from_bytes(b'const char *s = "\\u00A0";\n'),
            [1],
        )

    def test_target_string_literals_are_ascii(self):
        """Reject non-ASCII text or escaped bytes in compiled target sources."""
        sources = _target_sources()
        self.assertGreater(len(sources), 0)
        violations = []
        for source in sources:
            self.assertTrue(source.exists(), source)
            for line in _non_ascii_string_lines(source):
                violations.append(f"{source}:{line}")
        self.assertEqual(
            violations,
            [],
            "non-ASCII C string literal(s): " + ", ".join(violations),
        )


if __name__ == "__main__":
    unittest.main()
