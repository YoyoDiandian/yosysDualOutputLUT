#!/usr/bin/env python3

import json
import sys
from collections import deque

KNOWN_PORT_DIRECTIONS = {
    "GTP_LUT1": {"I0": "input", "Z": "output"},
    "GTP_LUT2": {"I0": "input", "I1": "input", "Z": "output"},
    "GTP_LUT3": {"I0": "input", "I1": "input", "I2": "input", "Z": "output"},
    "GTP_LUT4": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "Z": "output"},
    "GTP_LUT5": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "I4": "input", "Z": "output"},
    "GTP_LUT6": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "I4": "input", "I5": "input", "Z": "output"},
    "GTP_LUT4D": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "Z": "output", "Z5": "output"},
    "GTP_LUT5D": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "I4": "input", "Z": "output", "Z5": "output"},
    "GTP_LUT6D": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "I4": "input", "I5": "input", "Z": "output", "Z5": "output"},
    "GTP_VERSAL_LUT6D": {"I0": "input", "I1": "input", "I2": "input", "I3": "input", "I4": "input", "I5": "input", "O6": "output", "O5": "output"},
}


def choose_top_module(modules):
    for name, module in modules.items():
        attrs = module.get("attributes", {})
        top_attr = attrs.get("top")
        if top_attr in (1, "1", True):
            return name
        if isinstance(top_attr, str) and top_attr.strip("0") == "1":
            return name
    return next(iter(modules))


def bit_is_const(bit):
    return isinstance(bit, str)


def main():
    if len(sys.argv) not in (2, 3):
        print(f"Usage: {sys.argv[0]} <design.json> [module_name]", file=sys.stderr)
        return 2

    with open(sys.argv[1], "r", encoding="utf-8") as f:
        design = json.load(f)

    modules = design.get("modules", {})
    if not modules:
        print("JSON design does not contain any modules.", file=sys.stderr)
        return 1

    module_name = sys.argv[2] if len(sys.argv) == 3 else choose_top_module(modules)
    if module_name not in modules:
        print(f"Module '{module_name}' not found in JSON design.", file=sys.stderr)
        return 1

    module = modules[module_name]
    input_bits = set()
    output_bits = []
    for port_info in module.get("ports", {}).values():
        bits = [bit for bit in port_info.get("bits", []) if not bit_is_const(bit)]
        direction = port_info.get("direction")
        if direction == "input":
            input_bits.update(bits)
        elif direction == "output":
            output_bits.extend(bits)

    cell_inputs = {}
    bit_driver = {}
    cell_fanouts = {}
    cell_pending = {}
    cell_max_input_level = {}
    for cell_name, cell_info in module.get("cells", {}).items():
        port_dirs = dict(KNOWN_PORT_DIRECTIONS.get(cell_info.get("type"), {}))
        port_dirs.update(cell_info.get("port_directions", {}))
        inputs = []
        for port_name, bits in cell_info.get("connections", {}).items():
            direction = port_dirs.get(port_name)
            live_bits = [bit for bit in bits if not bit_is_const(bit)]
            if direction == "input":
                inputs.extend(live_bits)
            elif direction == "output":
                for bit in live_bits:
                    bit_driver[bit] = cell_name
        cell_inputs[cell_name] = inputs

    for cell_name in cell_inputs:
        deps = set()
        for bit in cell_inputs[cell_name]:
            if bit in input_bits:
                continue
            driver = bit_driver.get(bit)
            if driver is not None and driver != cell_name:
                deps.add(driver)
        cell_pending[cell_name] = len(deps)
        cell_max_input_level[cell_name] = 0
        for dep in deps:
            cell_fanouts.setdefault(dep, []).append(cell_name)

    cell_level = {}
    ready = deque()
    for cell_name, pending in cell_pending.items():
        if pending == 0:
            cell_level[cell_name] = 1
            ready.append(cell_name)

    while ready:
        cell_name = ready.popleft()
        current_level = cell_level[cell_name]
        for fanout in cell_fanouts.get(cell_name, []):
            if current_level > cell_max_input_level[fanout]:
                cell_max_input_level[fanout] = current_level
            cell_pending[fanout] -= 1
            if cell_pending[fanout] == 0:
                cell_level[fanout] = cell_max_input_level[fanout] + 1
                ready.append(fanout)

    if len(cell_level) != len(cell_inputs):
        missing = len(cell_inputs) - len(cell_level)
        print(f"Could not topologically evaluate {missing} cells.", file=sys.stderr)
        return 1

    if not output_bits:
        print(0)
        return 0

    max_level = 0
    for bit in output_bits:
        if bit in input_bits:
            continue
        driver = bit_driver.get(bit)
        if driver is not None:
            max_level = max(max_level, cell_level.get(driver, 0))

    print(max_level)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
