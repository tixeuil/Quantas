#!/usr/bin/env python3

import argparse
import json
from pathlib import Path
from typing import Any


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def dump_json(path: Path, payload: Any) -> None:
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")


def classify_metric(value: Any) -> dict[str, Any]:
    summary: dict[str, Any] = {"type": type(value).__name__}
    if isinstance(value, list):
        summary["length"] = len(value)
        if value and all(isinstance(item, (int, float)) for item in value):
            summary["numeric"] = True
        else:
            summary["numeric"] = False
    return summary


def compare_tests(abstract_tests: list[Any], concrete_tests: list[Any]) -> list[dict[str, Any]]:
    report: list[dict[str, Any]] = []
    for index in range(max(len(abstract_tests), len(concrete_tests))):
        abstract_entry = abstract_tests[index] if index < len(abstract_tests) else {}
        concrete_entry = concrete_tests[index] if index < len(concrete_tests) else {}

        if not isinstance(abstract_entry, dict):
            abstract_entry = {"__raw__": abstract_entry}
        if not isinstance(concrete_entry, dict):
            concrete_entry = {"__raw__": concrete_entry}

        abstract_keys = sorted(abstract_entry.keys())
        concrete_keys = sorted(concrete_entry.keys())
        shared_keys = sorted(set(abstract_keys) & set(concrete_keys))

        metric_details = {}
        for key in shared_keys:
            metric_details[key] = {
                "abstract": classify_metric(abstract_entry[key]),
                "concrete": classify_metric(concrete_entry[key]),
            }

        report.append(
            {
                "index": index,
                "abstractMetricKeys": abstract_keys,
                "concreteMetricKeys": concrete_keys,
                "onlyInAbstract": sorted(set(abstract_keys) - set(concrete_keys)),
                "onlyInConcrete": sorted(set(concrete_keys) - set(abstract_keys)),
                "sharedMetricDetails": metric_details,
            }
        )
    return report


def compare_aggregates(abstract_path: Path, concrete_path: Path) -> dict[str, Any]:
    abstract_payload = load_json(abstract_path)
    concrete_payload = load_json(concrete_path)

    abstract_tests = abstract_payload.get("tests", []) if isinstance(abstract_payload, dict) else []
    concrete_tests = concrete_payload.get("tests", []) if isinstance(concrete_payload, dict) else []

    abstract_keys = sorted(abstract_payload.keys()) if isinstance(abstract_payload, dict) else []
    concrete_keys = sorted(concrete_payload.keys()) if isinstance(concrete_payload, dict) else []

    return {
        "abstractPath": str(abstract_path),
        "concretePath": str(concrete_path),
        "abstractSizeBytes": abstract_path.stat().st_size,
        "concreteSizeBytes": concrete_path.stat().st_size,
        "topLevel": {
            "abstractKeys": abstract_keys,
            "concreteKeys": concrete_keys,
            "onlyInAbstract": sorted(set(abstract_keys) - set(concrete_keys)),
            "onlyInConcrete": sorted(set(concrete_keys) - set(abstract_keys)),
        },
        "tests": {
            "abstractCount": len(abstract_tests),
            "concreteCount": len(concrete_tests),
            "perTest": compare_tests(abstract_tests, concrete_tests),
        },
    }


def prepare_abstract(input_path: Path, output_path: Path, log_path: str, experiment_index: int) -> None:
    payload = load_json(input_path)
    experiments = payload.get("experiments", [])
    if experiment_index < 0 or experiment_index >= len(experiments):
        raise IndexError(f"experiment index {experiment_index} out of range for {input_path}")

    selected_experiment = dict(experiments[experiment_index])
    selected_experiment["logFile"] = log_path

    output_payload = {
        "algorithms": payload.get("algorithms", []),
        "experiments": [selected_experiment],
    }
    dump_json(output_path, output_payload)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Prepare abstract configs and compare QUANTAS aggregates.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    prepare_parser = subparsers.add_parser("prepare-abstract", help="Create a single-experiment abstract config with an overridden log file.")
    prepare_parser.add_argument("input", type=Path)
    prepare_parser.add_argument("output", type=Path)
    prepare_parser.add_argument("logfile")
    prepare_parser.add_argument("--experiment-index", type=int, default=0)

    compare_parser = subparsers.add_parser("compare", help="Compare one abstract aggregate with one concrete aggregate.")
    compare_parser.add_argument("abstract", type=Path)
    compare_parser.add_argument("concrete", type=Path)
    compare_parser.add_argument("--report", type=Path)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "prepare-abstract":
        prepare_abstract(args.input, args.output, args.logfile, args.experiment_index)
        return 0

    if args.command == "compare":
        report = compare_aggregates(args.abstract, args.concrete)
        if args.report is not None:
            dump_json(args.report, report)
        print(json.dumps(report, indent=2))
        return 0

    parser.error(f"unsupported command: {args.command}")
    return 2


if __name__ == "__main__":
    raise SystemExit(main())