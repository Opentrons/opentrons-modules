#!/usr/bin/env python3
"""Script to convert a binary file to an array c file."""
import argparse
import ntpath
from intelhex import IntelHex


def hex_to_struct(source: str, target: str, var_name: str) -> None:
    variable_name = var_name or ntpath.basename(target).split(".")[0]
    """Generates a c file containing an array of the given binary file."""

    # Use IntelHex
    intel_hex = IntelHex()
    intel_hex.fromfile(source, format="hex")
    # Load the segments.
    start, finish = intel_hex.segments()[0]
    binary_data = intel_hex.tobinarray(start=start, size=(finish - start))  # type: ignore
    binary_data_len = len(binary_data)
    # Write the data to file
    with open(target, "w") as target_file:
        # Write the array definition and variable containing the binary data
        target_file.write(
            f"// Automatically generated C file from binary file {source}\n\n"
        )
        target_file.write(f"const unsigned char {variable_name}[] = {{\n")

        # Write the data as an array of bytes
        for i in range(0, binary_data_len, 12):  # Display 12 bytes per line
            target_file.write("    ")
            target_file.write(
                ", ".join(
                    f"0x{binary_data[j]:02X}"
                    for j in range(i, min(i + 12, binary_data_len))
                )
            )
            target_file.write(",\n")

        # Close the array definition
        target_file.write(f"}};\n")

        # add length
        target_file.write(
            f"const unsigned int {variable_name}_start   = 0x{start:08X};\n"
            f"const unsigned int {variable_name}_finish  = 0x{start+binary_data_len:08X};\n"
            f"const unsigned int {variable_name}_length   = 0x{binary_data_len:08X};\n"
        )


def main() -> None:
    """Entry point."""
    parser = argparse.ArgumentParser(
        description="Convert binary file to c array of bytes file."
    )
    parser.add_argument(
        "source", metavar="SOURCE", type=str, help="hex file to convert."
    )
    parser.add_argument(
        "target",
        metavar="TARGET",
        type=str,
        help="name of hex file to generate; use - or do not specify for stdout",
    )
    parser.add_argument(
        "-n",
        "--name",
        type=str,
        help="name of the variable, uses filename if ommited.",
        required=False,
    )

    args = parser.parse_args()

    hex_to_struct(args.source, args.target, args.name)


if __name__ == "__main__":
    main()
