#!/usr/bin/env python3
"""Script to combine multiple hex files into one."""
import argparse
import re
from pathlib import Path

# REGEX to split a line in hex file.
PATTERN = re.compile(r"^:([A-F0-9]{2})([A-F0-9]{4})(\d{2})([A-F0-9]*)([A-F0-9]{2})$")


def hex_file_combine(target: str, *sources: str) -> None:
    """Combine numerous hex files into one hex file.

    Args:
        target: The path to write result to
        *sources: Files to combine

    Returns:
        None
    """
    # We are only interested in data and offset commands.
    #  https://en.wikipedia.org/wiki/Intel_HEX
    record_types = {"00", "02", "04"}
    with Path(target).open("w+") as target_file:
        for source in sources:
            with Path(source).open("r") as source_file:
                for line in source_file.readlines():
                    hex_record = PATTERN.match(line)
                    if not hex_record:
                        raise RuntimeError(
                            f"'{line}' is not a valid hex file formatted line."
                        )
                    if hex_record.groups()[2] in record_types:
                        target_file.write(line)

        # Write the EOF line
        target_file.write(":00000001FF\n")


def main() -> None:
    """Entry point."""
    parser = argparse.ArgumentParser(description="Combine multiple hex files into one.")
    parser.add_argument(
        "target",
        metavar="TARGET",
        type=str,
        nargs="?",
        help="path of hex file to generate; use - or do not specify for stdout",
    )
    parser.add_argument(
        "sources", metavar="N", type=str, nargs="+", help="hex files to combine"
    )

    args = parser.parse_args()

    hex_file_combine(args.target, *args.sources)


if __name__ == "__main__":
    main()
