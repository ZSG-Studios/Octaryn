"""Read Clang's compilation database without executing compiler commands.

Response files, precompiled ASTs/modules and plugin flags are deliberately
unsupported: this indexer must see source and must not load command-supplied code.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


class CompileCommandError(ValueError):
    """A command cannot be faithfully used for source indexing."""


@dataclass(frozen=True)
class ParseCommand:
    source: Path
    directory: Path
    arguments: tuple[str, ...]


def _path(value: str, directory: Path) -> Path:
    candidate = Path(value)
    return (candidate if candidate.is_absolute() else directory / candidate).resolve()


def _is_source(value: str, source: Path, directory: Path) -> bool:
    return not value.startswith("-") and _path(value, directory) == source


def _compiler(arguments: list[str]) -> tuple[list[str], bool]:
    if not arguments:
        raise CompileCommandError("Compilation command has no compiler")
    name = Path(arguments[0].replace("\\", "/")).name.lower()
    if name in {"ccache", "ccache.exe", "sccache", "sccache.exe"}:
        return _compiler(arguments[1:])
    stem = name.removesuffix(".exe")
    is_cl = stem in {"cl", "clang-cl"}
    if not is_cl and not re.fullmatch(r"(?:[\w.-]+-)?(?:clang\+\+|clang|g\+\+|gcc|c\+\+|cc)(?:-[\d.]+)?", stem):
        raise CompileCommandError(f"Unsupported compiler driver: {arguments[0]}")
    return arguments[1:], is_cl


_GNU_VALUES = {
    "-I", "-D", "-U", "-isystem", "-iquote", "-idirafter", "-include",
    "-imacros", "-isysroot", "--sysroot", "-target", "--target", "-x",
    "-resource-dir", "-stdlib", "-B", "-arch", "-gcc-toolchain", "--gcc-toolchain",
}
_GNU_EXACT = {
    "-pthread", "-pedantic", "-pedantic-errors", "-ansi", "-w", "-nostdinc",
    "-nostdinc++", "-nobuiltininc", "-undef", "-ffreestanding", "-fhosted",
    "-fexceptions", "-fno-exceptions", "-frtti", "-fno-rtti", "-fPIC", "-fpic",
    "-fPIE", "-fpie", "-fno-pic", "-fno-pie", "-fms-extensions",
    "-fms-compatibility", "-fno-ms-compatibility", "-fdelayed-template-parsing",
    "-fno-delayed-template-parsing", "-fdeclspec", "-fshort-wchar",
    "-fshort-enums", "-fsigned-char", "-funsigned-char", "-fno-builtin",
    "-fno-strict-aliasing", "-fstrict-aliasing", "-fwrapv", "-fno-wrapv",
    "-ffast-math", "-fno-fast-math", "-fno-omit-frame-pointer",
    "-fomit-frame-pointer", "-fno-common", "-fcommon", "-fvisibility-inlines-hidden",
    "-fcolor-diagnostics", "-fno-color-diagnostics", "-fdiagnostics-color",
    "-fsyntax-only", "-Qunused-arguments",
}
_GNU_PREFIXES = (
    "-std=", "--std=", "--target=", "-target=", "--sysroot=", "-isysroot=",
    "-resource-dir=", "-stdlib=", "--gcc-toolchain=", "-gcc-toolchain=",
    "-fms-compatibility-version=", "-fms-extensions=", "-fvisibility=",
    "-ferror-limit=", "-ftemplate-depth=", "-fconstexpr-depth=",
    "-fconstexpr-steps=", "-fdiagnostics-color=", "-fmacro-backtrace-limit=",
    "-finput-charset=", "-fexec-charset=", "-fpack-struct=",
)
_CL_EXACT = {
    "/nologo", "/TP", "/TC", "/EHsc", "/EHs", "/EHsc-", "/EHa", "/EHs-c-",
    "/GR", "/GR-", "/MD", "/MDd", "/MT", "/MTd", "/LD", "/LDd",
    "/permissive-", "/utf-8", "/GS", "/GS-", "/Gy", "/Gy-", "/Gw", "/Gw-",
    "/Oi", "/Oi-", "/Oy", "/Oy-", "/Z7", "/Zi", "/ZI", "/bigobj",
    "/FS", "/MP", "/J", "/Za", "/Ze", "/WX", "/WX-", "/w",
    "/Od", "/O1", "/O2", "/Ox", "/Os", "/Ot", "/Ob0", "/Ob1", "/Ob2", "/Ob3",
}
_CL_PREFIXES = (
    "/std:", "/arch:", "/fp:", "/Zc:", "/Zp", "/vd", "/vm",
    "/external:W", "/source-charset:", "/execution-charset:",
)


def sanitize_arguments(arguments: list[str], source: Path, directory: Path,
                       *, has_compiler: bool = True) -> list[str]:
    """Preserve semantic flags; remove only compile/dependency/output controls.

    GNU-style explicit arguments are accepted without a compiler executable.
    MSVC database entries use Clang's cl driver rather than lossy flag translation.
    Unknown flags fail closed; expand this allowlist with a targeted test.
    """
    source, directory = source.resolve(), directory.resolve()
    args, is_cl = _compiler(list(arguments)) if has_compiler else (list(arguments), False)
    if "--driver-mode=cl" in args:
        is_cl = True
        args.remove("--driver-mode=cl")
    result = ["--driver-mode=cl"] if is_cl else []
    result.append(f"-working-directory={directory}")
    index = 0
    while index < len(args):
        arg = args[index]
        index += 1
        if _is_source(arg, source, directory):
            continue
        if arg.startswith("@") or arg in {"-Xclang", "-Xpreprocessor", "/link"}:
            raise CompileCommandError(f"Unsupported indirect compiler argument: {arg}")
        if arg in {"-c", "-S", "-MD", "-MMD", "-MP", "-M", "-MM", "-MG"}:
            continue
        if arg in {"-o", "-MF", "-MT", "-MQ", "-MJ", "--serialize-diagnostics"}:
            if index == len(args):
                raise CompileCommandError(f"Missing value for {arg}")
            index += 1
            continue
        if any(arg.startswith(prefix) and len(arg) > len(prefix)
               for prefix in ("-MF", "-MT", "-MQ", "-MJ", "--serialize-diagnostics=")):
            continue
        if arg.startswith("-o") and len(arg) > 2 and not arg.startswith("-objc"):
            continue
        if is_cl:
            if arg in {"/c", "/showIncludes"}:
                continue
            output = next((p for p in ("/Fo", "/Fd", "/Fe", "/Fa", "/sourceDependencies")
                           if arg.startswith(p)), None)
            if output:
                if arg == output:
                    if index == len(args):
                        raise CompileCommandError(f"Missing value for {arg}")
                    index += 1
                continue
            value_flag = next((p for p in ("/external:I", "/imsvc", "/FI", "/I", "/D", "/U")
                               if arg.startswith(p)), None)
            if value_flag:
                result.append(arg)
                if arg == value_flag:
                    if index == len(args):
                        raise CompileCommandError(f"Missing value for {arg}")
                    result.append(args[index])
                    index += 1
                continue
            if arg in _CL_EXACT or arg.startswith(_CL_PREFIXES) or re.fullmatch(r"/(?:W[0-4]|wd\d+|we\d+|wo\d+)", arg):
                result.append(arg)
                continue
        if arg in _GNU_VALUES:
            if index == len(args):
                raise CompileCommandError(f"Missing value for {arg}")
            result.extend((arg, args[index]))
            index += 1
        elif arg in _GNU_EXACT or arg.startswith(_GNU_PREFIXES):
            result.append(arg)
        elif any(arg.startswith(p) and len(arg) > len(p) for p in ("-I", "-D", "-U", "-isystem", "-iquote", "-idirafter")):
            result.append(arg)
        elif re.fullmatch(r"-(?:O[0-3sgzfast]*|g(?:[0-3]|line-tables-only|dwarf-\d)?|W[\w=,+-]+|m[\w=.+-]+)", arg):
            result.append(arg)
        else:
            raise CompileCommandError(f"Unsupported compiler argument: {arg}")
    return result


def load_commands(database: Path, sources: list[Path], arguments: list[str]) -> list[ParseCommand]:
    """Use libclang's database tokenizer, including quoted Windows commands.

    Each matching configuration is indexed; extra arguments are appended to the
    database command. A missing source entry is an error, never a guessed command.
    """
    from clang import cindex

    database = database.resolve()
    directory = database.parent if database.is_file() else database
    if database.is_file() and database.name != "compile_commands.json":
        raise CompileCommandError("Compilation database file must be named compile_commands.json")
    try:
        commands = cindex.CompilationDatabase.fromDirectory(str(directory)).getAllCompileCommands()
    except cindex.CompilationDatabaseError as exc:
        raise CompileCommandError(f"Cannot load compilation database: {directory}") from exc
    selected = {source.resolve(): [] for source in sources}
    for command in commands or ():
        workdir = _path(command.directory, directory)
        source = _path(command.filename, workdir)
        if source in selected:
            normalized = sanitize_arguments(list(command.arguments) + arguments, source, workdir)
            selected[source].append(ParseCommand(source, workdir, tuple(normalized)))
    missing = [str(source) for source, matches in selected.items() if not matches]
    if missing:
        raise CompileCommandError("No compilation command for: " + ", ".join(missing))
    return list(dict.fromkeys(command for matches in selected.values() for command in matches))


def make_commands(root: Path, sources: list[Path], arguments: list[str],
                  compilation_database: Path | None = None) -> list[ParseCommand]:
    root = root.resolve()
    sources = [_path(str(source), root) for source in sources]
    if compilation_database is not None:
        return load_commands(_path(str(compilation_database), root), sources, arguments)
    return [ParseCommand(source, root, tuple(sanitize_arguments(
        arguments, source, root, has_compiler=False))) for source in dict.fromkeys(sources)]
