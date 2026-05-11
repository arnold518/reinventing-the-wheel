#!/usr/bin/env python3
"""Browser-based CircuitSim visualizer.

This server intentionally uses only the Python standard library plus the
existing pybind11 `circuit_backend` module. The browser owns rendering and
interaction; Python owns circuit discovery, simulator state scrubbing, and
layout persistence.
"""

from __future__ import annotations

import argparse
import copy
import json
import mimetypes
import os
from pathlib import Path
import re
import socket
import sys
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, unquote, urlparse


ROOT_DIR = Path(__file__).resolve().parents[1]
APP_DIR = Path(__file__).resolve().parent
STATIC_DIR = APP_DIR / "static"
LAYOUT_PATH = APP_DIR / "layout.json"
LEGACY_LAYOUT_PATH = ROOT_DIR / "visualizer" / "layout.json"
LEGACY_BACKEND_DIR = ROOT_DIR / "visualizer"

if str(LEGACY_BACKEND_DIR) not in sys.path:
    sys.path.insert(0, str(LEGACY_BACKEND_DIR))

try:
    import circuit_backend
except ImportError as exc:  # pragma: no cover - exercised manually
    raise SystemExit(
        "Could not import circuit_backend. Build the pybind11 backend first "
        "or ensure visualizer/circuit_backend*.so exists."
    ) from exc


STATE_TOKENS = {
    circuit_backend.LogicValue.HIGH: "1",
    circuit_backend.LogicValue.LOW: "0",
    circuit_backend.LogicValue.HIGH_Z: "Z",
    circuit_backend.LogicValue.UNKNOWN: "X",
}

DEFAULT_COLOR = [61, 90, 128, 100]
DEFAULT_ROOT_LAYOUT = {"pos": [50, 150], "width": 800}
NON_VISUALIZABLE_TESTS = {"WireTemplateTest", "RewireValidationTest"}
PREFERRED_SCENARIO_ALIASES = {
    "FullAdderTest": ["full-adder", "fulladder"],
    "HalfAdderTest": ["half-adder", "halfadder"],
    "FullCircuitTest": ["full-circuit", "fullcircuit"],
    "DFlipFlopTest": ["dff", "d-flip-flop"],
    "ClockGeneratorTest": ["clock"],
    "Adder8Test": ["adder8", "8-bit-adder"],
    "ZeroDetect8Test": ["zero-detect8"],
    "ALU8Test": ["alu8"],
}


def _clone(value: Any) -> Any:
    return copy.deepcopy(value)


def _logic_token(value: Any) -> str:
    return STATE_TOKENS.get(value, "X")


def _signal_width(cpp_handle: Any) -> int:
    try:
        return max(1, int(cpp_handle.get_width()))
    except (AttributeError, TypeError, ValueError):
        return 1


def _signal_value(cpp_handle: Any) -> str:
    width = _signal_width(cpp_handle)
    bits: list[str] = []
    for index in range(width):
        try:
            bits.append(_logic_token(cpp_handle.get_bit(index)))
        except (AttributeError, IndexError, RuntimeError):
            if index == 0:
                bits.append(_logic_token(cpp_handle.get_value()))
            else:
                bits.append("X")
    return bits[0] if width == 1 else "".join(reversed(bits))


class LayoutManager:
    def __init__(self, filepath: Path):
        self.filepath = filepath
        source_path = filepath if filepath.exists() else LEGACY_LAYOUT_PATH
        try:
            data = json.loads(source_path.read_text())
        except FileNotFoundError:
            data = {}

        self.settings = data.get("default_settings", {})
        self.type_layouts = data.get("type_layouts", {})
        self.instance_layouts = data.get("instance_layouts", {})
        self.root_layout_config = data.get("root_layout_config", _clone(DEFAULT_ROOT_LAYOUT))

    def get_layout_for(self, component_type: str, instance_key: str) -> dict[str, Any]:
        if instance_key in self.instance_layouts:
            return _clone(self.instance_layouts[instance_key])
        return _clone(self.type_layouts.get(component_type, {}))

    def get_child_layout(self, parent_type: str, parent_key: str, child_name: str) -> dict[str, Any]:
        parent_layout = self.get_layout_for(parent_type, parent_key)
        return _clone(parent_layout.get("children", {}).get(child_name, {}))

    def update_instance_child_layout(
        self, parent_key: str, child_name: str, rel_pos: list[float], rel_width: float
    ) -> None:
        instance_layout = self.instance_layouts.setdefault(parent_key, {})
        child_entry = instance_layout.setdefault("children", {}).setdefault(child_name, {})
        child_entry["rel_pos"] = rel_pos
        child_entry["rel_width"] = rel_width

    def update_type_child_layout(
        self, parent_type: str, child_name: str, rel_pos: list[float], rel_width: float
    ) -> None:
        type_layout = self.type_layouts.setdefault(parent_type, {})
        child_entry = type_layout.setdefault("children", {}).setdefault(child_name, {})
        child_entry["rel_pos"] = rel_pos
        child_entry["rel_width"] = rel_width

    def to_jsonable(self) -> dict[str, Any]:
        return {
            "default_settings": self.settings,
            "type_layouts": self.type_layouts,
            "instance_layouts": self.instance_layouts,
            "root_layout_config": self.root_layout_config,
        }

    def replace_from_client(self, layout: dict[str, Any]) -> None:
        self.settings = _clone(layout.get("default_settings", {}))
        self.type_layouts = _clone(layout.get("type_layouts", {}))
        self.instance_layouts = _clone(layout.get("instance_layouts", {}))
        self.root_layout_config = _clone(layout.get("root_layout_config", DEFAULT_ROOT_LAYOUT))

    def save(self) -> None:
        self.filepath.parent.mkdir(parents=True, exist_ok=True)
        pretty = json.dumps(self.to_jsonable(), indent=4)

        def format_list_content(match: re.Match[str]) -> str:
            compact = re.sub(r"\s+", "", match.group(1)).replace(",", ", ")
            return f"[{compact}]"

        final = re.sub(r"\[(.*?)\]", format_list_content, pretty, flags=re.DOTALL)
        self.filepath.write_text(final)


def scenario_default_alias(class_name: str) -> str:
    stem = class_name[:-4] if class_name.endswith("Test") else class_name
    stem = stem.replace("_", "-")
    stem = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1-\2", stem)
    stem = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", stem)
    return stem.lower()


def scenario_canonical_alias(class_name: str) -> str:
    return PREFERRED_SCENARIO_ALIASES.get(class_name, [scenario_default_alias(class_name)])[0]


def visualizable_test_names() -> list[str]:
    return [
        class_name
        for class_name in circuit_backend.get_registered_test_names()
        if class_name not in NON_VISUALIZABLE_TESTS and hasattr(circuit_backend, class_name)
    ]


def _scenario_classes() -> dict[str, type]:
    # Expose registry tests that build a reusable topology during setup_circuit().
    # The excluded tests intentionally verify API/validation behavior without a
    # meaningful visual root.

    scenarios: dict[str, type] = {}
    for class_name in visualizable_test_names():
        scenario_class = getattr(circuit_backend, class_name)
        aliases = {scenario_default_alias(class_name), class_name.lower(), *PREFERRED_SCENARIO_ALIASES.get(class_name, [])}
        for alias in aliases:
            scenarios[alias] = scenario_class
    return scenarios


def _scenario_options() -> list[dict[str, str]]:
    return [
        {"key": scenario_canonical_alias(class_name), "className": class_name}
        for class_name in visualizable_test_names()
    ]


SCENARIOS = _scenario_classes()
SCENARIO_OPTIONS = _scenario_options()
DEFAULT_SCENARIO = "adder8" if "adder8" in SCENARIOS else sorted(SCENARIOS)[0]


class CircuitSession:
    def __init__(self, scenario_key: str, layout_manager: LayoutManager):
        self.scenario_key = scenario_key
        self.layout_manager = layout_manager
        self.lock = threading.Lock()

        self.test_scenario = SCENARIOS[scenario_key]()
        self.test_scenario.setup_circuit()
        self.root = self.test_scenario.get_root()
        if not self.root:
            raise RuntimeError("C++ test scenario did not produce a root component")

        self.simulator = self.test_scenario.get_simulator()
        self.simulator.run_and_record(self.test_scenario.get_run_duration())
        self.timestamps = list(self.simulator.get_unique_timestamps())
        if not self.timestamps:
            self.timestamps = [0]
        self.simulator.set_circuit_state_at_time(self.timestamps[0])

        self.components: list[dict[str, Any]] = []
        self.pins: list[dict[str, Any]] = []
        self.wires: list[dict[str, Any]] = []
        self.pin_handles: dict[str, Any] = {}
        self.wire_handles: dict[str, Any] = {}
        self._build_topology()
        if len(self.components) == 1 and not self.pins and not self.wires:
            raise RuntimeError(
                f"Scenario '{scenario_key}' did not expose a visual circuit topology. "
                "Its C++ test likely creates local components only during verifyResults()."
            )

    def _component_layout_meta(self, component: Any) -> tuple[float, list[int]]:
        layout = self.layout_manager.get_layout_for(component.get_type_name(), component.get_id())
        return float(layout.get("aspect_ratio", 0.75 if component == self.root else 1.0)), layout.get("color", DEFAULT_COLOR)

    def _build_topology(self) -> None:
        def walk(component: Any, parent_id: str | None, depth: int) -> None:
            component_id = component.get_id()
            component_type = component.get_type_name()
            children = list(component.get_children())
            aspect_ratio, color = self._component_layout_meta(component)

            self.components.append(
                {
                    "id": component_id,
                    "name": component.get_name(),
                    "type": component_type,
                    "parentId": parent_id,
                    "depth": depth,
                    "childIds": [child.get_id() for child in children],
                    "aspectRatio": aspect_ratio,
                    "color": color,
                }
            )

            if hasattr(component, "get_input_pins"):
                for pin_name, pin in component.get_input_pins().items():
                    self._add_pin(component_id, pin_name, pin, "input")
                for pin_name, pin in component.get_output_pins().items():
                    self._add_pin(component_id, pin_name, pin, "output")

            self._add_wires(component)
            self._ensure_child_defaults(component, children, depth)
            for child in children:
                walk(child, component_id, depth + 1)

        walk(self.root, None, 0)

    def _add_pin(self, component_id: str, pin_name: str, pin: Any, pin_type: str) -> None:
        try:
            pin_id = pin.get_id()
        except AttributeError:
            pin_id = f"{component_id}.{pin_name}"
        self.pin_handles[pin_id] = pin
        self.pins.append(
            {
                "id": pin_id,
                "componentId": component_id,
                "name": pin_name,
                "type": pin_type,
                "width": _signal_width(pin),
            }
        )

    def _add_wires(self, component: Any) -> None:
        for wire in component.get_wires():
            wire_id = wire.get_id()
            self.wire_handles[wire_id] = wire
            source_pin = wire.get_source_pin()
            source_pin_id = source_pin.get_id() if source_pin else None
            sink_pin_ids = [pin.get_id() for pin in wire.get_sink_pins()]
            self.wires.append(
                {
                    "id": wire_id,
                    "name": wire.get_name(),
                    "ownerId": component.get_id(),
                    "sourcePinId": source_pin_id,
                    "sinkPinIds": sink_pin_ids,
                    "width": _signal_width(wire),
                }
            )

    def _ensure_child_defaults(self, component: Any, children: list[Any], depth: int) -> None:
        count = len(children)
        if count == 0:
            return

        cols = int((count ** 0.5) + 0.999999)
        rows = int((count / cols) + 0.999999) if cols > 0 else 0
        parent_type = component.get_type_name()
        parent_key = component.get_id()
        parent_layout = self.layout_manager.get_layout_for(parent_type, parent_key)
        parent_aspect = float(parent_layout.get("aspect_ratio", 0.75 if depth == 0 else 1.0))
        title_ratio = float(self.layout_manager.settings.get("title_bar_ratio", 0.15))
        title_fraction = min(0.7, title_ratio / max(parent_aspect, 0.01))
        usable_y_fraction = max(0.1, 1.0 - title_fraction)

        for index, child in enumerate(children):
            child_layout = self.layout_manager.get_child_layout(parent_type, parent_key, child.get_name())
            if "rel_pos" in child_layout and "rel_width" in child_layout:
                continue

            padding = 0.1
            cell_w = (1.0 - padding * (cols + 1)) / cols if cols > 0 else 0
            cell_h = (1.0 - padding * (rows + 1)) / rows if rows > 0 else 0
            col = index % cols
            row = index // cols
            rel_width = cell_w
            rel_pos = [
                padding + col * (cell_w + padding),
                title_fraction + usable_y_fraction * (padding + row * (cell_h + padding)),
            ]

            if depth == 0:
                self.layout_manager.update_instance_child_layout(parent_key, child.get_name(), rel_pos, rel_width)
            else:
                self.layout_manager.update_type_child_layout(parent_type, child.get_name(), rel_pos, rel_width)

    def topology_response(self) -> dict[str, Any]:
        return {
            "scenario": self.scenario_key,
            "rootId": self.root.get_id(),
            "rootType": self.root.get_type_name(),
            "components": self.components,
            "pins": self.pins,
            "wires": self.wires,
            "timestamps": self.timestamps,
            "layout": self.layout_manager.to_jsonable(),
            "state": self.state_at_index(0),
            "stats": {
                "componentCount": len(self.components),
                "pinCount": len(self.pins),
                "wireCount": len(self.wires),
                "timestampCount": len(self.timestamps),
            },
        }

    def state_at_index(self, index: int) -> dict[str, Any]:
        with self.lock:
            index = max(0, min(index, len(self.timestamps) - 1))
            timestamp = int(self.timestamps[index])
            self.simulator.set_circuit_state_at_time(timestamp)
            pins = {pin_id: _signal_value(pin) for pin_id, pin in self.pin_handles.items()}
            wires = {wire_id: _signal_value(wire) for wire_id, wire in self.wire_handles.items()}
            return {"index": index, "time": timestamp, "pins": pins, "wires": wires}


class SessionStore:
    def __init__(self) -> None:
        self.layout_manager = LayoutManager(LAYOUT_PATH)
        self.sessions: dict[str, CircuitSession] = {}
        self.lock = threading.Lock()

    def get(self, scenario: str) -> CircuitSession:
        scenario_key = normalize_scenario(scenario)
        with self.lock:
            if scenario_key not in self.sessions:
                self.sessions[scenario_key] = CircuitSession(scenario_key, self.layout_manager)
            return self.sessions[scenario_key]

    def replace_layout(self, layout: dict[str, Any]) -> None:
        with self.lock:
            self.layout_manager.replace_from_client(layout)
            self.layout_manager.save()
            self.sessions.clear()


STORE = SessionStore()


def normalize_scenario(scenario: str | None) -> str:
    key = (scenario or DEFAULT_SCENARIO).strip().lower().replace("_", "-")
    if key not in SCENARIOS:
        valid = ", ".join(sorted(SCENARIOS))
        raise ValueError(f"Unknown scenario '{scenario}'. Valid scenarios: {valid}")
    return key


def json_response(handler: SimpleHTTPRequestHandler, payload: Any, status: int = 200) -> None:
    encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json; charset=utf-8")
    handler.send_header("Content-Length", str(len(encoded)))
    handler.send_header("Cache-Control", "no-store")
    handler.end_headers()
    handler.wfile.write(encoded)


def error_response(handler: SimpleHTTPRequestHandler, message: str, status: int = 400) -> None:
    json_response(handler, {"error": message}, status)


class VisualizerHandler(SimpleHTTPRequestHandler):
    server_version = "CircuitSimVisualizerV2/1.0"

    def _normalize_path(self, path: str) -> str:
        api_index = path.find("/api/")
        if api_index >= 0:
            return path[api_index:]
        if path.endswith("/api"):
            return "/api"
        absproxy_index = path.find("/absproxy/")
        if absproxy_index >= 0:
            parts = path[absproxy_index:].split("/", 3)
            if len(parts) >= 4:
                return "/" + parts[3]
            return "/"
        return path

    def _base_href_for_request(self, request_path: str) -> str:
        absproxy_index = request_path.find("/absproxy/")
        if absproxy_index >= 0:
            parts = request_path[absproxy_index:].split("/", 3)
            if len(parts) >= 3 and parts[2]:
                return f"/absproxy/{parts[2]}/"
        return "/"

    def do_GET(self) -> None:  # noqa: N802 - http.server API
        parsed = urlparse(self.path)
        raw_path = parsed.path
        path = self._normalize_path(raw_path)
        query = parse_qs(parsed.query)

        try:
            if path == "/api/health":
                json_response(self, {"ok": True})
            elif path == "/api/scenarios":
                json_response(self, {"default": DEFAULT_SCENARIO, "scenarios": SCENARIO_OPTIONS})
            elif path == "/api/circuit":
                scenario = query.get("scenario", [DEFAULT_SCENARIO])[0]
                json_response(self, STORE.get(scenario).topology_response())
            elif path == "/api/state":
                scenario = query.get("scenario", [DEFAULT_SCENARIO])[0]
                index = int(query.get("index", [0])[0])
                json_response(self, STORE.get(scenario).state_at_index(index))
            else:
                self._serve_static(path, raw_path)
        except Exception as exc:
            error_response(self, str(exc), 500 if not isinstance(exc, ValueError) else 400)

    def do_POST(self) -> None:  # noqa: N802 - http.server API
        parsed = urlparse(self.path)
        if self._normalize_path(parsed.path) != "/api/layout":
            error_response(self, "Unknown POST endpoint", 404)
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            body = self.rfile.read(length).decode("utf-8")
            payload = json.loads(body or "{}")
            layout = payload.get("layout")
            if not isinstance(layout, dict):
                raise ValueError("POST /api/layout requires a layout object")
            STORE.replace_layout(layout)
            json_response(self, {"ok": True, "path": str(LAYOUT_PATH)})
        except Exception as exc:
            error_response(self, str(exc), 400)

    def _serve_static(self, request_path: str, raw_path: str | None = None) -> None:
        safe_path = unquote(request_path).split("?", 1)[0]
        if safe_path in ("", "/"):
            safe_path = "/index.html"
        target = (STATIC_DIR / safe_path.lstrip("/")).resolve()
        if not str(target).startswith(str(STATIC_DIR.resolve())) or not target.exists() or target.is_dir():
            if "." not in Path(safe_path).name:
                target = STATIC_DIR / "index.html"
            elif Path(safe_path).name in {"styles.css", "app.js"}:
                target = STATIC_DIR / Path(safe_path).name
            else:
                self.send_error(404, "File not found")
                return
        if not str(target.resolve()).startswith(str(STATIC_DIR.resolve())) or not target.exists() or target.is_dir():
            self.send_error(404, "File not found")
            return

        if target.name == "index.html":
            html = target.read_text()
            base_href = self._base_href_for_request(raw_path or request_path)
            html = html.replace("<head>", f'<head>\n  <base href="{base_href}">', 1)
            content = html.encode("utf-8")
        else:
            content = target.read_bytes()
        mime_type = mimetypes.guess_type(str(target))[0] or "application/octet-stream"
        self.send_response(200)
        self.send_header("Content-Type", mime_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, fmt: str, *args: Any) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def find_available_port(host: str, preferred_port: int) -> int:
    for port in range(preferred_port, preferred_port + 100):
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            try:
                sock.bind((host, port))
            except OSError:
                continue
            return port
    raise RuntimeError(f"No free port found from {preferred_port} to {preferred_port + 99}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Run the CircuitSim HTML visualizer.")
    parser.add_argument("--host", default=os.environ.get("VISUALIZER_V2_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("VISUALIZER_V2_PORT", "8765")))
    args = parser.parse_args()

    port = find_available_port(args.host, args.port)
    server = ThreadingHTTPServer((args.host, port), VisualizerHandler)
    print(f"CircuitSim visualizer v2 running at http://{args.host}:{port}/")
    print(f"Layout file: {LAYOUT_PATH}")
    server.serve_forever()


if __name__ == "__main__":
    main()
