#!/usr/bin/env python
import enum
import itertools
import logging
import os
from pathlib import Path
import shutil
import textwrap

from dumpparser import *


logger = logging.getLogger(__name__)


class CFormatter:
    def __init__(self, indentation=4):
        self.indentation = indentation

    def variable_to_str(self, var: DumpedTypedVariable, name: Optional[str]=None, depth=0, east=True) -> str:
        return self.type_name_to_str(var.kind, var.name if name is None else name, depth=depth, east=east, variable=True)

    def type_to_str(self, dumped_type: DumpedType, depth=0, east=True, variable=False, callconv=None) -> str:
        s = self.type_name_to_str(dumped_type, None, depth=depth, east=east, variable=variable, callconv=callconv)
        return s

    def type_name_to_str(self, dumped_type: DumpedType, name: Optional[str], depth=0, east=True, variable=False, callconv=None) -> str:
        if type(dumped_type) == DumpedTypedefType:
            prefix, suffix = dumped_type.name, ""
        else:
            prefix, suffix = self.type_to_prefix_suffix(dumped_type, depth=depth, east=east, variable=variable, callconv=callconv)
        if (prefix, suffix) == ("void", ""):
            return prefix
        if name:
            if prefix[-1:] == "*":
                if not east:
                    prefix += " "
            else:
                prefix += " "
            return prefix + name + suffix
        else:
            return prefix + suffix

    def type_to_prefix_suffix(self, dumped_type: DumpedType, depth=0, east=True, variable=False, callconv=None) -> tuple[str, str]:
        if type(dumped_type) == DumpedPointerType:
            return self.pointer_type_name_to_prefix_suffix(dumped_type.ptr, depth=depth, east=east, variable=variable, callconv=callconv)
        elif type(dumped_type) == DumpedVoidType:
            return "void", ""
        elif type(dumped_type) in (DumpedNamedType, DumpedTypedefType):
            prefix, suffix = dumped_type.name, ""
            if type(dumped_type) == DumpedTypedefType and type(dumped_type.subtype()) == DumpedPointerType and type(dumped_type.subtype().ptr) == DumpedFunctionType:
                prefix += "*"
            return prefix, suffix
        elif type(dumped_type) == DumpedArrayType:
            prefix, suffix = self.type_to_prefix_suffix(dumped_type.ptr, depth=depth, east=east, variable=variable)
            return prefix, f"[{dumped_type.count}]" + suffix
        elif type(dumped_type) == DumpedFunctionType:
            args = [self.type_to_prefix_suffix(a, depth=depth, east=east) for a in dumped_type.arguments()]
            args = [prefix + suffix for prefix, suffix in args]
            args_str = ", ".join(args)
            callconv = callconv or ""
            prefix = "".join(self.type_to_str(dumped_type.return_type(), depth=depth, east=east, variable=variable)) + "("+callconv+"*"
            suffix = f")({args_str})"
            return prefix, suffix
        elif type(dumped_type) == DumpedStructType:
            if variable:
                if dumped_type.tag_name is not None:
                    return f"struct {dumped_type.tag_name}", ""
            prefix  = "struct " + (dumped_type.tag_name + " " if dumped_type.tag_name else "") + "{\t\t" + f"// size: 0x{dumped_type.size:x}\n"
            member_strs = []
            for member in dumped_type.members:
                bit_str = ""
                if member.bit_size is not None:
                    bit_str = f" : {member.bit_size}"
                member_strs.append((" " * (self.indentation * (depth + 1))) + self.type_name_to_str(member.kind, member.name, depth=depth + 1, east=east, variable=variable) + bit_str + f";\t\t// @0x{member.offset:x}")
            prefix += "\n".join(member_strs)
            prefix += "\n" + " " * (self.indentation * depth) + "}"
            suffix = ""
            return prefix, suffix
        elif type(dumped_type) == DumpedUnionType:
            if variable:
                if dumped_type.tag_name is not None:
                    return f"union {dumped_type.tag_name}", ""
            prefix  = "union " + (dumped_type.tag_name + " " if dumped_type.tag_name else "") + "{\t\t" + f"// size: 0x{dumped_type.size:x}\n"
            member_strs = []
            for member in dumped_type.members:
                member_strs.append((" " * (self.indentation * (depth + 1))) + self.type_name_to_str(member.kind, member.name, depth=depth + 1, east=east, variable=variable) + f";\t\t// @0x{member.offset:x}")
            prefix += "\n".join(member_strs)
            prefix += "\n" + " " * (self.indentation * depth) + "}"
            suffix = ""
            return prefix, suffix
        elif type(dumped_type) == DumpedEnumType:
            if variable:
                if dumped_type.tag_name is not None:
                    return f"enum {dumped_type.tag_name}", ""
            prefix  = "enum " + (dumped_type.tag_name + " " if dumped_type.tag_name else "") + "{\n"
            prefix += ",\n".join(f"{' ' * (self.indentation * (depth + 1))}{k} = {v}" for k, v in dumped_type.members)
            prefix += "\n}"
            suffix = ""
            return prefix, suffix
        else:
            raise NotImplementedError(type(dumped_type))

    def function_to_str(self, fn: DumpedFunction, name: Optional[str]=None, depth=0, callconv: Optional[str]=None, east=True, ret_east=False) -> str:
        ret_str = self.type_to_str(fn.return_type(), east=ret_east)
        if not (ret_str[-1:] == "*" and ret_east):
            ret_str += " "
        if callconv is None:
            callconv = ""
        else:
            callconv += " "
        args_str_elems = [self.type_name_to_str(arg.kind, arg.name, depth=depth+1, east=east, variable=True) for arg in fn.arguments()]
        name = name if name else fn.name
        fn_str = f"{ret_str}{callconv}{name}({', '.join(args_str_elems)})"
        return fn_str

    def vars_to_format_str(self, vars: list[DumpedTypedVariable], prepend="") -> tuple[str, list[str]]:
        format_strs = []
        args = []
        for var in vars:
            format_names = self.typed_name_to_format_str(var.kind, var.name)
            if format_names is None:
                continue
            format_str, names = format_names
            format_strs.append(format_str)
            args.extend([prepend + name for name in names])
        return ", ".join(format_strs), args

    def typed_name_to_format_str(self, t: DumpedType, name: str) -> Optional[tuple[str, list[str]]]:
        if type(t) in (DumpedPointerType, DumpedArrayType):
            if type(t.ptr) == DumpedNamedType and t.ptr.name == "char":
                return "\\\"%s\\\"", [name]
            else:
                return "%p", [name]
        elif type(t) == DumpedEnumType:
            return "%d", [name]
        elif type(t) == DumpedTypedefType:
            return self.typed_name_to_format_str(t.subtype(), name)
        elif type(t) == DumpedStructType:
            format, vars = self.vars_to_format_str(t.members, prepend=f"{name}.")
            return "{" + format + "}", vars
        elif type(t) == DumpedNamedType:
            if t.name in ("char", ):
                return "'%c'", [name]
            elif t.name in ("long", "int", "short", "signed char", ):
                return "%d", [name]
            elif t.name in ("unsigned long", "unsigned int", "unsigned short", "unsigned char", ):
                return "%u", [name]
            elif t.name in ("float", "double", ):#"br_float", "br_scalar", ):
                return "%f", [name]
            else:
                raise ValueError(f"Unknown named type: {t.name}")
        elif type(t) == DumpedUnionType:
            # Ignore dumped
            return None
        else:
            try:
                raise ValueError(f"Unknown DumpedType: {type(t)} -> {DumpedNamedType}")
            except ValueError:
                raise

    def pointer_type_name_to_prefix_suffix(self, dumped_type_ptr: DumpedType, depth=0, east=True, variable=False, callconv=None) -> str:
        if type(dumped_type_ptr) == DumpedStructType:
            if dumped_type_ptr.tag_name is None:
                prefix, suffix =  self.type_to_prefix_suffix(dumped_type_ptr, depth=depth, east=east)
            else:
                prefix = f"struct {dumped_type_ptr.tag_name}"
                suffix = ""
        elif type(dumped_type_ptr) == DumpedUnionType:
            if dumped_type_ptr.tag_name is None:
                prefix, suffix =  self.type_to_prefix_suffix(dumped_type_ptr, depth=depth, east=east)
            else:
                prefix = f"union {dumped_type_ptr.tag_name}"
                suffix = ""
        elif type(dumped_type_ptr) == DumpedArrayType:
            prefix, suffix = self.type_to_prefix_suffix(dumped_type_ptr, depth=depth, east=east, variable=variable)
            prefix = prefix + "(*"
            suffix = ")" + suffix
            return prefix, suffix
        else:
            prefix, suffix = self.type_to_prefix_suffix(dumped_type_ptr, depth=depth, east=east, variable=variable, callconv=callconv)
        if prefix[-1:] != "*":
            if east:
                prefix += " "
        return (prefix + "*"), ("" + suffix)

    def pointer_type_name_to_str(self, dumped_type_ptr: DumpedType, name: str, depth=0, east=True) -> str:
        prefix, suffix = self.pointer_type_name_to_prefix_suffix(dumped_type_ptr, depth=depth, east=east, variable=True)
        return prefix + " " + name + " " + suffix


def find_duplicate_symbol_names(modules: list[DumpedModule]) -> list[str]:
    symbols_found = set()
    duplicate_symbols = set()

    for module in modules:
        for glob in module.iter_globals():
            if glob.name in symbols_found:
                duplicate_symbols.add(glob.name)
            symbols_found.add(glob.name)
        for fn in module.iter_functions():
            if fn.name in symbols_found:
                duplicate_symbols.add(fn.name)
            symbols_found.add(fn.name)
    return list(duplicate_symbols)


class AddressDatabase:
    def get_function_address(self, name: str) -> Optional[int]:
        raise NotImplementedError

    def get_function_call_convention(self, name: str) -> Optional[str]:
        raise NotImplementedError

    def get_data_address(self, name: str) -> Optional[int]:
        raise NotImplementedError


class DumpedAddressDatabase(AddressDatabase):
    def __init__(self, functions, datas):
        self.functions = functions
        self.datas = datas

    def get_function_address(self, name: str) -> Optional[int]:
        for item in self.functions:
            if item["name"] == name:
                return item["offset"]
        return None

    def get_function_call_convention(self, name: str) -> Optional[str]:
        for item in self.functions:
            if item["name"] == name:
                return item["conv"]
        return None

    def get_data_address(self, name: str) -> Optional[int]:
        for item in self.datas:
            if item["name"] == name:
                return item["offset"]
        return None

    @classmethod
    def create_from_dumped_modules(cls, modules: list[DumpedModule]) -> "DumpedAddressDatabase":
        duplicate_symbols = find_duplicate_symbol_names(modules)
        all_functions = []
        all_globals = []
        for module in modules:
            for fn in module.iter_functions():
                fn: DumpedFunction
                name = fn.name
                if name in duplicate_symbols:
                    clean_module_name = os.path.splitext(module.name.rsplit("\\",1)[-1])[0]
                    name = f"{name}__{clean_module_name}"
                all_functions.append({
                   "name": name,
                   "offset": fn.raw_data["offset"],
                   "conv": "__usercall" if fn.raw_data["args"] else "__cdecl",
                })
            for gv in module.iter_globals():
                gv: DumpedGlobalVariable
                name = gv.name
                if name in duplicate_symbols:
                    name = f"{name}_{module.name}"
                all_globals.append({
                    "name": name,
                    "offset": gv.address,
                })
        return cls(all_functions, all_globals)


def mkdir_p(path):
    try:
        os.makedirs(path)
    except FileExistsError:
        pass


class DethRaceProjectGenerator:
    def __init__(self, modules: list[DumpedModule], basepath: Path, indentation: int, formatter: CFormatter, address_db: AddressDatabase):
        self.modules = modules
        self.basepath = basepath
        self.indentation = indentation
        self.formatter = formatter
        self.address_db = address_db
        self.br_types_file = None
        self.dr_types_file = None
        self.duplicate_symbol_names = find_duplicate_symbol_names(self.modules)
        logger.info("Duplicate symbols: %s", self.duplicate_symbol_names)

    def clear(self):
        shutil.rmtree(self.basepath, ignore_errors=True)

    def _generate(self):
        mkdir_p(os.path.join(self.basepath, "types"))

        self.dr_types_file = open(os.path.join(self.basepath, "types/dr_types.h"), "w")
        self.br_types_file = open(os.path.join(self.basepath, "types/br_types.h"), "w")

        self.dr_types_file.write(textwrap.dedent("""\
            #ifndef DR_TYPES_H
            #define DR_TYPES_H
            
            #include "br_types.h"
            
            """))

        self.br_types_file.write(textwrap.dedent("""\
            #ifndef BR_TYPES_H
            #define BR_TYPES_H
            
            """))

        all_data = {}

        for m in self.modules:
            # ignore lib modules
            if "\\" not in m.name:
                logger.info("ignoring module '%s'", m.name)
                continue
            logger.debug("Generating c/h file for %s", m.name)
            self._generate_h_file(m)
            self._generate_c_file(m)
            self._generate_types_header(m, all_data)

        self.br_types_file.write("\n#endif")
        self.dr_types_file.write("\n#endif")

        self.br_types_file.close()
        self.dr_types_file.close()

        self.br_types_file = None
        self.dr_types_file = None

    def _function_to_logtrace(self, fn: DumpedFunction) -> str:
        do_comma = False
        format_string, arg_strings = self.formatter.vars_to_format_str(fn.arguments())
        logtrace_args = [f"\"({format_string})\""]
        logtrace_args.extend(arg_strings)
        return f"LOG_TRACE({', '.join(logtrace_args)})"

    def module_to_filename(self, module: DumpedModule, new_suffix: str) -> Path:
        return (self.basepath / module.name[3:].replace("\\", "/")).with_suffix(new_suffix)

    def _generate_h_file(self, module: DumpedModule):
        filePath = self.module_to_filename(module, ".h")
        logger.debug("Writing header for module '%s' to '%s'", module.name, filePath)
        mkdir_p(os.path.dirname(filePath))
        name = os.path.basename(filePath)
        guard_name = f"_{name.upper().replace('.', '_')}_"

        with filePath.open("w") as h_file:
            h_file.write(f"#ifndef {guard_name}\n")
            h_file.write(f"#define {guard_name}\n\n")

            h_file.write(self.get_header_extra(module))
            h_file.write("\n")

            # global variables
            for gv in sorted(module.iter_globals(), key=lambda m: m.address):
                logger.debug("Global variable %s (module=%s)", gv.name, module.name)
                # ignore this specific field
                if gv.name in ("rscid", ):
                    logger.debug("Ignoring global variable %s", gv.name)
                    continue

                unique_name = self.modify_symbol_name(gv.name, module)
                h_file.write(self.get_h_global_variable_extern_declaration(gv, unique_name))
            h_file.write("\n")

            for fn in module.iter_functions():
                unique_name = self.modify_symbol_name(fn.name, module)
                h_file.write(self.get_h_function_prefix(fn, unique_name))
                h_file.write(self.formatter.function_to_str(fn, name=unique_name or fn.name, callconv=self.address_db.get_function_call_convention(unique_name or fn.name)))
                h_file.write(";\n")
                h_file.write(self.get_h_function_suffix(fn, unique_name))
                h_file.write("\n")
            h_file.write("#endif\n")

    def get_h_global_variable_extern_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        s = ""
        var_s = f"extern {self.formatter.variable_to_str(gv, name=name)}; // addr: {gv.address:08X}"
        if "struct" not in var_s and "{" not in var_s:
            s += var_s
            if name:
                s += " // suffix added to avoid duplicate symbol"
            s += "\n"
        return s

    def get_c_global_variable_declaration(self, gv: DumpedGlobalVariable, name: str) -> str:
        s = ""
        if name:
            s += " // Suffix added to avoid duplicate symbol\n"
        s += f"{self.formatter.variable_to_str(gv, name=name)};\n"
        return s

    def _generate_c_file(self, module: DumpedModule):
        filePath = self.module_to_filename(module, ".c")
        logger.debug("Writing .c file for module '%s' to '%s'", module.name, filePath)

        with filePath.open("w") as c_file:
            c_file.write(f"#include \"{filePath.with_suffix('.h').name}\"\n\n")

            c_file.write(self.get_source_extra(module))

            # global variables
            for gv in sorted(module.iter_globals(), key=lambda m: m.address):

                # ignore this specific field
                if gv.name in ("rscid", ):
                    continue
                unique_name = self.modify_symbol_name(gv.name, module)
                c_file.write(self.get_c_global_variable_declaration(gv, unique_name))
            c_file.write("\n")

            # functions
            for fn in module.iter_functions():
                unique_name = self.modify_symbol_name(fn.name, module)
                c_file.write(self.get_c_function_prefix(fn, unique_name))

                c_file.write(self.formatter.function_to_str(fn, name=unique_name, callconv=self.address_db.get_function_call_convention(unique_name or fn.name)))
                c_file.write(" {\n")

                for lv in fn.local_variables():
                    c_file.write(" " * self.indentation)
                    if lv.static:
                        c_file.write("static ")
                    c_file.write(self.formatter.variable_to_str(lv, depth=1))
                    c_file.write(";\n")

                for block_i, block in enumerate(fn.block_scopes()):
                    for bv in block:
                        c_file.write(" " * self.indentation)
                        if bv.static:
                            c_file.write("static ")
                        name = f"__block{block_i}__" + bv.name
                        c_file.write(self.formatter.variable_to_str(bv, name=name))
                        c_file.write(";\n")

                c_file.write(self.get_function_body(fn, unique_name, depth=1));
                c_file.write("}\n")
                c_file.write(self.get_c_function_suffix(fn, unique_name))
                c_file.write("\n")

    def _generate_types_header(self, module: DumpedModule, all_data):
        new_br_items = []
        new_dr_items = []

        for type_idx in module.raw_data["types"]:
            t = module.create_dumped_type(type_idx)

            if type(t) == DumpedTypedefType:
                subtype = t.subtype()
                if type(subtype) in (DumpedStructType, DumpedEnumType, DumpedUnionType) and subtype.tag_name is not None:
                    if type(subtype) == DumpedStructType:
                        s = f"typedef struct {subtype.tag_name} {t.name}"
                    elif type(subtype) == DumpedEnumType:
                        s = f"typedef struct {subtype.tag_name} {t.name}"
                    elif type(subtype) == DumpedUnionType:
                        s = f"typedef struct {subtype.tag_name} {t.name}"
                else:
                    s = "typedef " + self.formatter.type_name_to_str(subtype, t.name)
                if t.name.startswith("br") or type(t.subtype) in (DumpedNamedType, DumpedPointerType, DumpedTypedefType):
                    new_br_items.append(s)
                else:
                    new_dr_items.append(s)
            elif type(t) == DumpedStructType:
                if not t.members:
                    continue
                if t.tag_name:
                    s = self.formatter.type_to_str(t)
                    if "br_" in t.tag_name:
                        new_br_items.append(s)
                    else:
                        new_dr_items.append(s)
            elif type(t) ==  DumpedUnionType:
                if not t.members:
                    continue
                if t.tag_name:
                    s = self.formatter.type_to_str(t)
                    if "br_" in t.tag_name:
                        new_br_items.append(s)
                    else:
                        new_dr_items.append(s)
            elif type(t) == DumpedEnumType:
                if t.tag_name:
                    s = self.formatter.type_to_str(t)
                    if "br_" in t.tag_name:
                        new_br_items.append(s)
                    else:
                        new_dr_items.append(s)
            else:
                pass

        for br_item in new_br_items:
            if br_item not in all_data.setdefault("br_items", []):
                all_data["br_items"].append(br_item)
                self.br_types_file.write(br_item)
                self.br_types_file.write("\n")

        for dr_item in new_dr_items:
            if dr_item not in all_data.setdefault("dr_items", []):
                all_data["dr_items"].append(dr_item)
                self.dr_types_file.write(dr_item)
                self.dr_types_file.write("\n")

    def modify_symbol_name(self, name: str, module: DumpedModule) -> Optional[str]:
        if name in self.duplicate_symbol_names:
            stem_name = module.name[module.name.rindex("\\")+1:-2]
            return f"{name}__{stem_name}"
        return None

    def _function_to_ida_str(self, fn: DumpedFunction, name: Optional[str]=None, east=True, ret_east=False) -> str:
        ret_str = self.formatter.type_to_str(fn.return_type(), east=ret_east)
        if not (ret_str[-1:] == "*" and ret_east):
            ret_str += " "
        args_str_elems = []
        return_reg = ""
        if fn.raw_data["args"] and fn.raw_data.get("return_value", None) is not None:
            return_reg = f"@<{fn.raw_data['return_value']}>"
        for arg, raw_arg in itertools.zip_longest(fn.arguments(), fn.raw_data["args"]):
            if raw_arg is None:
                arg_name = arg.name
            else:
                arg_name = f"{arg.name}@<{raw_arg}>"
            args_str_elems.append(self.formatter.type_name_to_str(arg.kind, arg_name, east=east))
        if fn.is_vararg():
            args_str_elems.append("...")
        name = name if name else fn.name
        callconv = self.address_db.get_function_call_convention(name)
        fn_str = f"{ret_str}{callconv} {name}{return_reg}({', '.join(args_str_elems)})"
        return fn_str

    def _get_function_prefix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        text = ""
        text += f"// Offset: {self.address_db.get_function_address(name or fn.name):d}\n"
        text += f"// Size: 0x{fn.raw_data['size']:x}\n"
        text += f"//IDA: {self._function_to_ida_str(fn)}\n"
        if name:
            text +="// Suffix added to avoid duplicate symbol\n"
        return text

    def generate(self):
        self._generate()

    def get_function_call_convention(self, function: DumpedFunction, unique_name: Optional[str]) -> Optional[str]:
        return None

    def get_function_body(self, function: DumpedFunction, name: Optional[str], depth: int) -> str:
        prefix = " " * self.indentation * depth

        s  = prefix + self._function_to_logtrace(function) + ";\n"
        s += "\n"
        s += prefix + "NOT_IMPLEMENTED();\n"
        return s

    def get_header_extra(self, module: DumpedModule) -> str:
        s = ""
        if "BRSRC" in module.name or "DETHRACE" in module.name:
            s += "#include \"br_types.h\"\n"
        if "DETHRACE" in module.name:
            s += "#include \"dr_types.h\"\n"
        return s

    def get_source_extra(self, module: DumpedModule) -> str:
        s = "#include \"harness/trace.h\"\n\n"
        return s

    def get_c_function_prefix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        return self._get_function_prefix(fn, name)

    def get_c_function_suffix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        return ""

    def get_h_function_prefix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        return self._get_function_prefix(fn, name)

    def get_h_function_suffix(self, fn: DumpedFunction, name: Optional[str]) -> str:
        return ""

def main():
    import argparse
    import sys

    logging.basicConfig(format="%(levelname)s: %(message)s")

    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("dumpfile", type=Path, help="Path to output of wdump")
    parser.add_argument("base", default=Path("_generated"), type=Path, nargs="?", help="Path where to generate the symbols")
    parser.add_argument("--indent", default=4, type=int, help="Indentation of generated code")
    parser.add_argument("--no-clear", dest="clear", action="store_false", help="Don't remove base folder")
    parser.add_argument("-v", dest="verbose", action="store_true", help="Verbose")
    args = parser.parse_args()

    if args.verbose:
        logging.root.setLevel(logging.INFO)

    args.base = args.base.resolve()

    formatter = CFormatter(
        indentation=args.indent,
    )

    modules = list(DumpedModuleReader.open(args.dumpfile).read_modules())

    address_db = DumpedAddressDatabase.create_from_dumped_modules(modules)

    project_generator = DethRaceProjectGenerator(
        modules=modules,
        basepath=args.base,
        indentation=args.indent,
        formatter=formatter,
        address_db=address_db,
    )

    if args.clear:
        project_generator.clear()

    project_generator.generate()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
