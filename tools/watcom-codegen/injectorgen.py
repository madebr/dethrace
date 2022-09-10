#!/usr/bin/env python
import enum
import itertools
import json
import logging
import os
from pathlib import Path
import shutil
import textwrap

from dumpparser import *
from codegen2 import *


logger = logging.getLogger(__name__)


class WebHookSymbol:
    def __init__(self, var_name:str, symbol_name: str, location: str):
        self.var_name = var_name
        self.symbol_name = symbol_name
        self.location = location


class InjectorGenerator(DethRaceProjectGenerator):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.forbidden_functions = (
            "matherr",
            "LoopLimitTooLow",
        )
        self._web_hook_symbols: dict[str, WebHookSymbol] = {}

    def _get_original_symbol_name(self, function: DumpedFunction, name: str) -> str:
        return "original_" + name

    def get_source_extra(self, module: DumpedModule) -> str:
        lower_name = module.name.lower()
        s  = "#include \"harness/trace.h\"\n\n"
        s += "#include \"carm95_hooks.h\"\n\n"
        s += "#include \"carm95_webserver.h\"\n\n"
        s += "#include <assert.h>\n"
        if any(part in lower_name for part in ("stdfile", )):
            s += "#include <stdio.h>\n"
        if "dossys.c" in lower_name:
            s += "#include <sys/stat.h>\n"
        return s

    def get_c_function_prefix(self, function: DumpedFunction, name: Optional[str]) -> str:
        if name and "_do_not_use" in name:
            name = None
        original_function_address = self.address_db.get_function_address(name or function.name)
        function_callconv = self.address_db.get_function_call_convention(name or function.name)

        s = ""
        if function.name in self.forbidden_functions:
            s += "#if 0\n"
        s += f"function_hook_state_t function_hook_state_{name or function.name} = HOOK_UNAVAILABLE;\n"
        if function.name not in self.forbidden_functions:
            s += f"CARM95_WEBSERVER_STATE(function_hook_state_{name or function.name})\n"
        if original_function_address:

            if function.is_vararg() or not self.address_db.is_function_okay_to_hook(name or function.name):
                name = function.name
                function_type_str = self.formatter.type_to_str(function.function_type, callconv=function_callconv)
                original_symbol_name = name
                original_symbol_decl = self.formatter.type_name_to_str(function.function_type, original_symbol_name, callconv=function_callconv)

                s += original_symbol_decl + f" = ({function_type_str})0x{original_function_address:08x};\n"
            else:
                name = name or function.name
                function_type_str = self.formatter.type_to_str(function.function_type, callconv=function_callconv)
                original_symbol_name = self._get_original_symbol_name(function, name)
                original_symbol_decl = self.formatter.type_name_to_str(function.function_type, original_symbol_name, callconv=function_callconv)

                s += "static " + original_symbol_decl + f" = ({function_type_str})0x{original_function_address:08x};\n"
                s += f"CARM95_HOOK_FUNCTION({original_symbol_name}, {name})\n"

        return s

    def get_c_function_suffix(self, function: DumpedFunction, name: Optional[str]) -> str:
        if name and "_do_not_use" in name:
            name = None
        s = ""
        if function.name in self.forbidden_functions:
            s += "#endif\n"
        return s

    def get_h_global_variable_extern_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        if name and "_do_not_use" in name:
            name = None
        s = ""
        address = self.address_db.get_data_address(name or gv.name)
        var_s = ""
        if not address:
            var_s += "// "
        var_s += f"extern {self.formatter.pointer_type_name_to_str(gv.kind, 'hookvar_' + (name or gv.name))};"
        if address:
            var_s += f" // addr: {address:08X}"
        if "struct" not in var_s and "{" not in var_s:
            s += var_s
            if name:
                s += " // suffix added to avoid duplicate symbol"
            s += "\n"
        return s

    def get_c_global_variable_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        s = ""
        address = self.address_db.get_data_address(name or gv.name)
        if name:
            s += " // Suffix added to avoid duplicate symbol\n"
        if not address:
            s += "#if 0\n"
        s += f"{self.formatter.pointer_type_name_to_str(gv.kind, 'hookvar_' + (name or gv.name))}"
        if address:
            s += f" = (void*)0x{address:08x}"
        s += ";\n"
        if not address:
            s += "#endif\n"
        return s

    def get_h_function_prefix(self, function: DumpedFunction, name: Optional[str]) -> str:
        original_function_address = self.address_db.get_function_address(function.name)

        s = ""
        if function.name in self.forbidden_functions:
            s += "#if 0\n"
        if original_function_address:
            if function.is_vararg() or not self.address_db.is_function_okay_to_hook(name or function.name):
                function_callconv = self.address_db.get_function_call_convention(function.name)
                if function.is_vararg():
                    original_symbol_decl = self.formatter.type_name_to_str(function.function_type, function.name, callconv=function_callconv)

                    s += "extern " + original_symbol_decl + f";\n"
        if name and "_do_not_use" in name:
            s += "#if 0\n"
        return s

    def get_h_function_suffix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        s = ""
        if name and "_do_not_use" in name:
            s += "#endif\n"
        if fn.name in self.forbidden_functions:
            s += "#endif\n"
        return s

    def get_function_body(self, function: DumpedFunction, name: Optional[str], depth: int) -> str:
        prefix = " " * self.indentation * depth
        original_function_address = self.address_db.get_function_address(name or function.name)

        s = ""
        s = prefix + self._function_to_logtrace(function) + ";\n"
        s += "\n"

        for arg in function.arguments():
            s += prefix + "(void)" + arg.name + ";\n"
        for arg in function.local_variables():
            s += prefix + "(void)" + arg.name + ";\n"
        for block_i, block in enumerate(function.block_scopes()):
            for bv in block:
                s += prefix + f"(void)__block{block_i}__" + bv.name + ";\n"

        s += "\n"
        fn_name = name.removesuffix("_do_not_use") if name and "_do_not_use" in name else (name or function.name)
        var_name = f"function_hook_state_{fn_name}"
        s += prefix + f"if ({var_name} == HOOK_ENABLED) {{\n"
        s += prefix + f"    assert(0 && \"{name or function.name} not implemented.\");\n"
        s += prefix + f"    abort();\n"
        s += prefix + f"}} else {{\n"

        if original_function_address is not None and not function.is_vararg():
            name = name or function.name
            original_symbol_name = self._get_original_symbol_name(function, name)
            retstr = ""
            if type(function.return_type()) != DumpedVoidType:
                retstr = "return "
            s += "    " + prefix + retstr + f"{original_symbol_name}(" + ", ".join(arg.name for arg in function.arguments()) + ");\n"
        else:
            s += "    " + prefix + "NOT_IMPLEMENTED();\n"

        s += prefix + f"}}\n"
        return s

    def modify_symbol_name(self, name: str, module: DumpedModule) -> Optional[str]:
        for fn in module.iter_functions():
            if fn.name == name:
                if fn.is_vararg() or not self.address_db.is_function_okay_to_hook(name or fn.name):
                    return name + "_do_not_use"
                break
        return super().modify_symbol_name(name, module)

    def _generate_hook_sources_cmake(self):
        sources = []
        for module in self.modules:
            if "BRSRC" in module.name or "DETHRACE" in module.name or "pc-dos" in module.name:
                sources.extend((
                    str(Path(self.module_to_filename(module, ".c")).relative_to(self.basepath)).replace("\\", "/"),
                    str(Path(self.module_to_filename(module, ".h")).relative_to(self.basepath)).replace("\\", "/"),
                ))
        with (self.basepath / "hook_sources.cmake").open("w") as f:
            f.write(textwrap.dedent(r"""
                set(HOOK_SOURCES
            """))
            for source in sources:
                f.write(f"    {source}\n")
            f.write(textwrap.dedent(r"""
                )
            """))

    def generate(self):
        mkdir_p(self.basepath)
        mkdir_p(self.basepath / "cmake")
        mkdir_p(self.basepath / "include/brender")
        mkdir_p(self.basepath / "include/harness")
        mkdir_p(self.basepath / "include/s3")
        mkdir_p(self.basepath / "tools")
        with (self.basepath / "carm95_dummy.c").open("w") as f:
            f.write("int OS_IsDebuggerPresent(void) { return 1; }\n")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/harness/include/harness/trace.h", self.basepath / "include/harness")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/harness/harness_trace.c", self.basepath)
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/brender.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_defs.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_inline_funcs.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/BRSRC13/include/brender/br_types.h", self.basepath / "include/brender")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/constants.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/dr_types.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/DETHRACE/macros.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parents[2] / "src/S3/include/s3/s3.h", self.basepath / "include/s3")
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/CMakeLists.txt", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/cmake/FindDetours.cmake", self.basepath / "cmake")
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/conanfile.py", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/injector_main.cpp", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/injector_hasher.c", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/injector.h", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/carm95_hooks.cpp", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/include/carm95_hooks.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/carm95_main.cpp", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/carm95_webserver.cpp", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/include/carm95_webserver.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/carm95_libc.c", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/include/carm95_libc.h", self.basepath / "include")
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/carm95_s3sound.c", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/hook_mingw.def", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/hook_msvc.def", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/hookclient.py", self.basepath)
        shutil.copy(Path(__file__).resolve().parent / "injectorgen-resources/tools/wrap-global-vars.py", self.basepath / "tools")
        self._generate()
        self._generate_hook_sources_cmake()
        for element in self.basepath.iterdir():
            if element.is_dir():
                if element.name not in ("include", "BRSRC13", "DETHRACE", "tools", "cmake"):
                    shutil.rmtree(element)


class JSONAddressDatabase(AddressDatabase):
    def __init__(self, symbols):
        self.symbols = symbols


    def get_function_call_convention(self, name: str) -> Optional[str]:
        for item in self.symbols["functions"]:
            if item["name"] == name:
                return item["conv"]
        return None

    def get_function_address(self, name: str) -> Optional[int]:
        for item in self.symbols["functions"]:
            if item["name"] == name:
                return item["offset"]
        return None

    def is_function_okay_to_hook(self, name) -> bool:
        for item in self.symbols["functions"]:
            if item["name"] == name:
                return item["size"] >= 5
        return True

    def get_data_address(self, name: str) -> Optional[int]:
        for item in self.symbols["datas"]:
            if item["label"] == name:
                return item["offset"]
        return None

    @classmethod
    def from_exported_json(cls, path: Path) -> "GhidraAddressFile":
        with open(path, "r") as f:
            data = json.load(f)
        return cls(data)



class IDAAddressDatabase(AddressDatabase):
    def __init__(self, func_map: dict[str, int], gv_map: dict[str, int]):
        self.func_map = func_map
        self.gv_map = gv_map

    def get_function_address(self, name: str) -> Optional[int]:
        return self.func_map.get(name)

    def get_data_address(self, name: str) -> Optional[int]:
        return self.gv_map.get(name)

    def is_function_okay_to_hook(self):
        return True

    @classmethod
    def from_exported_map(cls, path: Path) -> "IDAAddressFile":
        func_offset = 0x00401000
        gv_offset = 0x00507000

        func_map = {}
        gv_map = {}
        with path.open() as f:
            while True:
                line = f.readline()
                if line == "":
                    break

                line = line.strip()
                if line.startswith("0001:"):
                    parts = [s for s in line[5:].split(" ") if s]
                    # map function name to virtual address
                    func_map[parts[1]] = int(parts[0], 16) + func_offset
                elif line.startswith("0003:"):
                    parts = [s for s in line[5:].split(" ") if s]

                    # skip over most autogenerated stuff
                    if parts[1].startswith("g"):
                        # map global variable name to virtual address
                        gv_map[parts[1]] = int(parts[0], 16) + gv_offset
        return cls(func_map, gv_map)


def main():
    import argparse
    import sys

    logging.basicConfig(format="%(levelname)s: %(message)s")

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("dumpfile", type=Path, help="Path to output of wdump")
    parser.add_argument("addressfile", type=Path, help="Path to file containing symbol addresses")
    parser.add_argument("base", default=Path("_generated"), type=Path, nargs="?", help="Path where to generate the symbols")
    parser.add_argument("--type", choices=("json", "ida"), default="ida", help="Symbol type file")
    parser.add_argument("--indent", default=4, type=int, help="Indentation of generated code")
    parser.add_argument("--no-clear", dest="clear", action="store_false", help="Don't remove base folder")
    parser.add_argument("-v", dest="verbose", action="store_true", help="Verbose")
    args = parser.parse_args()

    if args.verbose:
        logging.root.setLevel(logging.INFO)

    args.base = args.base.resolve()

    modules = list(DumpedModuleReader.open(args.dumpfile).read_modules())

    if args.type == "json":
        address_db = JSONAddressDatabase.from_exported_json(args.addressfile)
    elif args.type == "ida":
        address_db = IDAAddressDatabase.from_exported_map(args.addressfile)
    else:
        raise NotImplementedError

    formatter = CFormatter(
        indentation=args.indent,
    )

    project_generator = InjectorGenerator(
        address_db=address_db,
        modules=modules,
        basepath=args.base,
        indentation=args.indent,
        formatter=formatter,
    )

    if args.clear:
        project_generator.clear()

    project_generator.generate()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
