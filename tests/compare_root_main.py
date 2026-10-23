"""Build main and the working headers against one ROOT installation and compare results.

Requires root-config, a C++17 compiler, and the project's FastBDT headers/library.
No checkout, branch switch, or commit is performed. Snapshots and logs are retained.
"""

import argparse
import filecmp
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile


REPO = Path(__file__).resolve().parents[1]


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, encoding="utf-8", errors="replace", **kwargs)


def windows_environment(root):
    installations = sorted((Path(os.environ["ProgramFiles"]) / "Microsoft Visual Studio").glob("*/*/VC/Auxiliary/Build/vcvars64.bat"))
    if not installations:
        raise RuntimeError("Visual Studio x64 C++ tools were not found")
    with tempfile.TemporaryDirectory(prefix="belle2_root_msvc_") as directory:
        setup = Path(directory) / "setup.cmd"
        setup.write_text(f'@echo off\ncall "{installations[-1]}" >nul\nif errorlevel 1 exit /b 1\ncall "{root / "bin/thisroot.bat"}" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding="utf-8")
        environment = run(["cmd.exe", "/d", "/c", str(setup)], capture_output=True).stdout
        for line in environment.splitlines():
            if "=" in line and not line.startswith("="):
                key, value = line.split("=", 1)
                os.environ[key] = value
    compilers = sorted(installations[-1].parents[2].glob("Tools/MSVC/*/bin/Hostx64/x64/cl.exe"))
    return str(compilers[-1])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ref", default="main")
    parser.add_argument("--cxx", default=os.environ.get("CXX", "cl" if os.name == "nt" else "g++"))
    parser.add_argument("--root-dir", type=Path, help="Portable ROOT directory, required for Windows")
    parser.add_argument("--fastbdt-include", type=Path, default=REPO / "FastBDT/include")
    parser.add_argument("--fastbdt-lib", type=Path, default=REPO / "lib")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    windows = os.name == "nt"
    if windows:
        if args.root_dir is None or not (args.root_dir / "bin/thisroot.bat").exists():
            parser.error("set --root-dir to a compatible ROOT Windows distribution")
        args.root_dir = args.root_dir.resolve()
        compiler = windows_environment(args.root_dir)
        if args.cxx == "cl":
            args.cxx = compiler
    elif shutil.which("root-config") is None:
        parser.error("root-config is unavailable; run in the analysis ROOT environment")
    if not (args.fastbdt_include / "Classifier.h").exists():
        parser.error("FastBDT headers are unavailable; set --fastbdt-include")
    output = (args.output or Path(tempfile.mkdtemp(prefix="belle2_root_compare_"))).resolve()
    output.mkdir(parents=True, exist_ok=True)
    print(f"Artifacts: {output}", flush=True)
    revision = run(["git", "rev-parse", args.ref], cwd=REPO, capture_output=True).stdout.strip()
    headers = run(["git", "ls-tree", "-r", "--name-only", revision, "include"], cwd=REPO, capture_output=True).stdout.splitlines()
    baseline_include = output / "baseline/include"
    candidate_include = output / "candidate/include"
    for name in headers:
        target = output / "baseline" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        result = subprocess.run(["git", "show", f"{revision}:{name}"], cwd=REPO, check=True, capture_output=True)
        target.write_bytes(result.stdout)
    shutil.copytree(REPO / "include", candidate_include)
    if windows:
        version_header = (args.root_dir / "include/ROOT/RVersion.hxx").read_text()
        version = ".".join(re.search(r"#define\s+ROOT_VERSION_" + part + r"\s+(\d+)", version_header).group(1) for part in ("MAJOR", "MINOR", "PATCH"))
    else:
        version = run(["root-config", "--version"], capture_output=True).stdout.strip()
        cflags = shlex.split(run(["root-config", "--cflags"], capture_output=True).stdout)
        libs = shlex.split(run(["root-config", "--ldflags", "--glibs"], capture_output=True).stdout)

    def build(source, executable, include, feature):
        if windows:
            command = [args.cxx, "/nologo", "/std:c++17", "/permissive-", "/FIstring", "/O2", "/MD", "/EHsc", "/utf-8", "/Zc:__cplusplus", "/D_CRT_SECURE_NO_WARNINGS", "/D_USE_MATH_DEFINES", "/DNOMINMAX", "/I" + str(include), "/I" + str(args.fastbdt_include.resolve()), "/I" + str(args.root_dir / "include")]
            if feature:
                command.append("/DBATCH_FEATURE")
            command += [str(source), "/Fe:" + str(executable), "/Fo:" + str(executable.with_suffix(".obj")), "/link", "/LIBPATH:" + str(args.root_dir / "lib"), "/LIBPATH:" + str(args.fastbdt_lib.resolve())]
            command += [name + ".lib" for name in ("libCore", "libRIO", "libTree", "libTreePlayer", "libHist", "libGraf", "libGpad", "libMatrix", "libMathCore", "libRooFit", "libRooStats", "libRooFitCore", "libMinuit", "FastBDT_static")]
        else:
            command = shlex.split(args.cxx) + cflags + ["-std=c++17", "-O2", "-I" + str(include), "-I" + str(args.fastbdt_include.resolve())]
            if feature:
                command.append("-DBATCH_FEATURE")
            command += [str(source), "-o", str(executable)]
            command += libs + ["-L" + str(args.fastbdt_lib.resolve()), "-lRooFit", "-lRooStats", "-lRooFitCore", "-lMinuit", "-lFastBDT_static"]
        with executable.with_suffix(".build.log").open("w") as log:
            run(command, stdout=log, stderr=subprocess.STDOUT)

    suffix = ".exe" if windows else ""
    executables = {}
    for label, include in (("baseline", baseline_include), ("candidate", candidate_include)):
        executable = output / label / ("root_main_driver" + suffix)
        build(REPO / "tests/root_main_driver.cc", executable, include, label == "candidate")
        executables[label] = executable
    integration_executable = output / "candidate" / ("batch_loading" + suffix)
    build(REPO / "tests/batch_loading.cc", integration_executable, candidate_include, True)
    with (output / "integration_run.log").open("w") as log:
        run([str(integration_executable)], stdout=log, stderr=subprocess.STDOUT)
        for mode in ("missing-key", "empty-key", "zero-size"):
            result = subprocess.run([str(integration_executable), mode], stdout=log, stderr=subprocess.STDOUT)
            if result.returncode != 1:
                raise RuntimeError(f"Expected validation failure for {mode}, got {result.returncode}")
    fixture = output / "input"
    run([str(executables["candidate"]), "create", str(fixture)])
    report = {
        "baseline": revision,
        "root_version": version,
        "compiler": args.cxx,
        "candidate_header_sha256": {str(p.relative_to(candidate_include)): hashlib.sha256(p.read_bytes()).hexdigest() for p in candidate_include.rglob("*.h")},
        "scope": "Synthetic ROOT files; rows, branch schemas/values, cutflow, histogram bins/errors; no FastBDT training or fit comparison",
        "cases": [],
    }
    def execute(label, destination, size, cut, random_modules):
        destination.mkdir()
        with (destination / "run.log").open("w") as log:
            run([str(executables[label]), "run", str(fixture), str(destination), str(size), str(cut), str(random_modules)], stdout=log, stderr=subprocess.STDOUT)
            run([str(executables[label]), "dump", str(destination)], stdout=log, stderr=subprocess.STDOUT)
    for cut in (0, 1):
        for random_modules in (0, 1):
            expected = output / f"main_cut{cut}_random{random_modules}"
            execute("baseline", expected, 0, cut, random_modules)
            for size in (1, 17, 100000):
                actual = output / f"batch{size}_cut{cut}_random{random_modules}"
                execute("candidate", actual, size, cut, random_modules)
                equal = all(filecmp.cmp(expected / name, actual / name, shallow=False) for name in ("trees.txt", "summary.txt"))
                report["cases"].append({"cut": cut, "random_modules": random_modules, "batch_size": size, "equal": equal})
                print(f"{'PASS' if equal else 'FAIL'} cut={cut} random={random_modules} batch={size}")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"Report: {output / 'report.json'}")
    return 0 if all(case["equal"] for case in report["cases"]) else 1


if __name__ == "__main__":
    raise SystemExit(main())
